// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc/rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

void
lock_release_flush::dorelease(lock_protocol::lockid_t eid) {
  ec_->flush(eid);
}

lock_client_cache::lock_client_cache(std::string xdst,
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  pthread_mutex_init(&mutex_, NULL);

  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  int r;
  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(lid)) == cache_.end()) {
    cache_[lid] = new Lock(mutex_);
    iter = cache_.find(lid);
  }

  iter->second->acquire();

reswitch:
  switch (iter->second->getState()) {
    case NONE:
      {
        iter->second->setState(ACQUIRING);
        pthread_mutex_unlock(&mutex_);

        while ((ret = cl->call(lock_protocol::acquire, lid, id, r)) == lock_protocol::RETRY) {
          pthread_mutex_lock(&mutex_);
          while (!iter->second->needRetry()) {
            iter->second->wait();
          }
          iter->second->setRetry(false);
          pthread_mutex_unlock(&mutex_);
        }

        if (ret == lock_protocol::OK) {
          pthread_mutex_lock(&mutex_);
          iter->second->setState(LOCKED);
          iter->second->setHeldId(pthread_self());
        }

        break;
      }
    case FREE:
      {
        iter->second->setHeldId(pthread_self());
        iter->second->setState(LOCKED);
        break;
      }
    case LOCKED:
      {
        do {
          iter->second->wait();
          if (iter->second->getState() == NONE) {
            goto reswitch;
          }
        } while (iter->second->getState() != FREE);

        iter->second->setHeldId(pthread_self());
        iter->second->setState(LOCKED);
        break;
      }
    case ACQUIRING:
      {
        do {
          iter->second->wait();
          if (iter->second->getState() == NONE) {
            goto reswitch;
          }
        } while (iter->second->getState() != FREE);
        iter->second->setHeldId(pthread_self());
        iter->second->setState(LOCKED);
        break;
      }
    case RELEASING:
      {
        do {
          iter->second->wait();
          if (iter->second->getState() == NONE) {
            goto reswitch;
          }
        } while (iter->second->getState() != FREE);
        iter->second->setHeldId(pthread_self());
        iter->second->setState(LOCKED);
        break;
      }
  }

  pthread_mutex_unlock(&mutex_);
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  int r;
  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(lid)) != cache_.end()) {
    iter->second->release();
    if (iter->second->getState() == LOCKED) {

      if (iter->second->needRevoke()) {
        iter->second->setState(RELEASING);
        pthread_mutex_unlock(&mutex_);

        lu->dorelease(lid);
        ret = cl->call(lock_protocol::release, lid, id, r);

        pthread_mutex_lock(&mutex_);
        if (ret == lock_protocol::OK) {
          iter->second->setState(NONE);
          iter->second->setRevoke(false);
        }

        iter->second->notifyall();
      } else {
        iter->second->setState(FREE);
        if (iter->second->someoneWait()) {
          iter->second->notifyall();
        }
      }

    } else {
      ret = lock_protocol::NOENT;
    }
  } else {
    ret = lock_protocol::NOENT;
  }

  pthread_mutex_unlock(&mutex_);
  return ret;

}
// above is client to server, to call func at server by rpc
// below is server to client, called by server by rpc
rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                  int &)
{
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  int r;
  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(lid)) != cache_.end()) {
    iter->second->setRevoke(true);
    if (iter->second->getState() == FREE) {
      iter->second->setState(RELEASING);
      pthread_mutex_unlock(&mutex_);
      lu->dorelease(lid);
      ret = cl->call(lock_protocol::release, lid, id, r);
      pthread_mutex_lock(&mutex_);
      if (ret == lock_protocol::OK) {
        iter->second->setState(NONE);
        iter->second->setRevoke(false);
      } else {
        ret = rlock_protocol::RPCERR;
      }

      iter->second->notifyall();  // should ?
    } // else just return, leave the release to deal
  } else {
    ret = rlock_protocol::RPCERR;
  }

  pthread_mutex_unlock(&mutex_);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                 int &)
{
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  pthread_mutex_lock(&mutex_);
  if ((iter = cache_.find(lid)) != cache_.end()) {
    if (iter->second->getState() == ACQUIRING) {
      iter->second->setRetry(true);
      iter->second->notifyall();
    } else {
      ret = rlock_protocol::RPCERR;
    }
  } else {
    ret = rlock_protocol::RPCERR;
  }
  pthread_mutex_unlock(&mutex_);

  return ret;
}

