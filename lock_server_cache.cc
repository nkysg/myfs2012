// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache() :
  nacquire(0)
{
  pthread_mutex_init(&mutex_, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  pthread_mutex_lock(&mutex_);
  ++nacquire;
  if ((iter = lockTable_.find(lid)) == lockTable_.end()) {
    lockTable_[lid] = new Lock(id);
    pthread_mutex_unlock(&mutex_);
  } else if (!iter->second->isHeld()){
    iter->second->set_held(id);
    if (iter->second->someoneWait()) {
      iter->second->setRevoke(true);
      pthread_mutex_unlock(&mutex_);
      ret = revoke(lid, id);
      if (ret == lock_protocol::OK) {
        pthread_mutex_lock(&mutex_);
        iter->second->setRevoke(false);
        pthread_mutex_unlock(&mutex_);
      }
    } else {
      pthread_mutex_unlock(&mutex_);
    }
  } else {
    iter->second->addWaitId(id);
    if (!iter->second->isRevoking()) {
      iter->second->setRevoke(true);
      std::string cid(iter->second->getHeldId());
      pthread_mutex_unlock(&mutex_);
      ret = revoke(lid, cid);
      if (ret == lock_protocol::OK) {
        pthread_mutex_lock(&mutex_);
        iter->second->setRevoke(false);
        pthread_mutex_unlock(&mutex_);
        ret = lock_protocol::RETRY;
      }
    } else {
      pthread_mutex_unlock(&mutex_);
      ret = lock_protocol::RETRY;
    }
  }
  return ret;
}

int
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  std::string cid;
  pthread_mutex_lock(&mutex_);
  --nacquire;
  if ((iter = lockTable_.find(lid)) != lockTable_.end()) {
    if (iter->second->isHeld()) {
      iter->second->set_unheld();
      if (iter->second->someoneWait()) {
        cid = iter->second->getWaitId();
        iter->second->removeWaitId();
      }

      pthread_mutex_unlock(&mutex_);
      ret = retry(lid, cid);
      if (ret == rlock_protocol::OK) {
        ret = lock_protocol::OK;
      } else {
        ret = lock_protocol::IOERR;
      }

    } else {
      pthread_mutex_unlock(&mutex_);
      ret = lock_protocol::NOENT;
    }
  } else {
    pthread_mutex_unlock(&mutex_);
    ret = lock_protocol::NOENT;
  }

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}


rlock_protocol::status
lock_server_cache::revoke(lock_protocol::lockid_t lid, std::string cid)
{
  int ret = lock_protocol::OK;

  int r;
  handle h(cid);
  rpcc *cl = h.safebind();
  if (cl) {
    ret = cl->call(rlock_protocol::revoke, lid, r);
  } else {
    ret = lock_protocol::RPCERR;
  }

  return ret;

}

rlock_protocol::status
lock_server_cache::retry(lock_protocol::lockid_t lid, std::string cid)
{
  int ret = lock_protocol::OK;

  int r;
  handle h(cid);
  rpcc *cl = h.safebind();
  if (cl) {
    ret = cl->call(rlock_protocol::retry, lid, r);
  } else {
    ret = rlock_protocol::RPCERR;
  }

  return ret;
}
