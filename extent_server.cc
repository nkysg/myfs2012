// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
  pthread_mutex_init(&mutex_, NULL);
  // TODO: we can load a file which saved before to the db
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  pthread_mutex_lock(&mutex_);
  db_[id].id_ = id;
  db_[id].buf_ = buf;
  if (db_[id].attr_.atime == 0) {
    db_[id].attr_.atime = (unsigned int)time(NULL);
  }
  db_[id].attr_.mtime = (unsigned int)time(NULL);
  db_[id].attr_.ctime = (unsigned int)time(NULL);
  db_[id].attr_.size = buf.size();
  pthread_mutex_unlock(&mutex_);
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  std::map<extent_protocol::extentid_t, struct DB>::iterator it;
  pthread_mutex_lock(&mutex_);
  if ((it = db_.find(id)) == db_.end()) {
    pthread_mutex_unlock(&mutex_);
    return extent_protocol::NOENT;
  }
  buf.assign(it->second.buf_);
  it->second.attr_.atime = (unsigned int)time(NULL);
  pthread_mutex_unlock(&mutex_);
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.

  // dbIter it;
  std::map<extent_protocol::extentid_t, struct DB>::iterator it;
  pthread_mutex_lock(&mutex_);
  if ((it = db_.find(id)) == db_.end()) {
    pthread_mutex_unlock(&mutex_);
    return extent_protocol::NOENT;
  }
  a = it->second.attr_;
  pthread_mutex_unlock(&mutex_);
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  std::map<extent_protocol::extentid_t, struct DB>::iterator it;
  pthread_mutex_lock(&mutex_);
  if ((it = db_.find(id)) != db_.end()) {
    db_.erase(it);
  }
  pthread_mutex_unlock(&mutex_);
  return extent_protocol::OK;
}

