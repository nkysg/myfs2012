// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex_, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  pthread_mutex_lock(&mutex_);
  ++nacquire;
  r = nacquire;
  pthread_mutex_unlock(&mutex_);

  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  pthread_mutex_lock(&mutex_);
  if ((iter = lockTable_.find(lid)) == lockTable_.end()) {
    lockTable_[lid] = new Lock(mutex_);
    lockTable_[lid]->lock();
  } else {
    while (iter->second->islocked()) {
      iter->second->wait();
    }
    iter->second->lock();
  }
  pthread_mutex_unlock(&mutex_);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  pthread_mutex_lock(&mutex_);
  --nacquire;
  r = nacquire;
  pthread_mutex_unlock(&mutex_);

  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;
  pthread_mutex_lock(&mutex_);
  if ((iter = lockTable_.find(lid)) == lockTable_.end() || !iter->second->islocked()) {
    ret = lock_protocol::RPCERR;
    return ret;
  }
  iter->second->unlock();
  iter->second->notify();
  pthread_mutex_unlock(&mutex_);

  return ret;
}
