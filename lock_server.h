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
  // enum isLocked{
    // kFree = 0,
    // kLocked = 1,
  // };
  // struct ConMutex {
    // ConMutex() {
      // pthread_mutex_init(&mutex_, NULL);
      // pthread_cond_init(&cond_, NULL);
    // }
    // ~ConMutex() {
      // pthread_mutex_destroy(&mutex_);
      // pthread_cond_destroy(&cond_);
    // }
    // pthread_mutex_t mutex_;
    // pthread_cond_t cond_;
  // };

  struct Mutex {
    Mutex() {
      pthread_mutex_init(&mutex_, NULL);
      islocked_ = false;
    }
    ~Mutex() {
      pthread_mutex_destroy(&mutex_);
    }
    void Lock() {
      pthread_mutex_lock(&mutex_);
      islocked_ = true;
    }
    void Unlock() {
      islocked_ = false;
      pthread_mutex_unlock(&mutex_);
    }
    pthread_mutex_t mutex_;
    bool islocked_;
  };
  std::map<lock_protocol::lockid_t, Mutex *> lockTable_;
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







