// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "rpc/rpc.h"

class extent_client {
  struct Cache {
    Cache() : dirty(false), removed(false), caching(false) {
      pthread_cond_init(&cond_, NULL);
    }
    ~Cache() {
      pthread_cond_destroy(&cond_);
    }
    void wait(pthread_mutex_t *m) {
      pthread_cond_wait(&cond_, m);
    }
    void notifyall() {
      pthread_cond_broadcast(&cond_);
    }

    extent_protocol::extentid_t eid;
    std::string buf;
    extent_protocol::attr attr;
    bool dirty;
    bool removed;
    bool caching;
    pthread_cond_t cond_;
  };
 private:
  rpcc *cl;

  std::map<extent_protocol::extentid_t, Cache *> cache_;
  pthread_mutex_t mutex_;
 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid,
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid,
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif

