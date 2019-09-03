#ifndef ns_qjump_h
#define ns_qjump_h

#include "connector.h"
#include "timer-handler.h"

#include <map>

using namespace std;

class QJUMP;
class QJUMP_Value;

class QJUMP_Timer : public TimerHandler {
 
 public:
 QJUMP_Timer(QJUMP_Value *r) : TimerHandler() {
    qjump_value_ = r;
  }

 protected:
  virtual void expire(Event *e);
  QJUMP_Value *qjump_value_;

};

class QJUMP_Value {

 public:
  QJUMP_Value(QJUMP *qjump);
  ~QJUMP_Value();
  void timeout(int);
  int flow_id_;
  long long bucket_size_;
  long long tokens_;
  double time_interval_;
  int queue_len_;
  //  double last_update_time_;
  PacketQueue* packet_queue_;
  QJUMP_Timer qjump_timer_;
  int prio_;
  QJUMP *qjump_;

};

typedef map<int, QJUMP_Value*> fid_qjump;
typedef fid_qjump::iterator fid_qjump_itr;
typedef fid_qjump::value_type fid_qjump_val;

class QJUMP : public Connector {

 public:
  int command(int argc, const char*const* argv);

 protected:
  void recv(Packet *, Handler *);
  void activate_fid(int fid, long long bucket, long long time_interval_ns, int qlen, int prio);
  fid_qjump qjumps_;

};

#endif
