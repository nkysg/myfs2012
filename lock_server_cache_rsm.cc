// the caching lock server implementation

#include "lock_server_cache_rsm.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


static void *
revokethread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->revoker();
  return 0;
}

static void *
retrythread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->retryer();
  return 0;
}

lock_server_cache_rsm::lock_server_cache_rsm(class rsm *_rsm)
  : rsm (_rsm)
{
  pthread_mutex_init(&mutex_, NULL);

  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  VERIFY (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  VERIFY (r == 0);

  rsm->set_state_transfer(this);
}

void
lock_server_cache_rsm::revoker()
{

  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
  while (true) {
    client_entry e;
    revokeq.deq(&e);

    if (rsm->amiprimary()) {
      handle h(e.cid);
      rpcc *cl = h.safebind();
      if (cl) {
        int r;
        tprintf("i am revoking....\n");
        cl->call(rlock_protocol::revoke, e.lid, e.xid, r);
      }
    }
  }
}


void
lock_server_cache_rsm::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.
  while (true) {
    client_entry e;
    retryq.deq(&e);
    if (rsm->amiprimary()) {
      handle h(e.cid);
      rpcc *cl = h.safebind();
      if (cl) {
        int r;
        tprintf("let the client:%s to revoke\n", e.cid.c_str());
        cl->call(rlock_protocol::retry, e.lid, e.xid, r);
      }
    }
  }
}


int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id,
             lock_protocol::xid_t xid, int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;
  std::map<std::string, lock_protocol::xid_t>::iterator it_xid;

  ScopedLock l(&mutex_);
  ++nacquire;

  iter  = lockTable_.find(lid);
  if (iter == lockTable_.end()) {
    lockTable_.insert(std::make_pair(lid, new Lock()));
  }
  Lock *entry = lockTable_[lid];

  if ((it_xid = entry->clt_seq.find(id)) == entry->clt_seq.end() || xid > it_xid->second) {
    // tprintf("acquire: id: %s it_xid: %llu, xid: %llu\n",id.c_str(), it_xid->second, xid);
    entry->clt_seq[id] = xid;
    entry->release_status.erase(id);

    if (!entry->held) {
      // tprintf("acquire: held \n");
      entry->held = true;
      entry->heldId = id;

      entry->waitIds.erase(id);
      if (!entry->waitIds.empty()) {
        entry->revoke = true;
        revokeq.enq(client_entry(id, lid, xid));
      }
    } else {
      // tprintf("acquire: wait\n");
      entry->waitIds.insert(id);
      if (!entry->revoke) {
        entry->revoke = true;
        std::string cid(entry->heldId);
        revokeq.enq(client_entry(cid, lid, entry->clt_seq[cid]));
      }
      ret = lock_protocol::RETRY;
    }

    entry->acquire_status[id] = ret;
  } else if (xid == it_xid->second) {
    ret = entry->acquire_status[id];
  } else {
    tprintf("the out-of-date requests, %llu\n", xid);
  }
  return ret;
}

int
lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id,
         lock_protocol::xid_t xid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter;
  std::map<std::string, lock_protocol::xid_t>::iterator it_xid;

  std::string cid;
  ScopedLock l(&mutex_);
  --nacquire;

  iter = lockTable_.find(lid);
  if (iter == lockTable_.end()) {
    return lock_protocol::NOENT;
  }

  Lock *entry = lockTable_[lid];

  if ((it_xid = entry->clt_seq.find(id)) != entry->clt_seq.end() && it_xid->second == xid) {
    // tprintf("release: it_xid: %llu, xid: %llu\n", it_xid->second, xid);

    std::map<std::string, lock_protocol::status>::iterator it_rs;
    it_rs = entry->release_status.find(id);

    if (it_rs == entry->release_status.end()) {
      if (entry->held) {
        entry->held = false;
        entry->revoke = false;

        if (!entry->waitIds.empty()) {
          cid = *(entry->waitIds.begin());
          entry->waitIds.erase(cid);
          retryq.enq(client_entry(cid, lid, entry->clt_seq[cid]));
          tprintf("release: let client to retry\n");
        }
      } else {
        tprintf("error, cid: %s, release unheld lock!\n", id.c_str());
        ret = lock_protocol::NOENT;
      }
      entry->release_status[id] = ret;
    } else {
      ret = it_rs->second;
    }

  } else if (it_xid != entry->clt_seq.end() && it_xid->second < xid){
    tprintf("release error: out of date request, it_xid: %llu, xid: %llu\n", it_xid->second, xid);
    ret = lock_protocol::RPCERR;
  } else {
    tprintf("relsase error: haven't receive the acquire number\n");
    ret = lock_protocol::RPCERR;
  }
  tprintf("release: client:%s is relseasing\n", id.c_str());
  return ret;
}

std::string
lock_server_cache_rsm::marshal_state()
{
  std::ostringstream ost;
  std::string r;

  ScopedLock ml(&mutex_);
  marshall rep;
  rep << (unsigned int)lockTable_.size();
  std::map<lock_protocol::lockid_t, Lock *>::iterator iter_lock;
  for (iter_lock = lockTable_.begin(); iter_lock != lockTable_.end(); ++iter_lock) {
    lock_protocol::lockid_t lid = iter_lock->first;
    Lock *entry = iter_lock->second;

    rep << lid;

    rep << entry->held;
    rep << entry->heldId;

    rep << entry->waitIds;
    rep << entry->clt_seq;
    rep << entry->acquire_status;
    rep << entry->release_status;

  }
  r = rep.str();
  return r;
}

void
lock_server_cache_rsm::unmarshal_state(std::string state)
{
  ScopedLock ml(&mutex_);
  unmarshall rep(state);
  unsigned int locks_size;
  rep >> locks_size;
  for (unsigned int i = 0; i < locks_size; i++) {
    lock_protocol::lockid_t lid;
    rep >> lid;

    Lock *entry = new Lock();
    rep >> entry->held;
    rep >> entry->heldId;

    rep >> entry->waitIds;
    rep >> entry->clt_seq;
    rep >> entry->acquire_status;
    rep >> entry->release_status;

    lockTable_.insert(std::make_pair(lid, entry));
  }
}

lock_protocol::status
lock_server_cache_rsm::stat(lock_protocol::lockid_t lid, int &r)
{
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

