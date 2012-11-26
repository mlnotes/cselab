// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
 
  VERIFY(pthread_mutex_init(&lock_mutex, NULL) == 0);
  VERIFY(pthread_mutex_init(&revoke_mutex, NULL) == 0);
}

lock_protocol::status 
lock_client_cache::wait_for_lock(lock_protocol::lockid_t lid){
  while(locks[lid].state != lock_client_info::NONE && 
         locks[lid].state != lock_client_info::FREE)
    pthread_cond_wait(&(locks[lid].cond), &lock_mutex);
        
   if(locks[lid].state == lock_client_info::FREE){
     locks[lid].state = lock_client_info::LOCKED;
     pthread_mutex_unlock(&lock_mutex);
     return lock_protocol::OK;
   }else if(locks[lid].state == lock_client_info::NONE){
     locks[lid].state = lock_client_info::ACQUIRING;
     pthread_mutex_unlock(&lock_mutex);
     return acquire_from_server(lid); 
   }
   return lock_protocol::OK;
}


lock_protocol::status 
lock_client_cache::acquire_from_server(lock_protocol::lockid_t lid){
  lock_protocol::status ret = lock_protocol::RETRY; 
  while(ret == lock_protocol::RETRY){
    pthread_mutex_lock(&lock_mutex);
    tprintf("acquire from server: lid=>%llu tid=>%lu id=>%s\n", lid, pthread_self(), id.c_str());
    if(locks[lid].state == lock_client_info::LOCKED){
      pthread_mutex_unlock(&lock_mutex);
      return lock_protocol::OK;
    }else{
      pthread_mutex_unlock(&lock_mutex);
    }
    
    int r;
    ret = cl->call(lock_protocol::acquire, lid, id, r);
  }
  
  //cache this lock
  pthread_mutex_lock(&lock_mutex);
  locks[lid].state = lock_client_info::LOCKED;
  pthread_mutex_unlock(&lock_mutex);
  return ret;     
}


lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  tprintf("[ACQUIRE]: lid=>%llu id=>%s\n", lid, this->id.c_str());
  pthread_mutex_lock(&lock_mutex);
  if(locks.find(lid) != locks.end() && 
      locks[lid].state != lock_client_info::NONE){
    tprintf("lock cached\n");
    switch(locks[lid].state){
      case lock_client_info::FREE:
        locks[lid].state = lock_client_info::LOCKED;  
        pthread_mutex_unlock(&lock_mutex);
        return lock_protocol::OK;
      case lock_client_info::LOCKED:
        return wait_for_lock(lid);
      case lock_client_info::ACQUIRING:
        return wait_for_lock(lid);
      case lock_client_info::RELEASING:
        return wait_for_lock(lid);
    } 
  }else{
    tprintf("lock not cached\n");
    // acqure this lock via rpc 
    locks[lid].state = lock_client_info::ACQUIRING;
    pthread_mutex_unlock(&lock_mutex);

    return acquire_from_server(lid); 
  } 
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  tprintf("[RELEASE]: lid=>%llu id=>%s\n", lid, this->id.c_str());
  pthread_mutex_lock(&lock_mutex);
  if(locks.find(lid) != locks.end()){
    if(revokes.find(lid) != revokes.end()){ // release to the server
      tprintf("[RELEASE TO SERVER]: lid=>%llu id=>%s\n", lid, this->id.c_str());
      locks[lid].state = lock_client_info::RELEASING;
      pthread_mutex_unlock(&lock_mutex);
    
      int r;
      cl->call(lock_protocol::release, lid, id, r);
      
      pthread_mutex_lock(&revoke_mutex);       
      revokes.erase(lid);
      pthread_mutex_unlock(&revoke_mutex);

      pthread_mutex_lock(&lock_mutex);
      locks[lid].state = lock_client_info::NONE;
      pthread_mutex_unlock(&lock_mutex);
    }else{
      locks[lid].state = lock_client_info::FREE;
      pthread_mutex_unlock(&lock_mutex);
      tprintf("[RELEASE TO CLIENT]: lid=>%llu id=>%s\n", lid, this->id.c_str());
    }
    // inform waiting threads
    pthread_cond_signal(&(locks[lid].cond)); 
  }
  return lock_protocol::OK;

}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  tprintf("[REVOKE]: lid=>%llu\n", lid);

  pthread_mutex_lock(&lock_mutex);
  if(locks[lid].state == lock_client_info::FREE){
    locks[lid].state = lock_client_info::RELEASING;
    pthread_mutex_unlock(&lock_mutex);
    tprintf("[RLEASE TO SERVER]: lid=>%llu id=>%s\n", lid, id.c_str());
    
    int r;
    cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&lock_mutex);
    locks[lid].state = lock_client_info::NONE;
    pthread_mutex_unlock(&lock_mutex);

    pthread_cond_signal(&(locks[lid].cond));
    return rlock_protocol::OK; 
  }else{
    pthread_mutex_unlock(&lock_mutex);
    tprintf("[REVOKE]: lid=>%llu id=>%s insert into revokes\n", lid, id.c_str());
    pthread_mutex_lock(&revoke_mutex);  
    revokes.insert(lid);
    pthread_mutex_unlock(&revoke_mutex);

    return rlock_protocol::OK;
  }
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  tprintf("[RETRY]: lid=>%llu\n", lid);
  
  pthread_mutex_lock(&lock_mutex);
  locks[lid].state = lock_client_info::LOCKED;
  pthread_mutex_unlock(&lock_mutex); 
   
  int ret = rlock_protocol::OK;
  return ret;
}
