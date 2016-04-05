// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

class extent_server {

  struct DB {
    extent_protocol::extentid_t id_;
    std::string buf_;
    extent_protocol::attr attr_;
  };

 public:
  extent_server();
  ~extent_server() {
    pthread_mutex_destroy(&mutex_);
  }

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);

 private:
  // TODO: we can use smart_ptr to avoid copy in the lock region
  std::map<extent_protocol::extentid_t, struct DB> db_;
  // typedef std::map<extent_protocol::extentid_t, struct DB>::iterator dbIter;
  pthread_mutex_t mutex_;
};

#endif







