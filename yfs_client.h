#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client_cache.h"

class yfs_client {
  extent_client *ec;
  lock_client_cache *lc;
  lock_release_cache *lrc; 
  
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);

  static dirent str2ent(std::string);
  static int str2dir(std::string, std::vector<dirent>&); 
  static std::string dir2str(std::vector<dirent>&);
public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int create(inum, inum, std::string);
  int lookup(inum, std::string, inum &);
  int readdir(inum, std::vector<dirent> &);
  int setsize(inum, unsigned long long);
  int read(inum, unsigned long long, unsigned long long, std::string &);  
  int write(inum, unsigned long long, std::string &);
  int unlink(inum, std::string);
};

#endif 
