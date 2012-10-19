// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include <queue>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <pthread.h>

class lock_server {

 protected:
  int nacquire;
  std::map<lock_protocol::lockid_t, int> locks;
  std::map<lock_protocol::lockid_t, pthread_cond_t* > conds;
  pthread_mutex_t mutex;
  

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);  
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);

};

#endif 
