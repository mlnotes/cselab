// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{  
  tprintf("acquire request: lid=>%llu id=>%s\n", lid, id.c_str());

  VERIFY(pthread_mutex_lock(&mutex) == 0);
  if(locks.find(lid) == locks.end())
  {
    locks[lid].owner = id;
    VERIFY(pthread_mutex_unlock(&mutex) == 0); 
    return lock_protocol::OK;
  }else{
    if(locks[lid].owner == id){
      VERIFY(pthread_mutex_unlock(&mutex) == 0); 
      return lock_protocol::OK; 
    }else if(locks[lid].waiting.size() > 0){ // there is alreay one client waiting for this lock
      VERIFY(pthread_mutex_unlock(&mutex) == 0); 
      return lock_protocol::RETRY;     
    }else if(locks[lid].waiting.size() == 0){  // lock is own by other client, but no client is waiting for this lock
      locks[lid].waiting = id;  
      // send a revoke to the owner of this lock
      handle h(locks[lid].owner);
      VERIFY(pthread_mutex_unlock(&mutex) == 0); 

      rpcc* cl = h.safebind();
      if(cl){
        int r;
        cl->call(rlock_protocol::revoke, lid, r);
      }
      return lock_protocol::RETRY;
    }
  }
  return lock_protocol::NOENT;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  tprintf("release request: lid=>%llu id=>%s\n", lid, id.c_str());
  VERIFY(pthread_mutex_lock(&mutex) == 0);
  if(locks.find(lid) != locks.end()){
    locks[lid].owner = "";
    if(locks[lid].waiting.size() == 0){  // remove this lock's info
      locks.erase(lid); 
      VERIFY(pthread_mutex_unlock(&mutex) == 0);
    }else{        
      // send a retry to the waiting client
      handle h(locks[lid].waiting);
      VERIFY(pthread_mutex_unlock(&mutex) == 0);
      
      rpcc* cl = h.safebind();
      if(cl){
        int r;
        cl->call(rlock_protocol::retry, lid, r);       
        VERIFY(pthread_mutex_lock(&mutex) == 0);
        locks[lid].owner = locks[lid].waiting;
        locks[lid].waiting = "";  
        tprintf("CHANGE WAITING TO OWNER NEW OWNER: %s\n", locks[lid].owner.c_str());
        VERIFY(pthread_mutex_unlock(&mutex) == 0);
      }
    } 
    return lock_protocol::OK; 
  } 
  return lock_protocol::NOENT;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request: lid=>%llu\n", lid);
  r = nacquire;
  return lock_protocol::OK;
}

