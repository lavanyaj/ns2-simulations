/* 
 * Author: Rui Zhang and Nandita Dukkipati
 * This file will implement the RCP router.
 * Part of the code is taken from Dina Katabi's XCP implementation.
 */

/* rcp.cc.v4
 * --------
 * R = R * { 1 + (T/rtt)*[alpha*(C - input_tr/T) - beta*Q/rtt]/link_capacity};
 * where the input_tr is measured over a time interval of 'T'
 */

#include <math.h>

#include "red.h"
#include "drop-tail.h"
#include "sperc_priqueue.h"
#include "tcp.h"
#include "random.h"
#include "ip.h"
#include <string.h>
#include "rcp-host.h"

// #define ALPHA 0.4
// #define BETA 0.4
// #define PHI 0.95
// lavanya: commented out, not used
#define PARENT DropTail
//DropTail
// #define INIT_RATE_FACT 0.05 
// lavanya: part of RcpQueue, set init_rtt_ in tcl
// #define RTT 0.2
// lavanya: commented out, not used
// #define INIT_NUMFLOWS 50 
// #define TIMESLOT 0.01
// lavanya: commented out, not used
// #define RTT_GAIN 0.02 //Should be on the order of 1/(number of co-existing flows)

static unsigned int next_router = 0; //Rui: really should be next_queue

class RCPQueue;


class RCPQTimer : public TimerHandler { 
public:
  RCPQTimer(RCPQueue *a, void (RCPQueue::*call_back)() ) : a_(a), call_back_(call_back) {};
protected:
  virtual void expire (Event *e);
  void (RCPQueue::*call_back_)();
  RCPQueue *a_;
}; 



class RCPQueue : public PARENT {
  friend class RCPQTimer;
public:
  RCPQueue();
  int command(int argc, const char*const* argv);
  void queue_timeout();    // timeout every Tq_ for load stats update
  void populate_path_info(Packet *pkt);
  void check_active_flows(Packet *pkt);
protected:
  // Modified functions
  virtual void enque(Packet* pkt);
  virtual Packet* deque();
  virtual void drop(Packet* pkt); // just for logging
  // ------------ Utility Functions
  inline double now()  { return  Scheduler::instance().clock(); }

  double running_avg(double var_sample, double var_last_avg, double gain);
  inline double max(double d1, double d2){ if (d1 > d2){return d1;} else {return d2;} }
  inline double min(double d1, double d2){ if (d1 < d2){return d1;} else {return d2;} }

  // ------------- Estimation & Control Helpers
  void init_vars() {
    routerId_ = next_router++; // Rui: should be queueId_ instead
    link_capacity_ = -1;
    input_traffic_ = 0.0;
    act_input_traffic_ = 0.0;
    output_traffic_ = 0.0;
    last_load_ = 0;
    traffic_spill_ = 0;
    // lavanya: commented out, not used
    // num_flows_ = INIT_NUMFLOWS;
    // lavanya: actually initialized
    // and bound to init_rtt_ in RCPQueue() via
    // binding of init_rtt_
    //avg_rtt_ = 0; // RTT;    
    avg_rtt_ = init_rtt_; // bound to tcl
    this_Tq_rtt_sum_ = 0;
    // this_Tq_rtt_     = 0; // not used
    this_Tq_rtt_numPkts_ = 0;
    input_traffic_rtt_   = 0;
    // lavanya: RTT_GAIN is not used, rtt_moving_gain_
    // initialized based on packets seen during RTT
    // before the first time it's ever used
    // program will exit w err if < 0 (see running_avg)
    rtt_moving_gain_ = -1; // RTT_GAIN;
//    Tq_ = RTT;
//    Tq_ = min(RTT, TIMESLOT);
    Q_ = 0;
    Q_last = 0;
    flow_starts_ = 0;
    flow_exits_ = 0;
  }

  virtual void do_on_packet_arrival(Packet* pkt); // called in enque(), but packet may be dropped
                                                  // used for updating the estimation helping vars 
                                                  // such as counting the offered_load_
  virtual void do_before_packet_departure(Packet* p); // called in deque(), before packet leaves
                                                      // used for writing the feedback in the packet
  virtual void fill_in_feedback(Packet* p); // called in do_before_packet_departure()
                                            // used for writing the feedback in the packet

  inline double packet_time(Packet* pkt);

  //int RCPQueue::timeslot(double time);


 /* ------------ Variables ----------------- */
  int    fromnodeid_; // lavanya, for logging and multipath (sort classifier entries by link head_ (this) . tonodeid)), bound in tcl
  int    tonodeid_; // lavanya, for logging, bound in tcl

  unsigned int    routerId_;
  RCPQTimer        queue_timer_;
  double          Tq_;

  //double RCP_HDR_BYTES_;
  double pkt_size_; // also set by tcl script, so link can cal min_pprtt cutoff
  // Rui: link_capacity_ is set by tcl script.
  // Rui: must call set-link-capacity after building topology and before running simulation
  double link_capacity_;
  double input_traffic_;       // traffic in Tq_
  double act_input_traffic_;
  double output_traffic_;
  double traffic_spill_;  // parts of packets that should fall in next slot
  double last_load_; 
  double end_slot_; // end time of the current time slot
  // lavanya: commented out, not used
  // int num_flows_;
  double avg_rtt_;
  int simple_rtt_update_;
  double this_Tq_rtt_sum_;
  // double this_Tq_rtt_; // not used??
  double this_Tq_rtt_numPkts_;
  int input_traffic_rtt_;
  double rtt_moving_gain_;
  int Q_;
  int Q_last;
  double flow_rate_;
  double min_rate_; // replaces min_pprtt here units are bytes/s
  double alpha_;  // Masayoshi
  double beta_;   // Masayoshi
  //  double gamma_;
  //  double min_pprtt_;   // Masayoshi minimum packet per rtt
  double init_rate_fact_;    // Masayoshi
  //  int    print_status_;      // Masayoshi
  //int    rate_fact_mode_;    // Masayoshi
  //  double fixed_rate_fact_;   // Masayoshi
  //  double propag_rtt_ ;       // Masayoshi (experimental, used with rate_fact_mode_ = 3)
  double upd_timeslot_ ;       // Masayoshi 
  double init_rtt_; // lavanya: initial rtt estimate used in updated before any packet is seen
  int flow_starts_;
  int flow_exits_;
  Tcl_Channel   channel_;      // Masayoshi

  // bool first_fill_in_feedback_;
  bool first_timeout_;

};


/*--------------------- Code -------------------------------------*/

static class RCPClass : public TclClass {
public:
  RCPClass() : TclClass("Queue/DropTail/RCP") {}
  TclObject* create(int, const char*const*) {
    return (new RCPQueue);
  }
} class_rcp_queue;


RCPQueue::RCPQueue(): PARENT(), queue_timer_(this, &RCPQueue::queue_timeout),
		      channel_(NULL), first_timeout_(true) 
		      //, first_fill_in_feedback_(true), 
{
  double T;
  init_vars();
  bind("fromnodeid_", &fromnodeid_);
  bind("tonodeid_", &tonodeid_);

  //bind("Tq_", &timeout_);
  bind("min_rate_", &min_rate_);
  bind("alpha_", &alpha_);  // Masayoshi
  bind("beta_", &beta_);    // Masayoshi
  //  bind("gamma_", &gamma_); 
  //  bind("min_pprtt_", &min_pprtt_);    // Masayoshi
  bind("init_rate_fact_", &init_rate_fact_);    // Masayoshi
  //  bind("print_status_", &print_status_);    // Masayoshi
  //  bind("rate_fact_mode_", &rate_fact_mode_);    // Masayoshi
  //  bind("fixed_rate_fact_", &fixed_rate_fact_);    // Masayoshi
  //  bind("propag_rtt_", &propag_rtt_);    // Masayoshi
  bind("upd_timeslot_", &upd_timeslot_);    // Masayoshi
  bind("init_rtt_", &init_rtt_); // lavanya:
  //  bind("rcp_hdr_bytes_", &RCP_HDR_BYTES_); // lavanya
  bind("packet_size_", &pkt_size_);
  bind("simple_rtt_update_", &simple_rtt_update_);
  avg_rtt_ = init_rtt_; // lavanya:
  Tq_ = min(init_rtt_, upd_timeslot_);  // Tq_ has to be initialized  after binding of upd_timeslot_ 
  if (pkt_size_ == 0) {
    fprintf(stderr, "pkt_size_ from tcl is 0 for RCPQueue.\n");
    exit(1);
  }
  fprintf(stdout, "%f %s %d->%d upd_timeslot_ %f\n", now(), this->name(), fromnodeid_, tonodeid_, upd_timeslot_);
  //  fprintf(stdout,"LOG-RCPQueue: alpha_ %f beta_ %f init_rtt %f Tq_ %f\n",alpha_,beta_, init_rtt_, Tq_);

  // Scheduling queue_timer randommly so that routers are not synchronized
  T = Random::normal(Tq_, 0.2*Tq_);
  //if (T < 0.004) { T = 0.004; } // Not sure why Dina did this...

  end_slot_ = T;
  queue_timer_.sched(T);
}


Packet* RCPQueue::deque()
{
	Packet *p;

	p = PARENT::deque();
	if (p != NULL)
	  do_before_packet_departure(p);

	// if(channel_ != NULL){
	//   char buf[1024];
	//   sprintf(buf, "DEQUEUE %s %d %d %f\n",this->name(),now(), length(), byteLength());
	//   (void)Tcl_Write(channel_, buf, strlen(buf));
	// }

	return (p);
}

void RCPQueue::populate_path_info(Packet* pkt) {
  hdr_rcp *hdr = hdr_rcp::access(pkt);
  // Fill in fromnode_ and increment hop here??
  if (hdr->is_fwd()) {
    hdr->fwd_path_[hdr->fwd_hop_] = fromnodeid_;
    hdr->fwd_hop_++;
  } else {
    hdr->rev_path_[hdr->rev_hop_] = fromnodeid_;
    hdr->rev_hop_++;
  }
}

void RCPQueue::enque(Packet* pkt)
{
  populate_path_info(pkt);
  if (hdr_cmn::access(pkt)->ptype() == PT_RCP)
    check_active_flows(pkt);

  do_on_packet_arrival(pkt);

  PARENT::enque(pkt);
  // if(channel_ != NULL){
  //   char buf[1024];
  //   sprintf(buf, "ENQUEUE %s %d %d %f\n",this->name(),now(), length(), byteLength());
  //   (void)Tcl_Write(channel_, buf, strlen(buf));
  // }

}

void RCPQueue::drop(Packet *pkt) {
  hdr_cmn* cmn = hdr_cmn::access(pkt);
  if (cmn->ptype() == PT_RCP) {
    hdr_rcp* hdr = hdr_rcp::access(pkt);
    if (hdr->RCP_pkt_type() == RCP_SYN) {
      fprintf(stderr, "warning! %lf %d %d data dropping RCP_SYN for flow %d\n", 
	      now(), fromnodeid_, tonodeid_, 
	      hdr_ip::access(pkt)->flowid());      
    } else if ((hdr->RCP_pkt_type() == RCP_ACK 
		or hdr->RCP_pkt_type() == RCP_FINACK)
	       and hdr->numpkts() == hdr->num_dataPkts_received ) {
      fprintf(stderr, "warning! %lf %d %d data dropping %s (all %d acked) for flow %d\n", 
	      now(), fromnodeid_, tonodeid_, 
	      RCP_PKT_T_STR[hdr->RCP_pkt_type()],
	      hdr->numpkts(),
	      hdr_ip::access(pkt)->flowid());
    }
  }
  PARENT::drop(pkt);
}

void RCPQueue::check_active_flows(Packet* pkt) {
  // assuming these start / end packets are never dropped
  // TODO: log if they ever are dropped
  
  hdr_rcp* hdr = hdr_rcp::access(pkt);
  if (hdr->RCP_pkt_type() == RCP_SYN) {
    flow_starts_++;
    fprintf(stdout, "ACTIVE_FLOWS: %lf %d %d data + %d %d\n", 
	    now(), fromnodeid_, tonodeid_, 
	    (flow_starts_), hdr_ip::access(pkt)->flowid());
    
  } else if ((hdr->RCP_pkt_type() == RCP_ACK 
	      or hdr->RCP_pkt_type() == RCP_FINACK)
	     and hdr->numpkts() == hdr->num_dataPkts_received ) {
    flow_exits_++;
    fprintf(stdout, "ACTIVE_FLOWS: %lf %d %d data - %d %d\n", 
	    now(), fromnodeid_, tonodeid_, 
	    (-1 * flow_exits_), hdr_ip::access(pkt)->flowid());
  }

}

void RCPQueue::do_on_packet_arrival(Packet* pkt){
  // Taking input traffic statistics
  int size = hdr_cmn::access(pkt)->size();
  hdr_rcp * hdr = hdr_rcp::access(pkt);
  double pkt_time_ = packet_time(pkt);
  double end_time = now() + pkt_time_;
  double part1, part2;

  // update avg_rtt_ here
  double this_rtt = hdr->rtt();

  // we're setting RTT from all ACKs 
  //  and REF/ REFACKs to -1
  // ACKs because they carry RTT from flow
  // that's using the reverse link
  // REFs because they're just here to pick up
  // rates (ideally they're so small
  // that they don't contribute to queues/ arrival)
  if (this_rtt > 0) {
    if (this_rtt < 0.000010) {
      fprintf(stderr, "Saw packet with rtt < 10us.\n");
    }
    if (!simple_rtt_update_) {
      this_Tq_rtt_sum_ += (this_rtt * size);
      input_traffic_rtt_ += size;
    } else {
      avg_rtt_ = running_avg(this_rtt, avg_rtt_, 0.5);
    }
       // this_Tq_rtt_ = running_avg(this_rtt, this_Tq_rtt_, flow_rate_/link_capacity_);    // not used

//     rtt_moving_gain_ = flow_rate_/link_capacity_;
//     avg_rtt_ = running_avg(this_rtt, avg_rtt_, rtt_moving_gain_);
  }

  // Switches will take into account arrival
  // rate of all packets -SYN, FIN, DATA, ACK, REF, REFACK, FINACK, SYNACK etc.
  if (end_time <= end_slot_)
    act_input_traffic_ += size;
  else {
    part2 = size * (end_time - end_slot_)/pkt_time_;
    part1 = size - part2;
    if (part2 <= 0 or part1 <= 0) {
      fprintf(stderr, "parts of pkt have negative sizes part1 %f part2 %f.\n", part1, part2);
      exit(1);
    }
    act_input_traffic_ += part1;
    traffic_spill_ += part2;
  }

  // Can do some measurement of queue length here
  // length() in packets and byteLength() in bytes

  /* Can read the flow size from a last packet here */
}


void RCPQueue::do_before_packet_departure(Packet* p){
  hdr_rcp * hdr = hdr_rcp::access(p);
  int size = hdr_cmn::access(p)->size();
  output_traffic_ += size;  
  
  if ( hdr->RCP_pkt_type() == RCP_SYN )
  {
//	  num_flows_++;
	  fill_in_feedback(p);
  }
  else if (hdr->RCP_pkt_type() == RCP_FIN )
  {
//	  num_flows_--;
  }
  else if (hdr->RCP_pkt_type() == RCP_DATA )
  {
	  fill_in_feedback(p);
  } else if (hdr->RCP_pkt_type() == RCP_REF )
  {
      fill_in_feedback(p);
 }
		  
}


void RCPQueue::queue_timeout()
{
  double temp;
  double datarate_fact;
  double estN1;
  double estN2;
  int Q_pkts;
  char clip;
  int Q_target_;

  double ratio;
  double input_traffic_devider_;
  double queueing_delay_;

  double virtual_link_capacity; // bytes per second
  
  last_load_ = act_input_traffic_/Tq_; // bytes per second

  Q_ = byteLength();
  Q_pkts = length();
 
  input_traffic_ = last_load_;
  // lav: this is the weighted average of RTTs seen in this interval
  if (!simple_rtt_update_) {
    if (input_traffic_rtt_ > 0)
      this_Tq_rtt_numPkts_ = this_Tq_rtt_sum_/input_traffic_rtt_; 
    if (this_Tq_rtt_numPkts_ >= avg_rtt_)
      rtt_moving_gain_ = (Tq_/avg_rtt_);
    else 
      rtt_moving_gain_ = (flow_rate_/link_capacity_)*(this_Tq_rtt_numPkts_/avg_rtt_)*(Tq_/avg_rtt_);
    avg_rtt_ = running_avg(this_Tq_rtt_numPkts_, avg_rtt_, rtt_moving_gain_);
  }

  estN1 = input_traffic_ / flow_rate_;
  estN2 = link_capacity_ / flow_rate_;

  //  if ( rate_fact_mode_ == 0) { // Masayoshi .. for Nandita's RCP

   virtual_link_capacity = 1 * link_capacity_;

    /* Estimate # of active flows with  estN2 = (link_capacity_/flow_rate_) */
   ratio = (1 + ((Tq_/avg_rtt_)*(alpha_*(virtual_link_capacity - input_traffic_) - beta_*(Q_/avg_rtt_)))/virtual_link_capacity);
    temp = flow_rate_ * ratio;

    //  } // lavanya: removed implementations for other rate_fact_mode_

  double old_flow_rate = flow_rate_;
  //  if ( rate_fact_mode_ != 4) { // Masayoshi .. Experimental
    // if (temp < min_pprtt_ * (pkt_size_/avg_rtt_) ){     // Masayoshi
    //   flow_rate_ = min_pprtt_ * (pkt_size_/avg_rtt_) ; // min pkt per rtt 
    //   clip  = 'L';
    // } 
    if (temp < min_rate_) {
      flow_rate_ = min_rate_; // just so it's not 0
      clip = 'L';
    } else if (temp > virtual_link_capacity){
      flow_rate_ = virtual_link_capacity;
      clip = 'U';
    } else {
      flow_rate_ = temp;
      clip = 'M';
    }
// } else{
//     if (temp < 16000.0 ){    // Masayoshi 16 KB/sec = 128 Kbps
//       flow_rate_ = 16000.0;
//       clip  = 'L';
//     } else if (temp > link_capacity_){
//       flow_rate_ = link_capacity_;
//       clip = 'U';
//     } else {
//       flow_rate_ = temp;
//       clip = 'M';
//     }
//   }

  // if (first_timeout_) {
  //   fprintf(stdout,
  // 	    "%f %s %d->%d queue_timeout() flow_rate_ changed from %f to %f clip %c\n", 
  // 	    now(), this->name(), fromnodeid_, tonodeid_, old_flow_rate, flow_rate_, clip);
  //   first_timeout_ = false;
  // }

//  else if (temp < 0 )  
//	 flow_rate_ = 1000/avg_rtt_; // 1 pkt per rtt 

  datarate_fact = flow_rate_/link_capacity_;

  if (channel_ != NULL && first_timeout_) {
    char buf[8192];
    sprintf(buf, "QUEUE2_TIMEOUT %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n", \
	    "name", "queue", "now()", "Q_", "Q_pkts", "datarate_fact", "flow_rate_", "link_capacity_", "last_load_",
	    "avg_rtt_","cap_minus_input__by_cap", "q_by_rtt_by_cap",
	    "ratio","estN1","estN2","clip", "upd_timeslot_", "next_upd_in");
    (void)Tcl_Write(channel_, buf, strlen(buf));	 
    first_timeout_=false;
  }

  if(channel_ != NULL){
    double next_upd_in = min(avg_rtt_, upd_timeslot_);
    char buf[8192];
    // lavanya: QUEUE2 so it's the same format as SPERC queues, where 1: ctrl, 2: data
    // 1: QUEUE2_TIMEOUT 2: this->name() 3: fromnofrid_, tonodeid_ 4: now() 5: Q_ 6: Q_pkts, 7: datarate_fact, 8: flow_rate_, 9: link_capacity_, 10: last_load_,
       // 11: avg_rtt_, 12: (link_capacity_ - input_traffic_)/link_capacity_, 13: (Q_/avg_rtt_)/link_capacity_,
    sprintf(buf, "QUEUE2_TIMEOUT %s %d->%d %f %d %d %.10lf %.10lf %.10lf %f %f %f %f %f %f %f %c %f %f\n", 
	    this->name(), fromnodeid_, tonodeid_, now(), Q_, Q_pkts, datarate_fact, flow_rate_, link_capacity_, last_load_,
	    avg_rtt_,(link_capacity_ - input_traffic_)/link_capacity_, (Q_/avg_rtt_)/link_capacity_,
	    ratio,estN1,estN2,clip, upd_timeslot_, next_upd_in);
    (void)Tcl_Write(channel_, buf, strlen(buf));
  }

// fflush(stdout);
  if (avg_rtt_ <= 0) {
    fprintf(stderr, "avg_rtt_ <= 0 %f\n", avg_rtt_);
    exit(1);
  }
  Tq_ = min(avg_rtt_, upd_timeslot_);

  // this_Tq_rtt_ = 0; not used
  this_Tq_rtt_sum_ = 0;
  input_traffic_rtt_ = 0;
  Q_last = Q_;
  act_input_traffic_ = traffic_spill_;
  traffic_spill_ = 0;  
  output_traffic_ = 0.0;
  end_slot_ = now() + Tq_;

  queue_timer_.resched(Tq_);
  //queue_timer_.resched(Tq_);
  //queue_timer_.resched(init_rtt_);
}


/* Rui: current scheme:  */
void RCPQueue::fill_in_feedback(Packet* p){

  hdr_rcp * hdr = hdr_rcp::access(p);
  double request = hdr->RCP_request_rate();
  
  // update avg_rtt_ here
  // double this_rtt = hdr->rtt();
  
  /*
  if (this_rtt > 0) {
    avg_rtt_ = running_avg(this_rtt, avg_rtt_, RTT_GAIN);
  }
  */

  if (request < 0 || request > flow_rate_) {
    // if (first_fill_in_feedback_) {      
    //   fprintf(stdout,
    // 	      "%f %s %d->%d fill_in_feedback() given request %f fill in flow_rate_ %f\n", 
    // 	      now(), this->name(), fromnodeid_, tonodeid_, request, flow_rate_);
    //   first_fill_in_feedback_ = false;
    // }
    hdr->set_RCP_rate(flow_rate_);
  }
}

int RCPQueue::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();

  if (argc == 2) {
    ;
  }
  else if (argc == 3) {
    if (strcmp(argv[1], "set-link-capacity") == 0) {
      link_capacity_ = strtod(argv[2],0);
      if (link_capacity_ < 0.0) {printf("Error: BW < 0\n"); abort();}
      // Rui: Link capacity is in bytes.
      flow_rate_ = link_capacity_ * init_rate_fact_;
      fprintf(stdout, "set-link-capacity: setting link_capacity_ to %f Bytes/s, flow_rate_ to %f Bytes/s\n", link_capacity_, flow_rate_);
      // lavanya: removed RED parent code
      return (TCL_OK);

    } 
    /*else if (strcmp(argv[1], "set-rate-fact-mode") == 0) { // Masayoshi
      rate_fact_mode_ = atoi(argv[2]);
      if (rate_fact_mode_ != 0 && rate_fact_mode_ != 1){
	printf("Error: (rcp) rate_fact_mode_ should be 1 or 0\n"); 
      }
      return (TCL_OK);
    } else if (strcmp(argv[1], "set-fixed-rate-fact") == 0) { // Masayoshi
      // lavanya: rate_fact_mode_ is always 0, this is moot
      fixed_rate_fact_ = strtod(argv[2],0);
      if (fixed_rate_fact_ < 0.0 || fixed_rate_fact_ > 1.0){
	printf("Error: (rcp) fixed_rate_fact_ < 0 or >1.0\n"); 
	abort();
      }
      return (TCL_OK);
      } 
    else if (strcmp(argv[1], "set-flow-rate") == 0) { // Masayoshi
      flow_rate_ = strtod(argv[2],0);
      fprintf(stdout, "set-flow-rate: link_capacity_ is %f Bytes/s, setting flow_rate_ to %f Bytes/s\n", link_capacity_, flow_rate_);

      return (TCL_OK);
      } */
    if (strcmp(argv[1], "attach") == 0) {
      int mode;
      const char* id = argv[2];
      channel_ = Tcl_GetChannel(tcl.interp(),
                                (char*) id, &mode);
      if (channel_ == NULL) {
	tcl.resultf("Tagger (%s): can't attach %s for writing",
		    name(), id);
	return (TCL_ERROR);
      }
      return (TCL_OK);
    }
  }
  return PARENT::command(argc, argv);
}


inline double RCPQueue::packet_time(Packet* pkt){
  return (hdr_cmn::access(pkt)->size()/link_capacity_);
}

void RCPQTimer::expire(Event *) { 
  (*a_.*call_back_)();
}

double RCPQueue::running_avg(double var_sample, double var_last_avg, double gain)
{
	double avg;
	if (gain < 0)
	  exit(3);
	avg = gain * var_sample + ( 1 - gain) * var_last_avg;
	return avg;
}
