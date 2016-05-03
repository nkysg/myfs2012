// extent wire protocol

#ifndef extent_protocol_h
#define extent_protocol_h

#include "rpc/rpc.h"

class extent_protocol {
 public:
  typedef unsigned long long extentid_t;
  enum xxstatus { OK, RPCERR, NOENT, IOERR };
  typedef int status;
  enum rpc_numbers {
    put = 0x6001,
    get,
    getattr,
    remove
  };

  struct attr {
    attr() : atime(0), mtime(0), ctime(0), size(0) { }

    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    unsigned int size;
  };
};

inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr &a)
{
  u >> a.atime;
  u >> a.mtime;
  u >> a.ctime;
  u >> a.size;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr a)
{
  m << a.atime;
  m << a.mtime;
  m << a.ctime;
  m << a.size;
  return m;
}

#endif
