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
  if (lockTable_.find(lid) == lockTable_.end()) {
    pthread_mutex_lock(&mutex_);
    if (lockTable_.find(lid) == lockTable_.end()) {
      lockTable_[lid] = new Mutex();
    }
    pthread_mutex_unlock(&mutex_);
  }
  lockTable_[lid]->Lock();
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
  if (lockTable_.find(lid) == lockTable_.end() || !lockTable_[lid]->islocked_) {
    ret = lock_protocol::RPCERR;
    return ret;
  }
  lockTable_[lid]->Unlock();
  return ret;
}
