// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "extent_client.h"
#include "lang/verify.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_release_flush : public lock_release_user {
 public:
   lock_release_flush(extent_client *ec) : ec_(ec) { }
   virtual void dorelease(lock_protocol::lockid_t eid);
   virtual ~lock_release_flush() {};
 private:
   extent_client *ec_;
};

class lock_client_cache : public lock_client {
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
    Lock(pthread_mutex_t &mutex) :
      mutex_(mutex), nacquire(0), revoke_(false), retry_(false), state_(NONE) {
      pthread_cond_init(&cond_, NULL);
    }
    ~Lock() {
      pthread_cond_destroy(&cond_);
    }

    state getState() {
      return state_;
    }

    void setState(state s) {
      state_ = s;
    }

    long getHeldId() {
      return heldId;
    }

    void setHeldId(long tid) {
      heldId = tid;
    }

    void acquire() {
      ++nacquire;
    }
    void release() {
      --nacquire;
    }
    long someoneWait() {
      return nacquire >= 1;
    }
    bool needRevoke() {
      return revoke_;
    }
    void setRevoke(bool need) {
      revoke_ = need;
    }

    bool needRetry() {
      return retry_;
    }
    void setRetry(bool need) {
      retry_ = need;
    }

    void wait() {
      pthread_cond_wait(&cond_, &mutex_);
    }

    void notify() {
      pthread_cond_signal(&cond_);
    }

    void notifyall() {
      pthread_cond_broadcast(&cond_);
    }

    private:
    Lock(Lock &);

    pthread_mutex_t &mutex_;
    pthread_cond_t cond_;
    long heldId;
    long nacquire;
    bool revoke_;
    bool retry_;
    state state_;
    // rstate rstate_;
  };

  std::map<lock_protocol::lockid_t, Lock *> cache_;
  pthread_mutex_t mutex_;
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {
    pthread_mutex_destroy(&mutex_);
  };
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t,
                                       int &);
};


#endif
