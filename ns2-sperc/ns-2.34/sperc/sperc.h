#include <inttypes.h>
#include <math.h>
#include <sstream> // for std::stringstream in log_prefix()
#include <memory> // for std::unique_ptr, std::make_unique
#include "priority.h"
#include "drr.h"
#include "tcp.h"
#include "random.h"
#include "ip.h"
#include <string.h>
#include "delay.h"
#include "sperc-hdrs.h"
#include "sperc-host.h"
#include "timer-handler.h"

class SPERCQueue;

class SPERCQTimer : public TimerHandler {
public:
  SPERCQTimer(SPERCQueue *a, 
	      void (SPERCQueue::*call_back)() ) : a_(a), call_back_(call_back) {};
protected:
  virtual void expire (Event *e);
  SPERCQueue *a_;
  void (SPERCQueue::*call_back_)();
}; 


class SPERCQueue : public Priority {
  friend class SPERCQTimer;

public:
  SPERCQueue();
  int command(int argc, const char*const* argv);

  double get_capacity();
  double get_sperc_capacity();
  double get_bytecnt();
  void log_to_channel(const std::string& str);

protected:
  virtual void enque(Packet* pkt);
  virtual Packet* deque();
  virtual void drop(Packet* pkt);
  inline double now()  { return  Scheduler::instance().clock(); }
  void populate_path_info(Packet* pkt);
  std::string log_prefix();
  int    fromnodeid_;
  int    tonodeid_;
  LinkDelay* ld_;
  Tcl_Channel   channel_;      // Masayoshi
  Tcl_Channel   sperc_channel_;      // Masayoshi

  // variables for estimating capacity
  double running_avg(double var_sample, double var_last_avg, double gain);
  void queue_timeout();

  SPERCQTimer queue_timer_;
  double avg_rtt_;
  double avg_rtt_by_q_[3];
  double act_input_traffic_;
  double act_input_traffic_by_q_[3];
  double act_output_traffic_;
  double act_output_traffic_by_q_[3];

  double alpha_;
  double beta_;
  double actual_capacity_;  
  double queue_threshold_;
  double sperc_capacity_;
  double end_slot_;
  double Tq_;
  double max_sperc_capacity_;
  double min_sperc_capacity_;
}; 


