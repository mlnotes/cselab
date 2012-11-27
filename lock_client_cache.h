// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

#include <set>

class lock_client_info{
 public:
  enum xxstatus{NONE, FREE, LOCKED, ACQUIRING, RELEASING};
  typedef int status;
  
  status state;
  pthread_cond_t cond;

  lock_client_info(){
    pthread_cond_init(&cond, NULL);
  }
  ~lock_client_info(){
    pthread_cond_destroy(&cond);  
  }
};
// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

  pthread_mutex_t lock_mutex;
  pthread_mutex_t revoke_mutex;
  std::map<lock_protocol::lockid_t, lock_client_info> locks;
  std::map<lock_protocol::lockid_t, pthread_cond_t> conds; 
  std::set<lock_protocol::lockid_t> revokes;

  lock_protocol::status acquire_from_server(lock_protocol::lockid_t);
  lock_protocol::status wait_for_lock(lock_protocol::lockid_t);
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
