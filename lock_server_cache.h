#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include <deque>
#include "lock_protocol.h"
#include "rpc/rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  struct Lock {
    Lock() : held(false), revoke(false) { }

    Lock(std::string id)
      : held(true), revoke(false), heldId(id) { }

    ~Lock() { }

    bool isHeld() {
      return held;
    }
    void set_held(std::string id) {
      held = true;
      heldId = id;
    }
    void set_unheld() {
      held = false;
    }

    bool isRevoking() {
      return revoke;
    }
    void setRevoke(bool s) {
      revoke = s;
    }

    std::string getHeldId() {
      return heldId;
    }
    bool someoneWait() {
      return !waitIds.empty();
    }
    std::string getWaitId() {
      return waitIds.front();
    }
    void addWaitId(std::string id) {
      waitIds.push_back(id);
    }
    void removeWaitId() {
      waitIds.pop_front();
    }

   private:
    bool held;
    bool revoke;
    std::string heldId;
    std::deque<std::string> waitIds;
  };

  std::map<lock_protocol::lockid_t, Lock *> lockTable_;
  pthread_mutex_t mutex_;
 private:
  int nacquire;
 public:
  lock_server_cache();
  ~lock_server_cache() {
    pthread_mutex_destroy(&mutex_);
  }
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
 private:
  rlock_protocol::status revoke(lock_protocol::lockid_t, std::string cid);
  rlock_protocol::status retry(lock_protocol::lockid_t, std::string cid);
};

#endif
