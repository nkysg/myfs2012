#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
  extent_client *ec;
  lock_client *lc;
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

  static inum random_inum(bool); // don't check the unique
  inum random_unique_inum(bool); // check the unique but maybe slow
 public:

  yfs_client(std::string, std::string);
  ~yfs_client();

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int look_up_file(inum, const char *, bool &, inum &);
  int createfile(inum, const char *, mode_t, inum &, bool);
  int unlink_file(inum, const char *);
  int read_dir(inum, std::vector<dirent> &);
  int set_attr(inum, struct stat *);
  int read(inum, size_t, off_t, std::string &);
  int write(inum, const char *, size_t, off_t);
};

std::istringstream& operator>>(std::istringstream &is, yfs_client::dirent &);
std::ostringstream& operator<<(std::ostringstream &os, yfs_client::dirent &);

class ScopedLockAcquire {
 public:
  ScopedLockAcquire(lock_client *lc, lock_protocol::lockid_t id)
    : lc_(lc), id_(id) { lc_->acquire(id_); }
  ~ScopedLockAcquire() { lc_->release(id_); }
 private:
  lock_client *lc_;
  lock_protocol::lockid_t id_;
};

#endif
