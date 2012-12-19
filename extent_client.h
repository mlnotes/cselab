// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "lock_client_cache.h"
#include "rpc.h"
#include <map>

struct extent_cache {
  extent_cache(){
    dirty = false;
    removed = false;
    content_cached = false;
  }

  bool dirty;
  bool removed;

  bool content_cached;
  std::string content;
  extent_protocol::attr a;
};


class extent_client {
 private:
  rpcc *cl;
  std::map<extent_protocol::extentid_t, extent_cache> cache;

 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  void flush(extent_protocol::extentid_t eid);
};

class lock_release_cache:public lock_release_user{
public:
  lock_release_cache(extent_client *ec){
    this->ec = ec;
  }

  void dorelease(lock_protocol::lockid_t lid){
    if(ec != 0)
      ec->flush(lid);
  }
private:
  extent_client *ec;
};

#endif 

