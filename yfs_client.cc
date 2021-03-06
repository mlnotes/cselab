// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lrc = new lock_release_cache(ec);
  lc = new lock_client_cache(lock_dst, lrc);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

yfs_client::dirent
yfs_client::str2ent(std::string str){
  size_t pos = str.find_first_of(':');
  
  yfs_client::dirent ent;
  ent.name = str.substr(0, pos);
  ent.inum = n2i(str.substr(pos+1)); 
 
#ifdef ZDEBUG
  printf("str2ent str -> %s\n name -> %s\n inum -> %016llx\n", 
        str.c_str(), ent.name.c_str(), ent.inum);
#endif
 
  return ent;
}

int
yfs_client::str2dir(std::string str, std::vector<dirent> &ents)
{
  size_t begin = 0;
  size_t pos;
  char tok = ';';
  while((pos = str.find_first_of(tok, begin)) != std::string::npos){
    std::string str_ent = str.substr(begin, pos-begin);
    begin = pos + 1;

    // split the name and inum
    ents.push_back(str2ent(str_ent)); 
  }
  return ents.size();
}

std::string
yfs_client::dir2str(std::vector<dirent> &ents)
{
  std::string str = "";
  for(unsigned int i = 0; i < ents.size(); ++i)
    str += ents[i].name + ":" + filename(ents[i].inum) + ";";

  return str;
} 

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock

  printf("getfile %016llx\n", inum);

  lc->acquire(inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:
  lc->release(inum);
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;

  lc->acquire(inum);
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  lc->release(inum);
  return r;
}

int
yfs_client::readdir(inum inum, std::vector<dirent> &ents)
{
  int r = OK;

  printf("readdir %016llx\n", inum);

  lc->acquire(inum);
  std::string str;
  r = ec->get(inum, str);
  if(r != extent_protocol::OK)
    goto release;

  str2dir(str, ents);

  release:
    lc->release(inum);
    return r;
}

int
yfs_client::lookup(inum parent, std::string child, inum &inum)
{
  int r = OK;

  printf("lookup %016llx -> child %s\n", parent, child.c_str());
  std::vector<dirent> ents;
  r = readdir(parent, ents);
  if(r != extent_protocol::OK)
    return r;

  r = extent_protocol::NOENT;
  for(unsigned int i = 0; i < ents.size(); ++i){
    if(child == ents[i].name){
      inum = ents[i].inum;
      r = extent_protocol::OK;
      break;
    }
  }
  return r;
} 

int 
yfs_client::create(inum parent, inum inum, std::string name)
{  
  int r = OK;  
  dirent ent;

  printf("create name -> %s inum -> %016llx\n", name.c_str(), inum);

  std::string parent_str;
  std::vector<dirent> ents;
  
  lc->acquire(parent);
  r = ec->get(parent, parent_str);
  if(r != extent_protocol::OK)
    goto release;

  str2dir(parent_str, ents);

  for(unsigned int i = 0; i < ents.size(); ++i){
    if(name == ents[i].name){
      r = yfs_client::EXIST;
      goto release;
    }
  }
  
  // do the real work
  lc->acquire(inum);
  r = ec->put(inum, name);
  lc->release(inum);

  if(r != extent_protocol::OK)
    goto release;
 
  ent.name = name;
  ent.inum = inum;    
  ents.push_back(ent);
  r = ec->put(parent, dir2str(ents));  

 release:
  lc->release(parent);
  return r;
}

int
yfs_client::setsize(inum inum, unsigned long long size)
{
  int r = OK;

  printf("setsize inum -> %016llx size -> %016llx\n", inum, size);

  lc->acquire(inum);
  std::string str;
  r = ec->get(inum, str); 
  if(r != extent_protocol::OK)
    goto release;

  str.resize(size);
  r = ec->put(inum, str);

 release:
  lc->release(inum);
  return r;
}

int
yfs_client::read(inum inum, unsigned long long offset, unsigned long long size, std::string &buf)
{
  int r = OK;
  printf("yfs_client read inum-> %016llx offset -> %016llx size -> %016llx\n", inum, offset, size);

  lc->acquire(inum);
  std::string str;
  r = ec->get(inum, str);
  if(r != extent_protocol::OK)
    goto release;

  if(offset >= str.size())
    buf = "";
  else
    buf = str.substr(offset, size);
  
  release:
    lc->release(inum);
    return r;
}

int
yfs_client::write(inum inum, unsigned long long offset, std::string &buf)
{
  int r = OK;
  printf("yfs_client write inum-> %016llx offset -> %016llx content -> %s\n", inum, offset, buf.c_str());

  lc->acquire(inum);
  std::string str;
  r = ec->get(inum, str);
  if(r != extent_protocol::OK)
    goto release;

  if(str.size() < offset + buf.size())
    str.resize(offset + buf.size());

  str.replace(offset, buf.size(), buf);
  r = ec->put(inum, str);

 release:
  lc->release(inum);
  return r;
}

int
yfs_client::unlink(inum parent, std::string name){

  int r = OK;
  inum inum = 0; 
  bool found = false;
 
  printf("unlink name -> %s parent -> %016llx\n", name.c_str(), parent);

  std::string parent_str;
  std::vector<dirent> ents;
  std::vector<dirent>::iterator iter;
  
  lc->acquire(parent);
  r = ec->get(parent, parent_str);
  if(r != extent_protocol::OK)
    goto release;
  
  str2dir(parent_str, ents);

  iter = ents.begin();
  while(iter != ents.end()){
    if(name == (*iter).name){
      found = true;
      inum = (*iter).inum;
      ents.erase(iter);
      break;
    }
    iter++;
  }
  
  if(!found){
    r = extent_protocol::NOENT; 
    goto release;
  }
 
  r = ec->put(parent, dir2str(ents));
  if(r != extent_protocol::OK)
    goto release;
  else{
    // remove content
    lc->acquire(inum);
    r = ec->remove(inum);
    lc->release(inum);
  }

 release:
  lc->release(parent);
  return r;
}
