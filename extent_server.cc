// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server()
{
  VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
  int r;
  VERIFY(put(0x00000001, "", r) == extent_protocol::OK);
}

extent_server::~extent_server()
{
  VERIFY(pthread_mutex_destroy(&mutex) == 0);
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  ScopedLock lock(&mutex);
  fs[id] = buf;
  extent_protocol::attr a = attrs[id];
  a.size = buf.size();
  a.ctime = a.mtime = time(0);
  attrs[id] = a;

  printf("put id=>%16llu, buf=>%s\n", id, buf.c_str());
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
#ifdef ZDEBUG
printf("get %016llx\n", id);
#endif

  ScopedLock lock(&mutex);
  if(fs.find(id) == fs.end()){
    return extent_protocol::NOENT;
  }else{
    buf = fs[id];
    attrs[id].atime = time(0);
  }
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  ScopedLock lock(&mutex);
  if(attrs.find(id) == attrs.end()){  
    return extent_protocol::NOENT;
  }else{
    a = attrs[id];
    return extent_protocol::OK;
  }  
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  ScopedLock lock(&mutex);
  if(fs.find(id) == fs.end()){
    return extent_protocol::NOENT;
  }else{
    fs.erase(id);
    attrs.erase(id);
    return extent_protocol::OK;
  }
}
