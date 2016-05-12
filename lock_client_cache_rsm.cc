// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"
#include "rpc/rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include "handle.h"

#include "rsm_client.h"

static void *
releasethread(void *x)
{
  lock_client_cache_rsm *cc = (lock_client_cache_rsm *) x;
  cc->releaser();
  return 0;
}

int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst,
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache_rsm::retry_handler);
  xid = 0;
  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC
  //   calls instead of the rpcc object of lock_client
  rsmc = new rsm_client(xdst);
  pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  VERIFY (r == 0);

  pthread_mutex_init(&mutex_, NULL);
}


void
lock_client_cache_rsm::releaser()
{

  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.

  while (true) {
    server_entry e;
    releaseq.deq(&e);

    lock_protocol::status ret;
    int r;
    if (lu) {
      lu->dorelease(e.lid);
    }
    ret = rsmc->call(lock_protocol::release, e.lid, id, e.xid, r);
    {
      ScopedLock l(&mutex_);
      std::map<lock_protocol::lockid_t, Lock *>::iterator iter;
      iter = cache_.find(e.lid);
      VERIFY(iter != cache_.end());
      Lock *entry = iter->second;
      entry->state_ = NONE;
      // pthread_cond_broadcast(&entry->retrycond_);
      pthread_cond_broadcast(&entry->waitcond_);
    }

  }
}


lock_protocol::status
lock_client_cache_rsm::acquire(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  int r;
  ScopedLock l(&mutex_);

  if ((iter = cache_.find(lid)) == cache_.end()) {
    cache_[lid] = new Lock();
    iter = cache_.find(lid);
  }
  Lock *entry = iter->second;
  entry->nacquire++;

  while (true) {

    switch (entry->state_) {
      case NONE:
        {
          entry->state_ = ACQUIRING;
          entry->xid = xid;
          lock_protocol::xid_t cur_xid = xid;
          ++xid;
          pthread_mutex_unlock(&mutex_);

          // tprintf("client none:\n");
          ret = rsmc->call(lock_protocol::acquire, lid, id, cur_xid, r);
          // tprintf("client none2:\n");

          pthread_mutex_lock(&mutex_);
          if (ret == lock_protocol::OK) {
            entry->state_ = LOCKED;
            return ret;
          } else if (ret == lock_protocol::RETRY) {
            // if (!entry->retry_) {
              // struct timespec now, deadline;
              // clock_gettime(CLOCK_REALTIME, &now);
              // deadline.tv_sec = now.tv_sec + 3;
              // deadline.tv_nsec = 0;

              // // in case the server down
              // int t = pthread_cond_timedwait(&entry->waitcond_, &mutex_, &deadline);
              // if (t == ETIMEDOUT) {
                // entry->retry_ = true;
              // }
            // }
          // // tprintf("client none3:\n");
          } else {
            // error
            return lock_protocol::RPCERR;
          }
          break;
        }
      case FREE:
        {
          // tprintf("client free:\n");
          entry->state_ = LOCKED;
          return lock_protocol::OK;
        }
      case LOCKED:
        {
          // tprintf("client acquire locked:\n");
          pthread_cond_wait(&entry->waitcond_, &mutex_);
          // if (entry->state_ == FREE) {
            // entry->state_ = LOCKED;
            // return lock_protocol::OK;
          // }
          // tprintf("client acquir locked2:\n");
          break;
        }
      case ACQUIRING:
        {
          if (!entry->retry_) {
          // tprintf("client acquiring:\n");
            // pthread_cond_wait(&entry->waitcond_, &mutex_);

            struct timespec now, deadline;
            clock_gettime(CLOCK_REALTIME, &now);
            deadline.tv_sec = now.tv_sec + 3;
            deadline.tv_nsec = 0;

            // in case the server down
            int t = pthread_cond_timedwait(&entry->waitcond_, &mutex_, &deadline);
            if (t == ETIMEDOUT) {
              entry->retry_ = true;
            }

          // tprintf("client acquiring 2:\n");
          } else {
          // tprintf("client acquiring3:\n");
            entry->retry_ = false;
            entry->xid = xid;
            lock_protocol::xid_t cur_xid = xid;
            ++xid;

            pthread_mutex_unlock(&mutex_);
            ret = rsmc->call(lock_protocol::acquire, lid, id, cur_xid, r);
            pthread_mutex_lock(&mutex_);

            if (ret == lock_protocol::OK) {
              entry->state_ = LOCKED;
              return ret;
            } else if (ret == lock_protocol::RETRY) {
              // if (!entry->retry_) {
                // struct timespec now, deadline;
                // clock_gettime(CLOCK_REALTIME, &now);
                // deadline.tv_sec = now.tv_sec + 3;
                // deadline.tv_nsec = 0;

                // // in case the server down
                // int t = pthread_cond_timedwait(&entry->waitcond_, &mutex_, &deadline);
                // if (t == ETIMEDOUT) {
                  // entry->retry_ = true;
                // }
              // }
            }
          }
          break;
        }
      case RELEASING:
        {
          // tprintf("client releasing:\n");
          pthread_cond_wait(&entry->waitcond_, &mutex_);
          // tprintf("client releasing222222:\n");
          break;
        }
    }

  }
  return ret;
}

lock_protocol::status
lock_client_cache_rsm::release(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  int r;
  ScopedLock l(&mutex_);
  if ((iter = cache_.find(lid)) != cache_.end()) {
    Lock *entry = iter->second;
    entry->nacquire--;

    if (entry->state_ == LOCKED) {
      if (entry->revoke_) {
          // tprintf("client releasing 11111:\n");
        entry->state_ = RELEASING;
          entry->revoke_ = false;
        lock_protocol::xid_t cur_xid = entry->xid;
        pthread_mutex_unlock(&mutex_);

        if (lu) {
          lu->dorelease(lid);
        }
        ret = rsmc->call(lock_protocol::release, lid, id, cur_xid, r);

        pthread_mutex_lock(&mutex_);
        // if (ret == lock_protocol::OK) {
          entry->state_ = NONE;
          // entry->revoke_ = false;
        // }
        pthread_cond_broadcast(&entry->waitcond_);
        // pthread_cond_broadcast(&entry->revokecond_);
      } else {
          // tprintf("client releasing 2222:\n");
        entry->state_ = FREE;
        pthread_cond_broadcast(&entry->waitcond_);
      }
    } else {
      ret = lock_protocol::NOENT;
    }
  } else {
    ret = lock_protocol::NOENT;
  }

  return lock_protocol::OK;
}


rlock_protocol::status
lock_client_cache_rsm::revoke_handler(lock_protocol::lockid_t lid,
			          lock_protocol::xid_t xid, int &)
{
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  int r;
  ScopedLock l(&mutex_);
  // tprintf("revoke: id: %s xid:%llu\n", id.c_str(), xid);
  if ((iter = cache_.find(lid)) != cache_.end()) {
    Lock *entry = iter->second;
      if (xid == entry->xid) {
        entry->revoke_ = true;
        if (entry->state_ == FREE) {
          entry->state_ = RELEASING;
          releaseq.enq(server_entry(lid, xid));
          entry->revoke_ = false;
        }
      } else {
        tprintf("revoke error, out of date: xid: %llu, entryxid: %llu\n", xid, entry->xid);
      }

  } else {
    tprintf("reveke error:.....\n");
    ret = rlock_protocol::RPCERR;
  }
  return ret;
}

rlock_protocol::status
lock_client_cache_rsm::retry_handler(lock_protocol::lockid_t lid,
			         lock_protocol::xid_t xid, int &)
{
  int ret = rlock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;

  ScopedLock l(&mutex_);
  if ((iter = cache_.find(lid)) != cache_.end()) {
    Lock *entry = iter->second;
    if (xid == entry->xid) {
      // if (entry->state_ == ACQUIRING) {
        entry->retry_ = true;
        pthread_cond_broadcast(&entry->waitcond_);
      // }
    }
  } else {
    ret = rlock_protocol::RPCERR;
  }
  return ret;
}


