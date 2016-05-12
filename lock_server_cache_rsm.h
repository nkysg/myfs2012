#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>
#include <set>
#include <deque>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"
#include "rpc/fifo.h"

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
    return *waitIds.begin();
  }
  void addWaitId(std::string id) {
    waitIds.insert(id);
  }
  void removeWaitId(std::string s) {
    waitIds.erase(s);
  }

 private:
  bool held;
  bool revoke;
  std::string heldId;
  std::set<std::string> waitIds;
 public:
  std::map<std::string, lock_protocol::xid_t> clt_seq;
  std::map<std::string, lock_protocol::status> acquire_status;
  std::map<std::string, lock_protocol::status> release_status;
};

class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  class rsm *rsm;

  std::map<lock_protocol::lockid_t, Lock *> lockTable_;
  pthread_mutex_t mutex_;

  struct client_entry {
    client_entry() { }

    client_entry(std::string _cid, lock_protocol::lockid_t lid,
        lock_protocol::xid_t _xid)
    : cid(_cid), lid (lid), xid(_xid) { }

    std::string cid;
    lock_protocol::lockid_t lid;
    lock_protocol::xid_t xid;
  };

  fifo<client_entry> revokeq;
  fifo<client_entry> retryq;
 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(lock_protocol::lockid_t, std::string id,
	      lock_protocol::xid_t, int &);
  int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
	      int &);
};

#endif
