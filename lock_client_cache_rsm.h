// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include "rpc/fifo.h"

#include "rsm_client.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};


class lock_client_cache_rsm;

// Clients that caches locks.  The server can revoke locks using
// lock_revoke_server.
class lock_client_cache_rsm : public lock_client {
 public:
  enum state {
    NONE = 0,
    FREE,
    LOCKED,
    ACQUIRING,
    RELEASING,
  };
 private:
  struct Lock {
    Lock() :
      nacquire(0), revoke_(false), retry_(false), state_(NONE) {
      pthread_cond_init(&waitcond_, NULL);
    }
    ~Lock() {
      pthread_cond_destroy(&waitcond_);
    }

    pthread_cond_t waitcond_;

    long nacquire;
    bool revoke_;
    bool retry_;
    state state_;
    lock_protocol::xid_t xid;
  };

  std::map<lock_protocol::lockid_t, Lock *> cache_;
  pthread_mutex_t mutex_;

  struct server_entry {
    server_entry() { }
    server_entry(lock_protocol::lockid_t _lid,
        lock_protocol::xid_t _xid)
      : lid(_lid), xid(_xid) { }

    lock_protocol::lockid_t lid;
    lock_protocol::xid_t xid;
  };

  fifo<server_entry> releaseq;
 private:
  rsm_client *rsmc;
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  lock_protocol::xid_t xid;
 public:
  static int last_port;
  lock_client_cache_rsm(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache_rsm() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  virtual lock_protocol::status release(lock_protocol::lockid_t);
  void releaser();
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
				        lock_protocol::xid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t,
				       lock_protocol::xid_t, int &);
};


#endif
