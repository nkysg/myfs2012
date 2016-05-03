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
  pthread_mutex_init(&mutex_, NULL);
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
  std::map<extent_protocol::extentid_t, Cache *>::iterator iter;

  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(eid)) == cache_.end()) {
    cache_[eid] = new Cache();
    iter = cache_.find(eid);
    iter->second->caching = true;
    pthread_mutex_unlock(&mutex_);

    ret = cl->call(extent_protocol::get, eid, buf);

    extent_protocol::attr attr;
    ret = cl->call(extent_protocol::getattr, eid, attr);

    pthread_mutex_lock(&mutex_);
    iter->second->eid = eid;
    iter->second->buf = buf;
    iter->second->attr = attr;
    iter->second->caching = false;
    // need a judge if there is none wait?
    iter->second->notifyall();
  } else if (iter->second->removed) {
    ret = extent_protocol::NOENT;
  } else if (iter->second->caching) {
    iter->second->wait(&mutex_);
    buf.assign(iter->second->buf);
    iter->second->attr.atime = (unsigned int)time(NULL);
  } else {
    buf.assign(iter->second->buf);
    iter->second->attr.atime = (unsigned int)time(NULL);
  }
  pthread_mutex_unlock(&mutex_);
  return ret;
}

// the getattr's definition is ambiguous,
// so I didn't cost much time to correct it, just ensure it pass the test
// exactly, it can be stored in another map cache to reduce the get
extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, Cache *>::iterator iter;

  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(eid)) == cache_.end()) {
    cache_[eid] = new Cache();
    iter = cache_.find(eid);
    iter->second->caching = true;
    pthread_mutex_unlock(&mutex_);

    std::string buf;
    // actually it isn't need, but for easy I call it
    ret = cl->call(extent_protocol::get, eid, buf);

    ret = cl->call(extent_protocol::getattr, eid, attr);

    pthread_mutex_lock(&mutex_);
    iter->second->eid = eid;
    iter->second->buf = buf;
    iter->second->attr = attr;
    iter->second->caching = false;
    // need a judge if there is none wait?
    iter->second->notifyall();
  } else if (iter->second->removed) {
    ret = extent_protocol::NOENT;
  } else if (iter->second->caching) {
    iter->second->wait(&mutex_);
    attr = iter->second->attr;
  } else {
    attr = iter->second->attr;
  }
  pthread_mutex_unlock(&mutex_);

  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, Cache *>::iterator iter;

  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(eid)) == cache_.end()) {
    cache_[eid] = new Cache();
    iter = cache_.find(eid);
    iter->second->eid = eid;
    iter->second->dirty = true;
    iter->second->buf.assign(buf);

    // should change the access time?
    iter->second->attr.atime = (unsigned int)time(NULL);
    iter->second->attr.mtime = (unsigned int)time(NULL);
    iter->second->attr.ctime = (unsigned int)time(NULL);
    iter->second->attr.size = buf.size();
  } else {
    iter->second->dirty = true;
    iter->second->removed = false;
    iter->second->buf.assign(buf);

    iter->second->attr.mtime = (unsigned int)time(NULL);
    iter->second->attr.ctime = (unsigned int)time(NULL);
    iter->second->attr.size = buf.size();

  }
  pthread_mutex_unlock(&mutex_);

  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, Cache *>::iterator iter;

  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(eid)) == cache_.end()) {
    // just make a marker indicate that it have been delete
    // the lock_client ensure the consistency
    // but it can't recognize if the eid didn't exist
    cache_[eid] = new Cache();
    cache_[eid]->removed = true;
  } else {
    if (iter->second->removed) {
      ret = extent_protocol::NOENT;
    } else {
      iter->second->removed = true;
    }
  }
  pthread_mutex_unlock(&mutex_);

  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, Cache *>::iterator iter;

  int r;
  pthread_mutex_lock(&mutex_);
  VERIFY((iter = cache_.find(eid)) != cache_.end());

  if (iter->second->removed) {
    pthread_mutex_unlock(&mutex_);
    // the acquire ensure the correctness
    // the lock ensure every eid can be accessed just by one client's one thread every time
    ret = cl->call(extent_protocol::remove, eid, r);
    pthread_mutex_lock(&mutex_);
    delete iter->second;
    cache_.erase(iter);
  } else if (iter->second->dirty) {
    std::string buf(iter->second->buf);
    pthread_mutex_unlock(&mutex_);

    ret = cl->call(extent_protocol::put, eid, buf, r);

    pthread_mutex_lock(&mutex_);
    delete iter->second;
    cache_.erase(iter);
  } else {
    delete iter->second;
    cache_.erase(iter);
  }
  pthread_mutex_unlock(&mutex_);

  return ret;
}
