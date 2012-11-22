// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;

  if(locks.find(lid) != locks.end())
    r = locks[lid];
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("acqure request from clt %d\n", clt);
  r = nacquire;

  VERIFY(pthread_mutex_lock(&mutex) == 0);

  if(locks.find(lid) == locks.end())
  {
    locks[lid] = clt;
    // set variable condition
    if(conds.find(lid) == conds.end())
    {
      pthread_cond_t *p_cond = new pthread_cond_t();
      pthread_cond_init(p_cond, NULL);
      conds[lid] = p_cond;
    }
  } 
  else
  {
    // TODO wait for the lock to be free
    while(locks[lid])
      pthread_cond_wait(conds[lid], &mutex); 
    
    locks[lid] = clt;
  } 
  
  r = clt;
  VERIFY(pthread_mutex_unlock(&mutex) == 0);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("release request from clt %d\n", clt);
  r = nacquire;

  VERIFY(pthread_mutex_lock(&mutex) == 0);

  if(locks.find(lid) != locks.end())
    locks.erase(lid);

  if(conds.find(lid) != conds.end())
    pthread_cond_signal(conds[lid]);


  VERIFY(pthread_mutex_unlock(&mutex) == 0);
  r = clt;
  return ret;
}
