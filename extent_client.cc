// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  if(cache.find(eid) != cache.end()){
    if(cache[eid].removed)
      return extent_protocol::NOENT;
    else if(cache[eid].content_cached){
      buf = cache[eid].content;
      cache[eid].a.atime = time(0); 
      printf("buffered content eid=>%16llu, buf=>%s\n", eid, buf.c_str());
      return ret;
    }
  }

  ret = cl->call(extent_protocol::get, eid, buf);
  printf("get conent from server buf=>%s\n", buf.c_str());
  if(ret == extent_protocol::OK){
    printf("cache content in the get eid=>%16llu\n", eid);
    cache[eid].content = buf; 
    cache[eid].content_cached = true;
    cache[eid].a.atime = time(0); 
  }
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  if(cache.find(eid) != cache.end()){
    if(cache[eid].removed)
      return extent_protocol::NOENT;
    attr = cache[eid].a;
    return ret;
  }

  ret = cl->call(extent_protocol::getattr, eid, attr);
  if(ret == extent_protocol::OK){
    printf("cache attr in the getattr eid=>%16llu\n", eid);
    cache[eid].a = attr;
  }

  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  cache[eid].removed = false;
  cache[eid].dirty = true;
  cache[eid].content_cached = true;
  cache[eid].content = buf;

  cache[eid].a.size = buf.size();
  cache[eid].a.ctime = cache[eid].a.mtime = time(0);
  printf("put content into buffer eid=>%16llu, buf=>%s\n", eid, buf.c_str());
 
   
  // TODO change attr?
  return ret;

/*
  int r;
  ret = cl->call(extent_protocol::put, eid, buf, r);
  return ret;
*/
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  cache[eid].removed = true;
  return ret;

/*
  int r;
  ret = cl->call(extent_protocol::remove, eid, r);
  return ret;
*/
}

void
extent_client::flush(extent_protocol::extentid_t eid){
  printf("extent client flush eid=>%16llu\n", eid);
  
  if(cache.find(eid) == cache.end())
    return;

  if(cache[eid].removed){
    int r;
    printf("extent client flush removed eid=>%16llu\n", eid);
    cl->call(extent_protocol::remove, eid, r);
  }else if(cache[eid].dirty){
    int r;
    printf("extent client flush buf=>%s\n", cache[eid].content.c_str());
    extent_protocol::status ret = cl->call(extent_protocol::put, eid, cache[eid].content, r);
    printf("extent client flush return ret=>%d\n", ret);
  }
  
  cache.erase(eid);
}
