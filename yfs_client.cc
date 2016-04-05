// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "rpc/slock.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

static unsigned int
random_number_seed()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  const unsigned int kPrime1 = 61631;
  const unsigned int kPrime2 = 64997;
  const unsigned int kPrime3 = 111857;
  return kPrime1 * static_cast<unsigned int>(getpid())
    + kPrime2 * static_cast<unsigned int>(tv.tv_sec)
    + kPrime3 * static_cast<unsigned int>(tv.tv_usec);
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  // init the seed for random
  srandom(random_number_seed());
  std::string buf;
  // when root directory is not exist, add it as a empty directory to the server
  ScopedLock ml(&mutex_);
  if (ec->get(0x00000001, buf) == extent_protocol::NOENT) {
    ec->put(0x00000001, buf);
  }
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

std::istringstream& operator>>(std::istringstream &is, yfs_client::dirent &dirent)
{
  is >> dirent.name;
  is >> dirent.inum;
  return is;
}

std::ostringstream& operator<<(std::ostringstream &os, yfs_client::dirent &dirent)
{
  os << dirent.name << " " << dirent.inum << " ";
  return os;
}

yfs_client::inum
yfs_client::random_inum()
{
  inum inum;
  inum = random();
  inum |= 0x80000000;
  return inum;
}


// this func can ensure the unique id of the inum
yfs_client::inum
yfs_client::random_inum(int)
{
  inum inum;
  std::string buf;
  do {
    // it should be srandom somewhere such as
    // the yfs_client construct function
    // notice!!! don't srandom frequently!!!then it will broken the random!!!
    inum = random();
    inum |= 0x80000000;
    if (ec->get(inum, buf) != extent_protocol::OK) {
      break;
    }
  } while (1);

  return inum;
}

int
yfs_client::createfile(inum parent, const char *name, mode_t mode, inum &inum)
{
  int r = OK;
  std::string buf;
  inum = random_inum(0);
  if (ec->put(inum, buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  buf.clear();
  if (ec->get(parent, buf) != extent_protocol::OK) {
    r = NOENT;
    return r;
  }

  // add the <name, ino> entry into @parent
  dirent entry;
  entry.name = name;
  entry.inum = inum;

  std::ostringstream ost;
  ost << entry;
  buf.append(ost.str());

  if (ec->put(parent, buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  return r;
}

int
yfs_client::look_up_file(inum parent, const char *name, bool &found, inum &inum)
{
  int r = OK;
  std::string buf;
  if (ec->get(parent, buf) != extent_protocol::OK) {
    r = NOENT;
    return r;
  }

  dirent entry;
  std::istringstream ist(buf);
  found = false;
  while (ist >> entry) {
    if (entry.name.compare(name) == 0) {
      found = true;
      break;
    }
  }

   if (found) {
    inum = entry.inum;
  }

  return r;
}

int
yfs_client::read_dir(inum parent, std::vector<dirent> &entries)
{
  int r = OK;
  std::string buf;
  if (ec->get(parent, buf) != extent_protocol::OK) {
    r = NOENT;
    return r;
  }

  std::istringstream ist(buf);
  dirent entry;
  while (ist >> entry) {
    entries.push_back(entry);
  }

  return r;
}

int
yfs_client::set_attr(inum inum, struct stat *attr)
{
  int r = OK;
  std::string buf;
  if (ec->get(inum, buf) != extent_protocol::OK) {
    r = NOENT;
    return r;
  }

  buf.resize(attr->st_size);
  if (ec->put(inum, buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  return r;
}

int
yfs_client::read(inum inum, size_t size, off_t off, std::string &buf)
{
  int r = OK;
  std::string buf2;
  if (ec->get(inum, buf2) != extent_protocol::OK) {
    r = NOENT;
    return r;
  }
  if ((size_t)off >= buf2.size()) {
    return r;
  }
  off_t end = off + size > buf2.size() ? buf2.size() : off + size;
  buf.assign(buf2.substr(off, end - off));

  return r;
}

int
yfs_client::write(inum inum, const char *buf, size_t size, off_t off)
{
  int r = OK;
  std::string buf2;
  if (ec->get(inum, buf2) != extent_protocol::OK) {
    r = NOENT;
    return r;
  }

  size_t end = off + size;
  if (end > buf2.size()) {
    buf2.resize(end);
  }
  buf2.replace(off, size, buf, size);

  if (ec->put(inum, buf2) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }
  return r;
}
