#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_info{
 public:
  std::string owner;
  std::string waiting;
};

class lock_server_cache {
 private:
  int nacquire;

  std::map<lock_protocol::lockid_t, lock_info> locks;
  pthread_mutex_t mutex;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
