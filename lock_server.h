// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <pthread.h>
#include <string>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {

 private:
  struct Lock {
    Lock(pthread_mutex_t &mutex) : mutex_(mutex) {
      pthread_cond_init(&cond_, NULL);
    }
    ~Lock() {
      pthread_cond_destroy(&cond_);
    }

    bool islocked() {
      return islocked_;
    }

    void lock() {
      islocked_ = true;
    }

    void unlock() {
      islocked_ = false;
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
    bool islocked_;
  };

  std::map<lock_protocol::lockid_t, Lock *> lockTable_;
  pthread_mutex_t mutex_;
 protected:
  int nacquire;

 public:
  lock_server();
  ~lock_server() {
    pthread_mutex_destroy(&mutex_);
  };
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif







