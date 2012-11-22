// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
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
  for(int i = 0; i < ents.size(); ++i)
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

  printf("getfile %016llx\n", inum);
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

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int
yfs_client::readdir(inum inum, std::vector<dirent> &ents)
{
  int r = OK;

  printf("readdir %016llx\n", inum);
  std::string str;
  r = ec->get(inum, str);
  if(r != extent_protocol::OK)
    return r;

  str2dir(str, ents);
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
  for(int i = 0; i < ents.size(); ++i){
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
  
  printf("create name -> %s inum -> %016llx\n", name.c_str(), inum);
  
  std::vector<dirent> ents;
  r = readdir(parent, ents);  
  if(r != extent_protocol::OK)
    return r;

  for(int i = 0; i < ents.size(); ++i){
    if(name == ents[i].name)
      return yfs_client::EXIST; 
  }
  
  // do the real work
  r = ec->put(inum, name);
  if(r != extent_protocol::OK)
    return r;
  
  dirent ent;
  ent.name = name;
  ent.inum = inum;    
  ents.push_back(ent);
  r = ec->put(parent, dir2str(ents));  

  return r;
}

int
yfs_client::setsize(inum inum, unsigned long long size)
{
  int r = OK;

  printf("setsize inum -> %016llx size -> %016llx\n", inum, size);

  std::string str;
  r = ec->get(inum, str); 
  if(r != extent_protocol::OK)
    return r;

  str.resize(size);
  r = ec->put(inum, str);
  return r;
}

int
yfs_client::read(inum inum, unsigned long long offset, unsigned long long size, std::string &buf)
{
  int r = OK;
  printf("yfs_client read inum-> %016llx offset -> %016llx size -> %016llx\n", inum, offset, size);

  std::string str;
  r = ec->get(inum, str);
  if(r != extent_protocol::OK)
    return r;

  if(offset >= str.size())
    buf = "";
  else
    buf = str.substr(offset, size);

  return r;
}

int
yfs_client::write(inum inum, unsigned long long offset, std::string &buf)
{
  int r = OK;
  printf("yfs_client write inum-> %016llx offset -> %016llx content -> %s\n", inum, offset, buf.c_str());

  std::string str;
  r = ec->get(inum, str);
  if(r != extent_protocol::OK)
    return r;

  if(str.size() < offset + buf.size())
    str.resize(offset + buf.size());

  str.replace(offset, buf.size(), buf);
  r = ec->put(inum, str);
  return r;
}
