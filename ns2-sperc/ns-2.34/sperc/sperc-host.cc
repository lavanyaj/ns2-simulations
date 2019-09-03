#include <stdlib.h>

#include "config.h"
#include "agent.h"
#include "random.h"

#include "sperc-hdrs.h"
#include "sperc-host.h"
#include "sperc-control.h"
#include "ip.h"

static class SPERCAgentClass : public TclClass {
public:
	SPERCAgentClass() : TclClass("Agent/SPERC") {}
	TclObject* create(int, const char*const*) {
		return (new SPERCAgent());
	}
} class_sperc_agent;

// TODO: maybe we'll need to make control packets
// an extension of data packet, so they include both valid
// SPERC and SPERC_CTRL headers
// cuz and Agent by default works with the same kind of
// packet, let that type be PT_SPERC, and we'll just fill
// in the SPERC_CTRL header and change the packet type later

const double  learning_thresh_for_prio_[5] = {0, 14.5, 145, 50, 79};
const double  search_thresh_for_prio_[5] = {0, 14.5, 145, 1288, 2361};

SPERCAgent::SPERCAgent() : 
  Agent(PT_SPERC), 
  nodeid_(-1),
  init_log_(1),
  start_time_(-1),
  last_rate_(0),
  last_rate_change_time_(0),
  next_timeout_(0),
  bytes_scheduled_(0),
  bytes_sent_(0),
  zero_gbps_(0.0001),
  rtt_(0),
  min_rtt_(-1),
  max_rtt_(-1),
  ctrl_rtt_(0),
  min_ctrl_rtt_(-1),
  max_ctrl_rtt_(-1),
  seqno_(0),
  ctrl_seqno_(0),
  syn_seqno_(0),
  SPERC_state(SPERC_INACT),
  size_(0),
  send_ctrl_fin_(false),  
  stop_requested_(false),
  sperc_rate_limiting_(SPERC_RATE_LIMITED),
  numpkts_(0),
  num_sent_(0),
  num_Pkts_to_retransmit_(0),
  num_pkts_resent_(0),
  num_enter_syn_retransmit_mode_(0),
  num_enter_retransmit_mode_(0),
  num_dataPkts_acked_by_receiver_(0),
  num_dataPkts_received_(0),
  ctrl_packet_interval_(-1),
  lastctrlpkttime_(-1e6),
  nextctrlpacket_(NULL),
  num_ctrl_packets_sent_(0),
  num_ctrl_packets_recvd_(0),
  lastpkttime_(-1e6), 
  sperc_data_interval_(-1),
  jitter_timer_(this, &SPERCAgent::send_first_packet),
  sperc_ctrl_timer_(this, &SPERCAgent::ctrl_timeout),
  sperc_timer_(this, &SPERCAgent::timeout),
  syn_retransmit_timer_(this, &SPERCAgent::syn_retransmit_timeout), 
  rto_timer_(this, &SPERCAgent::retrans_timeout), 
  log_timer_(this, &SPERCAgent::log_rates_periodic),
  channel_(NULL),
  common_channel_(NULL),
  first_log_rate_change_(true),
  first_log_recv_ack_(true),
  first_log_rate_periodic_(true),
  weight_(1),
  prio_(0),
  first_ack_(true),
  SPERC_SYN_RETX_PERIOD_SECONDS_(0),
  SPERC_FIXED_RTT_ESTIMATE_AT_HOST_(0),
  SPERC_CONTROL_PKT_HDR_BYTES_(0),
  SPERC_DATA_PKT_HDR_BYTES_(0),
  SPERC_INIT_SPERC_DATA_INTERVAL_(0),
  MATCH_DEMAND_TO_FLOW_SIZE_(0),
  SPERC_PRIO_WORKLOAD_(0),
  SPERC_PRIO_THRESH_INDEX_(0),
  SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES_(0),
  SPERC_JITTER_(0)
{
  //   SPERC_LINE_RATE_(0) is a member of control agent
  bind("seqno_", &seqno_);
  // size of a data packet on the wire inc IP/ SPERC header
  bind("PKTSIZE_", &size_); 
  /* numpkts_ has file size, need not be an integer */
  bind("numpkts_",&numpkts_);
  bind("fid_",&fid_);
  bind("nodeid_", &nodeid_);

  bind("SPERC_SYN_RETX_PERIOD_SECONDS", &SPERC_SYN_RETX_PERIOD_SECONDS_);
  bind("SPERC_FIXED_RTT_ESTIMATE_AT_HOST", &SPERC_FIXED_RTT_ESTIMATE_AT_HOST_);
  bind("SPERC_CONTROL_PKT_HDR_BYTES", &SPERC_CONTROL_PKT_HDR_BYTES_);
  bind("SPERC_DATA_PKT_HDR_BYTES", &SPERC_DATA_PKT_HDR_BYTES_);
  bind("SPERC_INIT_SPERC_DATA_INTERVAL", &SPERC_INIT_SPERC_DATA_INTERVAL_);
  bind("MATCH_DEMAND_TO_FLOW_SIZE", &MATCH_DEMAND_TO_FLOW_SIZE_);
  bind("SPERC_PRIO_WORKLOAD", &SPERC_PRIO_WORKLOAD_);
  bind("SPERC_PRIO_THRESH_INDEX", &SPERC_PRIO_THRESH_INDEX_);
  bind("SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES", &SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES_);
  bind("SPERC_JITTER", &SPERC_JITTER_);
  bind("SPERC_LINE_RATE", &SPERC_LINE_RATE_);

  // wait for N(SPERC_JITTER_, ..) seconds before sending first packet
  jitter_.set_seed(100 + 10);
  jitter_.reset_start_substream();

  if (SPERC_DATA_PKT_HDR_BYTES_ == 0 
      or SPERC_CONTROL_PKT_HDR_BYTES_ == 0) {
    fprintf(stderr, 
	    "Error!! SPERCHost(nodeid_ %d): invalid header sizes from tcl data %u ctrl %u\n",
	    nodeid_,
	    SPERC_DATA_PKT_HDR_BYTES_, 
	    SPERC_CONTROL_PKT_HDR_BYTES_);
    exit(1);
  } else if (init_log_ and nodeid_ == 0) {
    fprintf(stdout, 
  	    "SPERCHost(nodeid_ %d): using header sizes from tcl data %u ctrl %u (C++ data %d ctrl %d)\n", nodeid_,
  	    SPERC_DATA_PKT_HDR_BYTES_, 
  	    SPERC_CONTROL_PKT_HDR_BYTES_,
  	    SPERC_DATA_PKT_HDR_BYTES_, 
  	    SPERC_CONTROL_PKT_HDR_BYTES_);
    init_log_ = false;
  }
  
  if (size_ <= SPERC_DATA_PKT_HDR_BYTES_) {
    fprintf(stderr, 
	    "Error!! Requested size %d too small for headers %u\n", 
	    size_, SPERC_DATA_PKT_HDR_BYTES_);
    exit(1);
  }

	  
  sprintf(pathid_, "-1");

/*#ifdef LAVANYA_DEBUG
	fprintf(stdout, "%s : constructor() numpkts_ %d\n", log_prefix().c_str(), numpkts_);
#endif*/ // LAVANYA_DEBUG
	
}

double SPERCAgent::interval() {
  if (using_sperc_rate()) {	    
    double i1 = ((sperc_data_interval_ / weight_)) ;
 
    double line_rate = SPERC_LINE_RATE_*1e9;
    if ( i1> 0 and (size_*8)/i1 > (line_rate)*1.01) {
      double rate = (size_*8)/i1; 
      fprintf(stderr, "Rate exceeds line_rate_ %f v/s %f\n", rate, line_rate);
      exit(1);
    }
    
    return i1;
  }  else if (using_init_rate() or using_init_rate_until_first_sperc_rate()) {
    return SPERC_INIT_SPERC_DATA_INTERVAL_;
  } else {
    fprintf(stderr, "Error!! error with sperc_rate_limiting_ config.\n");
    exit(1);
  }
}

 std::string SPERCAgent::log_prefix() {
  std::stringstream ss;
  ss << "SPERCAgent fid_ " << fid_ << ", nodeid() " << nodeid() 
     << ", time " << ((double) Scheduler::instance().clock()*1e6) 
     << ", name " << this->name() << ", numpkts_ " << numpkts_;
  return ss.str();
}


void SPERCAgent::start()
{
  // called on reciept of data SYNACK or ctrl SYNREV if we have a usable rate
  SPERC_state = SPERC_RUNNING;
  Tcl::instance().evalf("%s begin-datasend", this->name()); 
  
  // timeout should be later than the above SPERC_state change.
  // This is because if numpkts_ is one (1 pkt file transfer),
  // the above state change may harmfully overwrite
  // the change to SPERC_FINSENT in timeout().
  timeout();  
}

void SPERCAgent::stop_data()
{
  sperc_timer_.force_cancel();
  next_timeout_ = 0;
  rto_timer_.force_cancel();
  Tcl::instance().evalf("%s stop-data %d", this->name(), 
			num_enter_retransmit_mode_);

  if (min_rtt_ > 0 and max_rtt_ > 1000 * min_rtt_) {
    fprintf(stdout, "warning! %s: min_rtt_ %g max_rtt_ %g\n",
	    log_prefix().c_str(), min_rtt_, max_rtt_);    
  }

  // if we were using 
  // init_rate_until_first_sperc_rate need to
  // finish control packet flow cleanly

  if (using_init_rate_until_first_sperc_rate()) {
    fprintf(stderr, "Warning!! %s: stop_data() called while using init_rate_until_first_sperc_rate(), will wait to wrap up ctrl final handshake before finish.\n", log_prefix().c_str());
  }
  if (using_init_rate())
    finish();
}

void SPERCAgent::stop_ctrl()
{
  sperc_ctrl_timer_.force_cancel();
  Tcl::instance().evalf("%s stop-ctrl", this->name());

  if (min_rtt_ > 0 and max_rtt_ > 100 * min_rtt_) {
    fprintf(stdout, 
	    "warning! %s: min_ctrl_rtt_ %g max_ctrl_rtt_ %g\n",
	    log_prefix().c_str(), min_ctrl_rtt_, max_ctrl_rtt_);    
  }
  finish();
}


/* Nandita: SPERC_desired_rate sets the senders initial desired rate
 */
double SPERCAgent::SPERC_desired_rate()
{
	double SPERC_rate_ = -1; 
	return(SPERC_rate_);
}

// returns prio() to use for flow, if flow is short enough then 1 else 0.
int SPERCAgent::get_prio() {
  // 11.6us * 100 Gb/s = 145KB (96.67 * 1.5KB)
  // 10.8us * 100 Gb/s =  135KB (90 * 1.5KB)
  // search 0.5% of bytes in 40% of flows max flow size 50KB (on wire)
  // search 1% of bytes in 53% of flows max flow size 79KB (on wire)
  // learning 0.5% of bytes in 92% of flows max flow size 1288KB (on wire)
  // learning 1% of bytes in 94% of flows max flows size 2361KB (on wire)
  // pick one of the following (independent) thresholds  

  bool learning_workload = (SPERC_PRIO_WORKLOAD_ == 1);
  int num_thresh = 0;
  if (learning_workload) num_thresh = sizeof(learning_thresh_for_prio_) / sizeof(double);
  else num_thresh = sizeof(search_thresh_for_prio_) / sizeof(double);
  int thresh_index = SPERC_PRIO_THRESH_INDEX_;
  if (thresh_index < 0 or thresh_index >= num_thresh) {
    fprintf(stderr, "Error!! invalid threshold for marking priority packets %d [0-%u)\n", 
	    thresh_index, num_thresh);
    exit(1);
  }
  double size_kilobytes = (numpkts_*size_)/1000.0;
  if (learning_workload) {
    if (size_kilobytes <  learning_thresh_for_prio_[thresh_index]) return 1; else return 0;
  } else {
    if (size_kilobytes <  search_thresh_for_prio_[thresh_index]) return 1; else return 0;
  }
}

double SPERCAgent::get_weight() {
  // re-purpose WEIGHTED_MAX_MIN_NUM_CLASSES_ = number of priorities
  // PRIO_WORKLOAD_ = which workload (learning 1 or search 2)
  double num_weights = SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES_;
  bool learning_workload = (SPERC_PRIO_WORKLOAD_ == 1);
  double weight = 1;
  if (not ((num_weights == 1 or num_weights == 2 or num_weights == 4) \
	   and (SPERC_PRIO_WORKLOAD_ == 1 \
		or SPERC_PRIO_WORKLOAD_ == 2 or \
		SPERC_PRIO_WORKLOAD_ == 0))) {
    fprintf(stderr, 
	    "Error! invalid config for max-min weights num_weights (1/2/4) %f workload %f (0/1/2)",
	    num_weights, SPERC_PRIO_WORKLOAD_);
    exit(1);
  }

  double learning_thresh[] = {89881, 534582, 780926}; // kilobytes 25thpc, 50thpc, 75thpc (by bytes)
  double search_thresh[] = {3809, 7712, 17451, 29196}; // kilobytes 25thpc, 50thpc, 75thpc (by bytes)
  double num_kbytes = (numpkts_*size_)/1000;
  
  if (num_weights == 1) {
    weight = 1;
  } else if (num_weights == 2) {
    if (learning_workload) {
      if (num_kbytes < learning_thresh[1]) {
	weight = 4;
      } else {
	weight = 1;
      }
    } else {
      if (num_kbytes < search_thresh[1]) {
	weight = 4;
      } else {
	weight = 1;
      }
    } // if learning workload
  } else {        
    // num_weights = 4
    if (learning_workload) {
      if (num_kbytes < learning_thresh[0]) {
	weight = 32;
      }  else if (num_kbytes < learning_thresh[1]) {
	weight = 16;
      } else if (num_kbytes < learning_thresh[2]) {
	weight = 4;
      } else {
	weight = 1;
      }
    } else { 
      if (num_kbytes < search_thresh[0]) {
	weight = 32;
      }  else if (num_kbytes < search_thresh[1]) {
	weight = 16;
      } else if (num_kbytes < search_thresh[2]) {
	weight = 4;
      } else {
	weight = 1;
      }
    }  // if learning workload
  } // if num_weights == 1
  return weight;
}


void SPERCAgent::sendfile()
{
  if (SPERC_FIXED_RTT_ESTIMATE_AT_HOST_ <= 0) {
    fprintf(stderr, "Error!! SPERC_FIXED_RTT_ESTIMATE_AT_HOST_ < 0");
    exit(1);
  }
  prio_ = get_prio();
  weight_ = get_weight();

  sperc_rate_limiting_ = SPERC_RATE_LIMITED;
  // Only start short flows at high rate (should work when bytes contributed by short flows is small) 
  if (SPERC_INIT_SPERC_DATA_INTERVAL_ > 0 and prio_ == 1) {
    sperc_rate_limiting_ = INIT_RATE_LIMITED_UNTIL_FIRST_SPERC_RATE;
    // if we can send all packets in this RTT, don't even send out control packets to reserve bandwidth
    double numpkts_this_rtt = std::round(SPERC_FIXED_RTT_ESTIMATE_AT_HOST_/SPERC_INIT_SPERC_DATA_INTERVAL_);
    if (numpkts_+1 < numpkts_this_rtt) sperc_rate_limiting_ = INIT_RATE_LIMITED;
  }
  start_time_ = Scheduler::instance().clock();

  // lavanya: sending first control packet:
  double jitter = jitter_.normal(SPERC_JITTER_, SPERC_JITTER_*0.1);
  if (jitter < 0) jitter = 0;
  jitter_timer_.resched(jitter);
  double now = Scheduler::instance().clock();
  int now_us = int(now * 1e6);
  // round to nearest 20us
  int rem = now_us % 20;
  if (rem < 10)  now_us -= rem;
  else now_us += (20 - rem);
  double next_log_time = ( (double) now_us+20) / 1e6;
  log_timer_.resched((next_log_time-now));
// #ifdef LAVANYA_DEBUG
//   fprintf(stdout, "%s: Scheduling log_rates_periodic for %f sec, now %f sec\n", 
//       log_prefix().c_str(), next_log_time, now); 
// #endif
}

void SPERCAgent::send_first_packet() {
  jitter_timer_.force_cancel();
  if (using_sperc_rate() || using_init_rate_until_first_sperc_rate()) {
    // Agent::allocpkt() by default allocates a generic packet
    // fills in common, ip header fields, inits header flags
    // sets up type_ to be PT_SPERC (Agent::type_) and size_ to be 0    
    Packet* cp = allocpkt();
    populate_SPERC_cmn_ctrl(cp, NULL);
    populate_SPERC_base_ctrl(cp, NULL);
    populate_SPERC_ctrl(cp, NULL); // by default SPERC_CTRL_SYN    
    if (weight_ <= 0) {
      fprintf(stderr, "Error!! %s: sending syn but weight_ <= 0 %f min_pkts %f numpkts %u\n",
	      log_prefix().c_str(), weight_, SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES_, numpkts_);
      exit(1);
    }
    hdr_sperc_ctrl::access(cp)->weight() = weight_;

    if (match_demand_to_flow_size()) {
      int arg_host = MAX_HOPS-1;
      unsigned num_pkts_left =  numpkts_;
      if (SPERC_FIXED_RTT_ESTIMATE_AT_HOST_ <= 0 or weight_ <= 0 or num_pkts_left > (1<<30)) {
	fprintf(stderr, "Error!! %s: SPERC_FIXED_RTT_ESTIMATE_AT_HOST_/ weight <= 0 or num_pkts_left %d\n", 
		log_prefix().c_str(), num_pkts_left);
	exit(1);
      }
      // If there's only numpkts_ left, no point in asking for a rate more
      // than numpkts_ / RTT, since we only have so much to send until the next
      // control packet gets back after 1 RTT. Erring on side of smaller RTT
      // and bigger rate. Unit of bottleneck rate is Gb/s.
      double max_rate = (((numpkts_+1)*size_*8)/SPERC_FIXED_RTT_ESTIMATE_AT_HOST_)/1e9;
      //double max_rate = (((num_pkts_left+1)*size_*8)/10.8e-6)/1e9;
      max_rate = max_rate / weight_;
      hdr_sperc_ctrl::access(cp)->set_allocations(arg_host, max_rate);
      hdr_sperc_ctrl::access(cp)->set_bottleneck_rate(arg_host, max_rate);
      hdr_sperc_ctrl::access(cp)->clear_ignore_bit(arg_host);
    }

    populate_route_new(cp, NULL);
    
    // sh1->set_SPERC_pkt_type(SPERC_CTRL_SYN);	
    // Send control packet and record send time
    lastctrlpkttime_ =  Scheduler::instance().clock();
    target_->recv(cp, (Handler*)0);	
    num_ctrl_packets_sent_++;
    nextctrlpacket_ = NULL;
    Tcl::instance().evalf("%s ctrl-syn-sent", this->name());
  }
 
  if (using_sperc_rate()) {
    // Sending SYN data packet: 
    Packet* p = allocpkt();
    populate_SPERC_cmn_data_header(p);
    populate_SPERC_base_data(p, NULL, SPERC_SYN);
    populate_route_new(p, NULL);
    bytes_sent_ += hdr_cmn::access(p)->size();  
    target_->recv(p, (Handler*)0);
    // Change state of sender: Sender is in listening state 
    SPERC_state = SPERC_SYNSENT;
    syn_retransmit_timer_.resched(SPERC_SYN_RETX_PERIOD_SECONDS_);
    lastpkttime_ =  Scheduler::instance().clock();
    Tcl::instance().evalf("%s syn-sent", this->name());
  } else if (using_init_rate() or using_init_rate_until_first_sperc_rate()) {
    //SPERC_state doesn't matter start() will change to SPERC_RUNNING
    min_rtt_ = -1; // who uses min_rtt_ ?
    if (rtt_ <= 0) rtt_ = SPERC_FIXED_RTT_ESTIMATE_AT_HOST_;
    start();
  }

}

void SPERCAgent::populate_SPERC_cmn_ctrl(Packet *p, Packet *rp) {
  hdr_cmn *cmnh = hdr_cmn::access(p);
  cmnh->size() = SPERC_CONTROL_PKT_HDR_BYTES_;
  cmnh->ptype() = PT_SPERC_CTRL;	
  hdr_ip *iph = hdr_ip::access(p);
  iph->prio() = 2;
}

// Populate base fields of control packet
void SPERCAgent::populate_SPERC_base_ctrl(Packet *send_pkt, Packet *recv_pkt) {
  bool sending_ack = false;
  SPERC_PKT_T recv_type = SPERC_OTHER;
  SPERC_PKT_T send_type = SPERC_CTRL_SYN;
  if (recv_pkt) {
    recv_type = hdr_sperc::access(recv_pkt)->SPERC_pkt_type();
    send_type = hdr_sperc::get_control_rev_type(recv_type);
    if (send_type == SPERC_CTRL_REV 
      || send_type == SPERC_CTRL_SYN_REV) {
      sending_ack = true;
    }
  }

  hdr_sperc* send_rh = hdr_sperc::access(send_pkt);
  send_rh->set_SPERC_pkt_type(send_type);

  if (sending_ack) {
    // mostly copying all the fields from received packet
    hdr_sperc *recv_rh = hdr_sperc::access(recv_pkt);
    send_rh->seqno() = recv_rh->seqno(); // why?
    send_rh->ts() = recv_rh->ts();
    send_rh->rtt() = recv_rh->rtt();
    // Reset control packet rate, same classifier / link state will update packet
    // on the way back
    send_rh->set_SPERC_rate(SPERC_desired_rate());
    // send_rh->set_SPERC_rate(recv_rh->SPERC_request_rate());
    send_rh->set_num_dataPkts_received(num_dataPkts_received_);;
  } else {
    if (recv_pkt != NULL) {
      ctrl_rtt_ =  (Scheduler::instance().clock() - hdr_sperc::access(recv_pkt)->ts());
      if (min_ctrl_rtt_ == -1 or min_ctrl_rtt_ > ctrl_rtt_) 
	min_ctrl_rtt_ = ctrl_rtt_;
      if (max_ctrl_rtt_ < ctrl_rtt_) max_ctrl_rtt_ = ctrl_rtt_;
    }
    send_rh->ts_ = Scheduler::instance().clock();
    send_rh->rtt_ = ctrl_rtt_;       
    send_rh->set_SPERC_rate(SPERC_desired_rate());
    send_rh->set_num_dataPkts_received(num_dataPkts_received_);
    send_rh->seqno() = ctrl_seqno_++;
  }
}


void SPERCAgent::populate_SPERC_cmn_data_header(Packet *p) {
  hdr_cmn *cmnh = hdr_cmn::access(p);
  cmnh->size() = SPERC_DATA_PKT_HDR_BYTES_;
  cmnh->ptype() = PT_SPERC;	
}

void SPERCAgent::populate_SPERC_cmn_data(Packet *p) {
  hdr_cmn *cmnh = hdr_cmn::access(p);
  cmnh->size() = size_;
  cmnh->ptype() = PT_SPERC;	
  hdr_ip *iph = hdr_ip::access(p);
  iph->prio() = prio_;
}


void SPERCAgent::populate_SPERC_base_data(Packet *send_pkt, Packet *recv_pkt, SPERC_PKT_T type) {
  // Populate base fields of data packet
  hdr_sperc *send_rh = hdr_sperc::access(send_pkt);
  bool sending_ack = false;
  send_rh->set_SPERC_pkt_type(type);

  if (type == SPERC_SYN) {
    send_rh->seqno() = syn_seqno_++; 
    send_rh->set_numpkts(numpkts_);
  } else if (type == SPERC_DATA) {
    send_rh->seqno() = seqno_++;
    send_rh->set_numpkts(numpkts_);
  } else if (type == SPERC_FIN) {
    send_rh->seqno() = seqno_++;
    send_rh->set_numpkts(numpkts_);
  } else if (type == SPERC_FINACK || type == SPERC_SYNACK
      || type == SPERC_ACK) {
    sending_ack = true;
  }

  if (!sending_ack and send_rh->numpkts() > (1 << 30)) {
    fprintf(stderr, 
      "Error!! sending syn/data/fin with invalid numpkts %u (numpkts_ %u): %s\n",
      send_rh->numpkts(), numpkts_, hdr_sperc_ctrl::get_string(send_pkt).c_str());
    exit(1);
  }

  
  if (sending_ack) {
    // mostly copying all the fields from received packet
    hdr_sperc *recv_rh = hdr_sperc::access(recv_pkt);
    if (recv_rh->numpkts() > (1 << 30)) {
      fprintf(stderr, 
	      "Error!! received syn/data/fin with invalid numpkts %d: %s\n",
	      recv_rh->numpkts(), hdr_sperc_ctrl::get_string(recv_pkt).c_str());
      exit(1);
    }
    send_rh->set_numpkts(recv_rh->numpkts());
    send_rh->seqno() = recv_rh->seqno(); // why?
    send_rh->ts() = recv_rh->ts();
    send_rh->rtt() = recv_rh->rtt();
    send_rh->set_SPERC_rate(recv_rh->SPERC_request_rate());
    send_rh->set_num_dataPkts_received(num_dataPkts_received_);;
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout,
	    "%s: populate_SPERC_base_data for %s packet num_dataPkts_received %d in hdr num_dataPkts_received is %d, this is %llu\n",
	    log_prefix().c_str(), 
	    SPERC_PKT_T_STR[type],
	    num_dataPkts_received_,
	    hdr_sperc::access(send_pkt)->num_dataPkts_received(),
	    this);
#endif*/ // LAVANYA_DEBUG
  } else {
    send_rh->ts_ = Scheduler::instance().clock();
    send_rh->rtt_ = rtt_;       
    send_rh->set_SPERC_rate(SPERC_desired_rate());
    send_rh->set_num_dataPkts_received(num_dataPkts_received_);;
  }
}

double SPERCAgent::get_rate_from_remaining_pkts() {
      // what is the maximum rate we can ask for? infinity
      // realistically given that we already have a rate
      // what is the maximum rate we can ask for?
      // we'll send at least num_pkts_until_next_ctrl
      // so we'll have at most num_pkts_left - num_pkts_until_next_ctrl to send
      // the fastest we can send it is ()/min_rtt.

    unsigned num_pkts_left = numpkts_;
    switch (SPERC_state) {
  case SPERC_RUNNING:
  case SPERC_SYNSENT:
  case SPERC_SYNACK_RECVD:  
    if (numpkts_ >= num_sent_) 
      num_pkts_left = numpkts_ - num_sent_;
    else {
      num_pkts_left = 0;
      fprintf(stderr, "Warning!! %s: num_sent_ %u exceeds numpkts_ %u  in state %s, setting num_pkts_left to 0.\n",
	      log_prefix().c_str(), num_sent_, numpkts_, SPERC_HOST_STATE_STR[SPERC_state]);
    }
    break;
  case SPERC_RETRANSMIT:
    if (num_Pkts_to_retransmit_ >= num_pkts_resent_)
      num_pkts_left = num_Pkts_to_retransmit_ - num_pkts_resent_;
    else {
      num_pkts_left = 0;
      fprintf(stderr, "Warning!! %s: num_pkts_resent_ %u exceeds num_Pkts_to_retransmit_ %u  in state %s, setting num_pkts_left to 0.\n",
	      log_prefix().c_str(), num_pkts_resent_, num_Pkts_to_retransmit_, SPERC_HOST_STATE_STR[SPERC_state]);

    }
    break;
  case SPERC_FINSENT:
    num_pkts_left = 0;
    break;
  case SPERC_INACT:
    // just before sending SYN
    num_pkts_left = numpkts_;
    break;
  default:
    fprintf(stderr, "Error!! %s: returning num pkts left = numpkts_ %u in state %s\n",
      log_prefix().c_str(), numpkts_, SPERC_HOST_STATE_STR[SPERC_state]);
    exit(1);
    num_pkts_left = numpkts_;
  }
        

      double curr_rtt = 10.8e-6;
      if (ctrl_rtt_ > 0) curr_rtt = ctrl_rtt_;

      double min_rtt = 10.8e-6;
      //if (min_ctrl_rtt_ > 0) min_rtt = min_ctrl_rtt_;

      double now = Scheduler::instance().clock();
      double next_ctrl_recv = now + min_rtt;
      double curr_interval = interval();      
      double num_pkts_until_next_ctrl = 0;
      // we'll send at least this many packets until we get the next ctrl pkt
      if (curr_interval > 0 
      and next_timeout_ > now 
      and next_ctrl_recv > next_timeout_) {
    num_pkts_until_next_ctrl = std::round((next_ctrl_recv-next_timeout_)/curr_interval);
    if (num_pkts_until_next_ctrl > num_pkts_left) num_pkts_left = 0;
    else num_pkts_left -= num_pkts_until_next_ctrl;
  }

      // after we get the next ctrl pkt, it could be at least min_rtt until
      // we get a new rate, while we'll have at most ^ num pkts
      if (min_rtt <= 0 or curr_rtt <= 0 or weight_ <= 0 or num_pkts_left < 0) {
	fprintf(stderr, "Error!! %s: rtt / weight <= 0 or num_pkts_left < 0\n", 
		log_prefix().c_str());
	exit(1);
      }

      double max_rate = (((num_pkts_left*size_+1)*8)/min_rtt)/1e9;
      return max_rate;
}

void SPERCAgent::ctrl_timeout() 
{
  if (nextctrlpacket_) {
    lastctrlpkttime_ = Scheduler::instance().clock();

    if (match_demand_to_flow_size()) {
      int arg_host = MAX_HOPS-1;
      double max_rate = get_rate_from_remaining_pkts() / weight_;
      hdr_sperc_ctrl::access(nextctrlpacket_)->set_bottleneck_rate(arg_host, max_rate);
      hdr_sperc_ctrl::access(nextctrlpacket_)->clear_ignore_bit(arg_host);
    }

    target_->recv(nextctrlpacket_, (Handler*) 0);
    num_ctrl_packets_sent_++;   
    nextctrlpacket_ = NULL;
  }

  if (ctrl_packet_interval_ > 0)
    sperc_ctrl_timer_.resched(ctrl_packet_interval_);
}

void SPERCAgent::syn_retransmit_timeout() {
  syn_retransmit_timer_.force_cancel();

  if (SPERC_state == SPERC_RUNNING or SPERC_state == SPERC_SYNACK_RECVD) return;
    // nothing to do

  if (!using_sperc_rate()) { // error
    fprintf(stderr, "Error!! in syn_retransmit but sending at init-rate\n");
    exit(1);
  }

  if (SPERC_state == SPERC_SYNSENT) {
    num_enter_syn_retransmit_mode_++;
    // assume SYN was dropped, send again
    // Sending SYN data packet: 
    Packet* p = allocpkt();
    populate_SPERC_cmn_data_header(p);
    populate_SPERC_base_data(p, NULL, SPERC_SYN);
    populate_route_new(p, NULL);  

    bytes_sent_ += hdr_cmn::access(p)->size();
    target_->recv(p, (Handler*)0);
    syn_retransmit_timer_.resched(SPERC_SYN_RETX_PERIOD_SECONDS_);
    lastpkttime_ =  Scheduler::instance().clock();
    Tcl::instance().evalf("%s syn-sent", this->name());
  }
}

void SPERCAgent::timeout() 
{
  bool rto_scheduled = false;
  double curr_interval = interval();
  double now = Scheduler::instance().clock();

  if (SPERC_state == SPERC_RUNNING) {
    if (num_sent_ < numpkts_ - 1) {
      sendpkt();

      // if (fid() == 26 or fid() == 308) {
      // 	fprintf(stderr, "%s: timeout() call sendpkt() numpkts_ %f num_sent_ %d num_dataPkts_acked_by_receiver_ %d interval() %f -> rate %f\n, state %s",
      // 		log_prefix().c_str(), numpkts_, num_sent_, num_dataPkts_acked_by_receiver_,
      // 		interval(), ((size_ * 8)/interval())/1e9, SPERC_HOST_STATE_STR[SPERC_state]);
      // }
      sperc_timer_.resched(curr_interval);
      next_timeout_ = now + curr_interval;
    } else {			
      sendlast();

      // if (fid() == 26 or fid() == 308) {
      // 	fprintf(stderr, "%s: timeout() call sendlast() numpkts_ %f num_sent_ %d num_dataPkts_acked_by_receiver_ %d interval() %f -> rate %f\n, state %s",
      // 		log_prefix().c_str(), numpkts_, num_sent_, num_dataPkts_acked_by_receiver_,
      // 		interval(), ((size_ * 8)/interval())/1e9, SPERC_HOST_STATE_STR[SPERC_state]);
      // }

      Tcl::instance().evalf("%s finish-datasend", this->name());
      SPERC_state = SPERC_FINSENT;			
      sperc_timer_.force_cancel();
      next_timeout_ = 0;
      rto_timer_.resched(2*rtt_);
      rto_scheduled = true;
    }    
  } else if (SPERC_state == SPERC_RETRANSMIT) {
    if (num_pkts_resent_ < num_Pkts_to_retransmit_ - 1) {
      sendpkt();

      // if (fid() == 26 or fid() == 308) {
      // 	fprintf(stderr, "%s: timeout() call sendpkt() numpkts_ %f num_sent_ %d num_dataPkts_acked_by_receiver_ %d interval() %f -> rate %f\n, state %s",
      // 		log_prefix().c_str(), numpkts_, num_sent_, num_dataPkts_acked_by_receiver_,
      // 		interval(), ((size_ * 8)/interval())/1e9, SPERC_HOST_STATE_STR[SPERC_state]);
      // }

      sperc_timer_.resched(curr_interval);
      next_timeout_ = now + curr_interval;
    } else if (num_pkts_resent_ == num_Pkts_to_retransmit_ - 1) {
      sendpkt();

      // if (fid() == 26 or fid() == 308) {
      // 	fprintf(stderr, "%s: timeout() call last sendpkt() numpkts_ %f num_sent_ %d num_dataPkts_acked_by_receiver_ %d interval() %f -> rate %f\n, state %s",
      // 		log_prefix().c_str(), numpkts_, num_sent_, num_dataPkts_acked_by_receiver_,
      // 		interval(), ((size_ * 8)/interval())/1e9, SPERC_HOST_STATE_STR[SPERC_state]);
      // }

      sperc_timer_.force_cancel();
      next_timeout_ = 0;
      rto_timer_.resched(2*rtt_);      
      rto_scheduled = true;
    }
  }

  if (rto_scheduled and ((2*rtt_) > 0.001 or rtt_ <= 0)) {
    double now = Scheduler::instance().clock();
      fprintf(stderr, 
	      "Warning!! %f: fid_ %d long RTO (2*rtt_) cuz rtt_ %f. num_enter_retransmit_mode_ %u num_sent_ %u num_dataPkts_acked_by_receiver_ %u numpkts_ %u\n", 
	      now, fid_, rtt_, num_enter_retransmit_mode_, 
	      num_sent_, num_dataPkts_acked_by_receiver_, numpkts_);
  }
}

void SPERCAgent::retrans_timeout()
{ 
	SPERC_state = SPERC_RETRANSMIT;
	num_enter_retransmit_mode_++;     
	num_pkts_resent_ = 0;
	num_Pkts_to_retransmit_ = numpkts_ - num_dataPkts_acked_by_receiver_;
	double curr_interval = interval();
#ifdef LAVANYA_DEBUG
	if (num_Pkts_to_retransmit_ > 0)
	  fprintf(stdout, 
		  "%s : retrans_timeout() %d, num_Pkts_to_retransmit_ %u, interval %f \n",
		  log_prefix().c_str(),
		  num_enter_retransmit_mode_, num_Pkts_to_retransmit_, curr_interval);
#endif // LAVANYA_DEBUG
	if (num_Pkts_to_retransmit_ > 0) {
	  double now = Scheduler::instance().clock();
	  sperc_timer_.resched(curr_interval);
	  next_timeout_ = now + curr_interval;
	}
}


/*
 * finish() is called when we must stop (either by request or because
 * we're out of packets to send.
 * Tcl will call reset before each sendfile()
 */
void SPERCAgent::finish()
{
	SPERC_state = SPERC_INACT;
        log_timer_.force_cancel();

	Tcl::instance().evalf("%s done %f %f", this->name(), bytes_scheduled_, bytes_sent_);
}


void SPERCAgent::recv(Packet* p, Handler*) {
  if (hdr_cmn::access(p)->ptype() == PT_SPERC_CTRL)
    recv_ctrl(p); 
  else 
    recv_data(p);
}

void SPERCAgent::recv_ctrl(Packet* p) {
  num_ctrl_packets_recvd_++;
  populate_route_recvd(p);
  
  Packet* send_pkt = allocpkt();
  populate_SPERC_cmn_ctrl(send_pkt, p);
  populate_route_new(send_pkt, p);
  populate_SPERC_base_ctrl(send_pkt, p);
  populate_SPERC_ctrl(send_pkt, p); // by default SPERC_CTRL_SYN

  hdr_sperc_ctrl *send_rch = hdr_sperc_ctrl::access(send_pkt);
  send_rch->seqno() = ctrl_seqno_++;	
  hdr_sperc *rh = hdr_sperc::access(p);	
  if (rh->SPERC_pkt_type() == SPERC_CTRL or rh->SPERC_pkt_type() == SPERC_CTRL_SYN) {
    // destination will always echo ctrl packets ASAP
    target_->recv(send_pkt, (Handler*)0);
    num_ctrl_packets_sent_++;	  
  } else if (rh->SPERC_pkt_type() == SPERC_CTRL_SYN_REV 
	     or rh->SPERC_pkt_type() == SPERC_CTRL_REV) {

      if (rh->SPERC_pkt_type() == SPERC_CTRL_SYN_REV) {
	double num_bytes = numpkts_*size_; //1500.0;
	fprintf(stdout, "SPERC_CTRL_SYN_REV recv, sending packet with fid %d num_bytes %f numpkts %u start_time %.12f gid %d %d path %s\n size %d\n", 
		hdr_ip::access(p)->flowid(), num_bytes, numpkts_, 
		start_time_, hdr_sperc::access(p)->fwd_path_[0],
		hdr_sperc::access(p)->rev_path_[0],
		get_pkt_path(p).c_str(), hdr_cmn::access(p)->size());
	fprintf(stdout, "SPERC_CTRL_SYN_REV recv, sending packet with fid %d num_bytes %f numpkts %u start_time %.12f gid %d %d  size %d weight %f prio %d\n", 
		hdr_ip::access(p)->flowid(), num_bytes, numpkts_, 
	       start_time_, hdr_sperc::access(p)->fwd_path_[0], hdr_sperc::access(p)->rev_path_[0],
	       hdr_cmn::access(p)->size(), weight_, prio_);
      }

    log_ack(p);

    double rate = get_rate_from_SPERC_ctrl(p);
    // this will also call update_data_rate

    if (rate <= 0) {
      fprintf(stderr, "Error!! %s : rate from SPERC ctrl is %g\n",
	      log_prefix().c_str(), rate);
      exit(1);
    }


    // If we were waiting for a rate to start sending data, this is it!
    bool waiting_to_start =  (SPERC_state == SPERC_SYNACK_RECVD) &&  
      using_sperc_rate() and (sperc_data_interval_ > 0);
    if (waiting_to_start) start();


    if (weight_ <= 0) {
      fprintf(stderr, "Error!! %s: replying to CTRL_REV but weight  <= 0 %f min_pkts %f numpkts %u\n",
	      log_prefix().c_str(), weight_, SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES_, numpkts_);
      exit(1);
    }

    hdr_sperc_ctrl::access(send_pkt)->weight() = weight_;

  // here we might want to modify request based on flow size
    // only at sender
    //    assert(!match_demand_to_flow_size());
    // if (match_demand_to_flow_size()) {      
    //   modify_ctrl_request(send_pkt);
    // }

    // here we might also update data rate to line rate
    // if it looks like we can finish everything in the next RTT
    // modify_sending_rate();

    /*#ifdef LAVANYA_DEBUG
    fprintf(stdout, "%s : received SPERC_CTRL_REV or SPERC_CTRL_SYN_REV, get sperc_data_rate from pkt %f Gb/s and send new pkt %s\n",
      log_prefix().c_str(), rate, hdr_sperc_ctrl::get_string(send_pkt).c_str());
    #endif*/
	    
    if (num_ctrl_packets_sent_ != num_ctrl_packets_recvd_) { // error      
      fprintf(stderr,
	      "Error!! %s : received (rev) control packet out of turn, num_sent %u, num_recvd %u pkt %s\n",
	      log_prefix().c_str(), num_ctrl_packets_sent_,
	      num_ctrl_packets_recvd_, hdr_sperc_ctrl::get_string(p).c_str());
      nextctrlpacket_ = NULL;
      sperc_ctrl_timer_.force_cancel();
      drop(send_pkt);
      exit(1);
    }

    // To make sure that we don't sent control packets out of turn
    // nextctlpacket_ is set to NULL as soon as it is sent
    // see ctrl_timeout()

    // we're handling the case when we recv SPERC_CTRL_REV so this must be at src
    if (ctrl_fin_sent() and ctrl_fin_rcvd()) {  // stop sending control packets
      /*#ifdef LAVANYA_DEBUG
      fprintf(stdout,
	      "%s : stop sending ctrl packets, num_sent %d, num_recvd %d pkt %s\n",
	      log_prefix().c_str(), num_ctrl_packets_sent_, num_ctrl_packets_recvd_, hdr_sperc_ctrl::get_string(p).c_str());
      #endif*/     
      nextctrlpacket_ = NULL;
      sperc_ctrl_timer_.force_cancel();
      drop(send_pkt);
      stop_ctrl();
    } else { // common case
      if (rh->SPERC_request_rate() <= 0) { // error
	fprintf(stderr, "Error!! %s : asked to update ctrl pkt rate to %f\n",
		log_prefix().c_str(), rh->SPERC_request_rate());	
	exit(1);
      } 

	// this will start ctrl_timeouts or adjust them if they've already started
	// request_rate (of control packets) is also in bytes/s, stamped by links.	
      nextctrlpacket_ = send_pkt;
      // this is where we change sending rate of control packets
      // rate from sperc-control is in Gb/s
      double old_interval = ctrl_packet_interval_;
      double new_interval = (SPERC_CONTROL_PKT_HDR_BYTES_*8)/(rh->SPERC_request_rate()*1.0e9);
      if (old_interval != new_interval) {
	// fprintf(stdout, "new rate of control packet for flow %f is %f Gb/s: %f B @ %f us -> %f B @ %f us",
	// 	hdr_ip::access(send_pkt)->flowid(),
	// 	rh->SPERC_request_rate(),
	// 	hdr_cmn::access(p)->size(),
	// 	old_interval*1e6,
	// 	new_interval*1e6);
	ctrl_packet_interval_ = new_interval;
	log_rates();
	ctrl_rate_change();	
      }
    }
  } else {
    fprintf(stderr, "Error!! Unknown SPERC CTRL packet type!\n");
    exit(1);
  }
  
  Packet::free(p);
}

void SPERCAgent::log_ack(Packet *p) {
  // don't log acks
  return;
  
  if(channel_ != NULL){
    if (first_log_recv_ack_) {
      char header[8192];      
      sprintf(header, "@RECV_ACK fid time(s) ack_type is_exit rtt(us) num_dataPkts_received prio\n");	    
      (void)Tcl_Write(channel_, header, strlen(header));
      first_log_recv_ack_ = false;
    } 
    char buf[8192];
    // "%s : sperc_data %g us %g Mb/s, rcp_data %g us %g Mb/s, rcp_ctrl %g us = %g Mb/s\n"
    hdr_sperc* rh = hdr_sperc::access(p);
    double rtt =  (Scheduler::instance().clock() - rh->ts())*1e6;
    bool is_exit = (hdr_cmn::access(p)->ptype() == PT_SPERC_CTRL && hdr_sperc_ctrl::access(p)->is_exit());    
    int prio = hdr_ip::access(p)->prio();
    sprintf(buf, "@RECV_ACK %d %f %s %d %f %u %d\n",
	    fid_,	       
	    ((double) Scheduler::instance().clock()),
	    SPERC_PKT_T_STR[rh->SPERC_pkt_type()],
	    is_exit,
	    rtt,
	    rh->num_dataPkts_received(),
	    prio);
    
    (void)Tcl_Write(channel_, buf, strlen(buf));
  } // else {
  // TODO: maybe output to STDOUT only if log_stdout_ = true etc.?
  //   hdr_sperc* rh = hdr_sperc::access(p);
  //   double rtt =  (Scheduler::instance().clock() - rh->ts())*1e6;
  //   bool is_exit = (hdr_cmn::access(p)->ptype() == PT_SPERC_CTRL && hdr_sperc_ctrl::access(p)->is_exit());    
  //   fprintf(stdout, "RECV_ACK %d %f %s %d %f %d\n",
  // 	    fid_,	       
  // 	    ((double) Scheduler::instance().clock()),
  // 	    SPERC_PKT_T_STR[rh->SPERC_pkt_type()],
  // 	    is_exit,
  // 	    rtt,
  // 	    rh->num_dataPkts_received());
   
  // }
}

void SPERCAgent::log_rates_periodic() {

  double now = Scheduler::instance().clock();
  int now_us = int(now * 1e6);
  // round to nearest 20us
  int rem = now_us % 20;
  if (rem < 10)  now_us -= rem;
  else now_us += (20 - rem);

  //  char buf[8192];
  // "%s : sperc_data %g us %g Mb/s, rcp_data %g us %g Mb/s, rcp_ctrl %g us = %g Mb/s\n"
  
  if(common_channel_ != NULL){
    if (first_log_rate_periodic_) {
      char header[8192];
      sprintf(header, "RATE_PERIODIC pathid fid time(s) sperc_data(mb/s) rcp_data(mb/s) actual_data(mb/s) rcp_ctrl(mb/s) SPERC_state\n");	    
      (void)Tcl_Write(common_channel_, header, strlen(header));
      first_log_rate_periodic_ = false;
    } 

    double actual_interval = interval();
    char buf[8192];
    sprintf(buf, "RATE_PERIODIC %s %d %d %g %g %g %g %s\n",
	    pathid_,
	    fid_,	       
	    now_us,
	    (((size_*8)/(sperc_data_interval_))*1e-6)*weight_,
	    ((((size_*8)/(sperc_data_interval_))*1e-6)*weight_),
	    ((size_*8)/(actual_interval))*1e-6,
	    (((SPERC_CONTROL_PKT_HDR_BYTES_*8.0)/(ctrl_packet_interval_))*1e-6),
	    SPERC_HOST_STATE_STR[SPERC_state]);    
    (void)Tcl_Write(common_channel_, buf, strlen(buf));
  }

    double next_log_time = ( (double) now_us+20) / 1e6;
    log_timer_.resched((next_log_time-now));

  
}

void SPERCAgent::log_rates() {
  if(channel_ != NULL){
    if (first_log_rate_change_) {
      char header[8192];
      sprintf(header, "#RATE_CHANGE fid time(s) wtd_rate(mb/s) actual_rate(mb/s) ctrl_rate(mb/s) limited_by(4=host)\n");	    
      (void)Tcl_Write(channel_, header, strlen(header));
      first_log_rate_change_ = false;
    } 
    char buf[8192];
    // "%s : sperc_data %g us %g Mb/s, rcp_data %g us %g Mb/s, rcp_ctrl %g us = %g Mb/s\n"
    double actual_rate = -1;
    if (interval() > 0) actual_rate = ((size_*8)/(interval()))*1e-6;
    //	    (((size_*8)/(interval_))*1e-6) ,

    sprintf(buf, "#RATE_CHANGE %d %f %g %g %g %d\n",
	    fid_,	       
	    ((double) Scheduler::instance().clock()),
	    ((((size_*8)/(sperc_data_interval_))*1e-6))*weight_,
	    actual_rate,
	    (((SPERC_CONTROL_PKT_HDR_BYTES_*8.0)/(ctrl_packet_interval_))*1e-6),
	    rate_limited_by_);
    
    (void)Tcl_Write(channel_, buf, strlen(buf));

  }
}

void SPERCAgent::log_ctrl_fin_sent_at_src() {
  Tcl::instance().evalf("%s ctrl-fin-sent-at-src", this->name());
}

void SPERCAgent::log_ctrl_fin_sent_at_rcvr() {
  Tcl::instance().evalf("%s ctrl-fin-sent-at-rcvr", this->name());
}

// Track change in RCP interval/ rate, SPERC interval/rate on same plot
void SPERCAgent::update_data_rate(double rate) {
  if (rate <= 0) {
    fprintf(stderr, "Warning!! %s : asked to update SPERC rate to %f\n",
	    log_prefix().c_str(), rate);
    return;
  }

  double now = Scheduler::instance().clock();

  if (last_rate_ == 0.0001) last_rate_ = 0;
  double tmp = last_rate_ * (now - last_rate_change_time_); // Gb/s*s
  bytes_scheduled_ += (weight_*tmp*1e9)/8; // bytes
  
  last_rate_ = rate;
  last_rate_change_time_ = now;

  double old_interval = sperc_data_interval_;
  double new_interval = (size_*8)/(rate*1e9);


  // currently SPERC rate at SPERClassifier is in Gb/s
  if (old_interval != new_interval) {  
    sperc_data_interval_  = new_interval;

    // in case we were using line until we got
    // the first sperc rate, time to switch rate
    // limiting mode now, rate_change()
    // will also cancel previous timeout
    // assert already in SPERC_RUNNING
    if (using_init_rate_until_first_sperc_rate()) {
      sperc_rate_limiting_ = SPERC_RATE_LIMITED;
      if (SPERC_state != SPERC_RUNNING) {
	fprintf(stderr, "Warning!! %s: in init_rate_until.., got first rate but we're in %s.\n",
		log_prefix().c_str(), SPERC_HOST_STATE_STR[SPERC_state]);
	// exit(1);
      }
    }

    if (using_sperc_rate()) {
      rate_change();
    } else {
      fprintf(stderr, "Error!! %s: asked to update rate, but we're not using sperc rate\n",
	      log_prefix().c_str());
      exit(1);
    }

    log_rates();
  }
}

bool SPERCAgent::is_spurious(Packet *p) {
  // A source can detect when it receives an
  // ack for some other flow and ignore it.
  // A receiver can do this once it bootstraps
  // flow id from first SYN it gets
  hdr_sperc* rh = hdr_sperc::access(p);
  const auto rtype = rh->SPERC_pkt_type();
  // once a flow is done, fid of source is reset
  // only when a new flow is started. Until then
  // source continues to receive any outstanding 
  // ACKs. stop_ctrl -> Agent/SPERC done 
  // -> Agent_Aggr_Pair fin_notify -> calls schedule
  // in fct experiments

  // Agent_Aggr_Pair setup -> holds all SPERC_pairs
  // Agent_Aggr_Pair will set_callback for SPERC_Pair

  // SPERC_pair setup -> connects spercs, spercr
  // SPERC_pair: aggr_ctrl, start_cbfunc, fin_cbfunc
  // SPERC_pair will set_callback for Agent/SPERC

  // Agent/SPERC setup in C++ (from SPERC_pair init)
  // SPERC_pair start will set fid_, numpkts_
  // in Agent/SPERC, id_ in SPERC_pair, set is_active
  // and call spercs sendfile()

  // SPERC_pair stop calls Agent/SPERC stop (spercs)


  // Agent/SPERC done -> ctrl (SPERC_pair) fin_notify
  // SPERC_pair fin_notify -> 
  //  aggr_ctrl (Agent_Aggr_Pair) fin_cbfnc
  //  spercs reset
  //  spercr reset,
  //  is_active set to 0
  //  id set to 0
  // should also reset fid to 0? it's not reset in 
  // SPERCAgent::reset(), only reset in tcl on
  // start
  // note that receiver (and its fid)
  //  must be reset from tcl
  //  otherwise it continues to handle any packet
  //  that it gets
  // hmm but reset from tcl not handled??
  if (hdr_ip::access(p)->flowid() != fid()) {
    if (start_time_ > 0) {
      // this source started sending a file
      if (rtype == SPERC_SYNACK ||
	  rtype == SPERC_ACK ||
	  rtype == SPERC_FINACK) {    
	fprintf(stderr, 
	      "Warning!! src got spurious ack- fid %d (%d)\n",
		hdr_ip::access(p)->flowid(), fid());
	return true;
      } else {
	fprintf(stderr,
		"Warning!! src got a spurious non ACK.\n");
	return true;
      }
    } else {
      // this must be a receiver
      if (rtype == SPERC_SYN) {
	fprintf(stderr,
		"Error!! rcv received a spurious SYN.\n");
	exit(1);
      } else if (rtype == SPERC_FIN ||
		 rtype == SPERC_DATA) {
	fprintf(stdout, 
	      "Warning!! rcv got spurious fin/data - %d (%d)\n",
		hdr_ip::access(p)->flowid(), fid());
	return true;
      } else {
	fprintf(stderr,
		"Warning!! rcv got a spurious ACK.\n");
	return true;
      }
    }
  }

  if (not (rtype == SPERC_SYN || rtype == SPERC_DATA || rtype == SPERC_FIN\
	   || rtype == SPERC_SYNACK || rtype == SPERC_ACK || rtype == SPERC_FINACK)) {
    fprintf(stderr,"Warning!! %lf in SPERCAgent::recv_data() %s unknown pkt %s\n",
	    Scheduler::instance().clock(),this->name(),
	    hdr_sperc_ctrl::get_string(p).c_str());
    return true;
  }

  return false;
}
	  
void SPERCAgent::recv_data(Packet* p)
{

  /*#ifdef LAVANYA_DEBUG
    fprintf(stdout, "%s : recv_data()  num_dataPkts_acked_by_receiver_ %d\n",
    log_prefix().c_str(), num_dataPkts_acked_by_receiver_);
    #endif*/ // LAVANYA_DEBUG
  hdr_sperc* rh = hdr_sperc::access(p);
  populate_route_recvd(p);
  int recv_prio = hdr_ip::access(p)->prio();


  // if this isn't the case throw error and exit
  //  if ( (rh->SPERC_pkt_type() == SPERC_SYN) ||
  //    ((SPERC_state != SPERC_INACT) && (ip->flowid() == fid_)) ) 

  const auto rtype = rh->SPERC_pkt_type();
  bool interval_changed = false;

  if (is_spurious(p)) { 
    fprintf(stderr, "Warning!! Spurious data packet received flow %d path %s\n", 
	    hdr_ip::access(p)->flowid(),
	    get_forward_path(p).c_str());

    Packet::free(p); 
    return;
  }


  if ((rtype == SPERC_SYNACK || rtype == SPERC_ACK || rtype == SPERC_FINACK) && first_ack_) {
    double num_bytes = numpkts_*size_; //1500.0;

    bool learning_workload =  (SPERC_PRIO_WORKLOAD_ == 1);
    int num_thresh = 0;
    if (learning_workload) 
      num_thresh = (int) (sizeof(learning_thresh_for_prio_) / sizeof(double));
    else num_thresh = (int) (sizeof(search_thresh_for_prio_) / sizeof(double));
    int thresh_index = SPERC_PRIO_THRESH_INDEX_;
    if (thresh_index < 0 or thresh_index >= num_thresh) {
      fprintf(stderr, "Error!! invalid threshold for marking priority packets %d [0-%d)\n", 
	      thresh_index, num_thresh);
      exit(1);
    }
    double thresh = 0;
    if (learning_workload) 
      thresh = learning_thresh_for_prio_[thresh_index];
    else thresh = search_thresh_for_prio_[thresh_index];

    fprintf(stdout, "FIRSTACK %d %f %.12f %s weight %f kilobytes %u prio %d (thresh: %.3f KB)\n", 
	    hdr_ip::access(p)->flowid(), num_bytes,
	    start_time_,
	    get_forward_path(p).c_str(),
	    weight_,
	    (numpkts_*size_)/1000,
	    recv_prio,
	    thresh);
    first_ack_ = false;
    //    fprintf(stdout, "path of flow %d is %s\n", 
    //  hdr_ip::access(p)->flowid(), get_pkt_path(p).c_str());
    sprintf(pathid_, "%s", get_path_id(p).c_str());
  }


  if (rtype == SPERC_ACK || rtype == SPERC_FINACK) {
    num_dataPkts_acked_by_receiver_ = rh->num_dataPkts_received(); 

    if (numpkts_ == 0 or num_dataPkts_acked_by_receiver_ == 0) {
      fprintf(stderr, "Error!! received SPERC_ACK/ FINACK but numpkts_ %u num_dataPkts_acked_by_receiver_ %u.\n", numpkts_, num_dataPkts_acked_by_receiver_);
      exit(1);
    }

    if (num_dataPkts_acked_by_receiver_ >= numpkts_) {
      if (!send_ctrl_fin_) {
	send_ctrl_fin_ = true;
	Tcl::instance().evalf("%s send-ctrl-fin-set", this->name());
	stop_data();
      }
    }
  }
  /*#ifdef LAVANYA_DEBUG
    fprintf(stdout, "%s: recv_data got %s with num_dataPkts_received %d, set num_dataPkts_acked_by_receiver to %d numpkts_ is %f\n",
    log_prefix().c_str(), SPERC_PKT_T_STR[rtype], rh->num_dataPkts_received(), num_dataPkts_acked_by_receiver_, numpkts_);
    #endif*/ // LAVANYA_DEBUG

  /*#ifdef LAVANYA_DEBUG
    fprintf(stdout,
    "%s : stop() and set send_ctrl_fin_ to true num_ctrl_packets_sent_ %d num_ctrl_packets_recvd_ %d numdataPkts_acked_by_receiver_ %d pkt: %s\n",
    log_prefix().c_str(), num_ctrl_packets_sent_, num_ctrl_packets_recvd_, 
    num_dataPkts_acked_by_receiver_,
    hdr_sperc_ctrl::get_string(p).c_str());
    #endif*/
  

  if (rtype == SPERC_SYNACK ||  rtype == SPERC_ACK || rtype == SPERC_FINACK) {
    // received at source
    log_ack(p);
    // update RTT and rate using values that the switch filled in
    rtt_ = Scheduler::instance().clock() - rh->ts();		  
    if (min_rtt_ == -1 || min_rtt_ > rtt_) min_rtt_ = rtt_;
    if (max_rtt_ == -1 || max_rtt_ < rtt_) max_rtt_ = rtt_;    
  } 


  if (rtype == SPERC_FIN || rtype == SPERC_DATA) {
  // received at destination
  // because SPERC_FIN is piggybacked on the last packet of flow
  unsigned old_value = num_dataPkts_received_;
  unsigned new_value = old_value + 1;
  //num_dataPkts_received_ = num_dataPkts_received_ + 1; 
  this->num_dataPkts_received_ = new_value;

  if (this->num_dataPkts_received_ == rh->numpkts()) {
    Tcl::instance().evalf("%s all-datapkts-received", this->name()); 
  }
 }

  if (rtype == SPERC_FIN || rtype == SPERC_SYN || rtype == SPERC_DATA) {
    // received at destination
    Packet* send_pkt = allocpkt();
    // ACK has same priority as received packet
    populate_SPERC_cmn_data_header(send_pkt);    
    populate_SPERC_base_data(send_pkt, p, hdr_sperc::get_ack_type(rtype));
    populate_route_new(send_pkt, p);			
    hdr_ip::access(send_pkt)->prio() = recv_prio;

    if (recv_prio > 0 and rtype == SPERC_SYN) {
      fprintf(stdout, "received a SYN packet of flow %d with prio %d, sending ack with prio %d.\n", hdr_ip::access(p)->flowid(), recv_prio, hdr_ip::access(send_pkt)->prio());
    }

    target_->recv(send_pkt, (Handler*)0);
  }  

  if (rtype == SPERC_FIN) Tcl::instance().evalf("%s fin-received", this->name()); 

  if (rtype == SPERC_SYNACK and !using_sperc_rate()) {
    fprintf(stderr, 
    "Error!! %s: we got a SYNACK even though we're not using_sperc_rate() fid %d numpkts %u rate limiting %s  SPERC_INIT_SPERC_DATA_INTERVAL_ %f state %s", log_prefix().c_str(),
	    fid_, numpkts_, SPERC_RATE_LIMITING_STR[sperc_rate_limiting_],
	    SPERC_INIT_SPERC_DATA_INTERVAL_,
    SPERC_HOST_STATE_STR[SPERC_state]);
    exit(1);
  }

  // by the time we get SYNACK we should have a rate to use for data packets
  // sperc has default initial rate, rcp must get rate in SYNACK, 
  // must be in SYNSENT when we get SYNACK
  
  // if we don't have a rate to start with, then just move to SYNACK_RECEIVED
  // so we can start as soon as we get a rate. we will not re-transmit SYN now.

  bool ready_to_start =  (SPERC_state == SPERC_SYNSENT) &&  
    using_sperc_rate() and (sperc_data_interval_ > 0);
      
  if (rtype == SPERC_SYNACK  and ready_to_start and !stop_requested_) start();
  else if (rtype == SPERC_SYNACK and SPERC_state == SPERC_SYNSENT)
    SPERC_state = SPERC_SYNACK_RECVD;
 Packet::free(p);
}


int SPERCAgent::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();

	if (argc == 2) {
	  if (strcmp(argv[1], "rate-change") == 0) {
	    rate_change();
	    return (TCL_OK);
	  } else 
	    if (strcmp(argv[1], "start") == 0) {
	      start();
	      return (TCL_OK);
	    } else if (strcmp(argv[1], "stop") == 0) {
	      if (!stop_requested_) {
		stop_requested_ = true;
		Tcl::instance().evalf("%s stop-request-set", this->name()); 

		if (!send_ctrl_fin_) {
		  send_ctrl_fin_ = true;
		  Tcl::instance().evalf("%s send-ctrl-fin-set", this->name()); 
		  stop_data();
		}
	      }	      
	      return (TCL_OK);
	    }
	    else if (strcmp(argv[1], "sendfile") == 0) {
	      sendfile();
	      return(TCL_OK);
	    } else if (strcmp(argv[1], "reset") == 0) {
	      reset();
	      return(TCL_OK);
	    }
	} 
	else if (argc == 3) {
	  if (strcmp(argv[1], "attach") == 0) {
	    int mode;
	    const char* id = argv[2];
	    channel_ = Tcl_GetChannel(tcl.interp(),
				      (char*) id, &mode);
	    if (channel_ == NULL) {
	      tcl.resultf("Tagger (%s): can't attach %s for writing\n",
			  name(), id);
	      return (TCL_ERROR);
	    }
	    return (TCL_OK);
	  } 
	  else 	  if (strcmp(argv[1], "attach-common") == 0) {
	    int mode;
	    const char* id = argv[2];
	    common_channel_ = Tcl_GetChannel(tcl.interp(),
					     (char*) id, &mode);
	    if (common_channel_ == NULL) {
	      tcl.resultf("Tagger (%s): can't attach %s for writing\n",
			  name(), id);
	      return (TCL_ERROR);
	    }
	    return (TCL_OK);
	  }
	  else if (strcmp(argv[1], "get-sending-rate") == 0) {
	    if (SPERC_state == SPERC_SYNSENT) {
	    }
	  }
	  
	}
	return (Agent::command(argc, argv));
}

void SPERCAgent::ctrl_rate_change() {
	sperc_ctrl_timer_.force_cancel();		
	if (ctrl_packet_interval_ <= 0) {
		fprintf(stderr, "Warning!! ctrl rate change for interval %g, ignore\n", ctrl_packet_interval_);
		return;
	}
	double t = lastctrlpkttime_ + ctrl_packet_interval_;		
	double now = Scheduler::instance().clock();
	if ( t > now) {
		sperc_ctrl_timer_.resched(t - now);
	} else {
		// send ctrl packet immediately (and reschedule timer)
		ctrl_timeout();
	}
}

/* 
 * We modify the rate in this way to get a faster reaction to the a rate
 * change since a rate change from a very low rate to a very fast rate may 
 * take an undesireably long time if we have to wait for timeout at the old
 * rate before we can send at the new (faster) rate.
 */
void SPERCAgent::rate_change()
{

  // if (fid_ == 26 or fid() == 308) {
  //   fprintf(stderr, "%s: rate_change() interval() %f lastpkttime_ % f\n",
  // 	    log_prefix().c_str(), interval(), lastpkttime_);
  // }
  if (SPERC_state == SPERC_RUNNING) {
    sperc_timer_.force_cancel();		
    next_timeout_ = 0;

    if (interval() <= 0) {
      fprintf(stderr, "Error!! data rate change for interval %g, ignore", interval());
      exit(1);
    }

    double t = lastpkttime_ + interval();		
    double now = Scheduler::instance().clock();
    // next packet (based on switch's rate) is in the future

    if ( t > now)  {
      sperc_timer_.resched(t - now);
      next_timeout_ = t;
      // if (fid_ == 26 or fid() == 308) {
      // 	fprintf(stderr, "%s: sperc_timer_resched %f\n",
      // 		log_prefix().c_str(), t - now);
      // }
    } else {
      timeout();
      next_timeout_ = now;
      // if (fid_ == 26 or fid() == 308) {
      // 	fprintf(stderr, "%s: timeout()\n",
      // 		log_prefix().c_str());
      // }

    }
    // next packet should have been sent already
    // sends a packet immediately and reschedules timer for interval
  }
}

void SPERCAgent::sendpkt()
{
	Packet* p = allocpkt();
	populate_SPERC_cmn_data(p);
	populate_SPERC_base_data(p, NULL, SPERC_DATA);
	populate_route_new(p, NULL);

/*#ifdef LAVANYA_DEBUG
	if (hdr_cmn::access(p)->uid_ < 5) {
	  hdr_cmn *cmnh = hdr_cmn::access(p);
	  fprintf(stdout, "%s : sendpkt() with cmnh->size_ %d (size_=%d)\n", log_prefix().c_str(), cmnh->size(), size_);
	}
#endif*/ // LAVANYA_DEBUG

	lastpkttime_ = Scheduler::instance().clock();

	bytes_sent_ += hdr_cmn::access(p)->size();
	target_->recv(p, (Handler*)0);

	if (SPERC_state == SPERC_RETRANSMIT)
		num_pkts_resent_++;
	else
		num_sent_++;
}

void SPERCAgent::sendlast()
{
	Packet* p = allocpkt();
	populate_SPERC_cmn_data(p);
	populate_SPERC_base_data(p, NULL, SPERC_FIN);
	populate_route_new(p, NULL);
	lastpkttime_ = Scheduler::instance().clock();

	bytes_sent_ += hdr_cmn::access(p)->size();
	target_->recv(p, (Handler*)0);
	num_sent_++;
/*#ifdef LAVANYA_DEBUG
	{
	  hdr_cmn *cmnh = hdr_cmn::access(p);
	fprintf(stdout, "%s : sendlast() with cmnh->size_ %d (size_=%d)\n", log_prefix().c_str(), cmnh->size(), size_);
	}
#endif*/ // LAVANYA_DEBUG
}

void SPERCATimer::expire(Event* /*e*/) {
	(*a_.*call_back_)();
}


void SPERCAgent::reset() {
  Tcl::instance().evalf("%s reset-called", this->name()); 

  jitter_timer_.force_cancel();

  // Rate limiters, check they are disabled TODO
  sperc_ctrl_timer_.force_cancel();  
  sperc_timer_.force_cancel();
  next_timeout_ = 0;
  // For retransmissions, start this timer after
  // sending numpkts_, then after sending 
  // num_Pkts_to_retransmit_
  rto_timer_.force_cancel();
  log_timer_.force_cancel();

  SPERCControlAgent::reset();
  

  // some of these variables were bound to Tcl versions
  // as soon as Agent was created. If we reset and later start
  // a new flow using this same agent, hopefully the Tcl
  // variables (numpkts_, fid_, seqno_, packetSize_, using_sperc_data_rate_,
  // nodeid_, rate_change_interval_ have been updated as needed.
  //Tcl::instance().evalf("%s reset-tcl", this->name()); 
  // fprintf(stdout, "%s: reset called.\n", log_prefix().c_str());

  if (SPERC_state != SPERC_INACT and numpkts_ > 0) {
    // It's possible that old flow got lost, in that case make this a warning
    fprintf(stderr, "Error!! %s: reset() called when in state %s numpkts_ %u\n",
	    log_prefix().c_str(), SPERC_HOST_STATE_STR[SPERC_state], numpkts_);
    exit(1);
  }

  // same as constructor except tcl bound variables
  // int nodeid_; // bound
  sprintf(pathid_, "-1");
  weight_ = 1;
  prio_ = 0;
  // we want to remember RTT to use for next flow
  if (SPERC_INIT_SPERC_DATA_INTERVAL_ <= 0) {
    rtt_ = 0; // wait to get from SYNACK
  }
  first_ack_ = true;
  start_time_ = -1;
  last_rate_ = 0;
  last_rate_change_time_ = 0;

  next_timeout_ = 0;
  bytes_scheduled_ = 0;
  bytes_sent_ = 0;
  min_rtt_ = -1; 
  max_rtt_ = -1;
  ctrl_rtt_ = 0;
  min_ctrl_rtt_ = -1;
  max_ctrl_rtt_ = -1;
  seqno_ = 0;
  ctrl_seqno_ = 0;
  syn_seqno_ = 0; // used for SYN
  SPERC_state = SPERC_INACT;
  // int size_; // bytes on the wire for data packets // bound
  // set to true once all data packets have been acked
  // or when stop requested by tcl
  // so control packets can signal links that flow has
  // finished
  send_ctrl_fin_ = false;
  // set to true if tcl requests to stop
  // regardless of outstanding packets, only
  // relevant to source. we'll stop_data
  // and not send any more new data packets. 
  // We'll start wrapping up in control plane
  // by setting send_ctrl_fin_ to true.
  // we'll handle outstanding ACKs and 
  // update num_.._acked
  // as long as ACK flow matches our flow id.
  stop_requested_ = false;

  // set true to use rate from SPERC headers (hdr_sperc_ctrl),
  // otherwise uses rate from RCP headers (hdr_sperc)
  // using_sperc_data_rate_; // bound
  // state that matters to sender
  // unsigned numpkts_; // bound
  sperc_rate_limiting_ = SPERC_RATE_LIMITED;
  num_sent_ = 0;
  num_Pkts_to_retransmit_ = 0;           
  num_pkts_resent_ = 0;
  num_enter_syn_retransmit_mode_ = 0;
  num_enter_retransmit_mode_ = 0;
  num_dataPkts_acked_by_receiver_ = 0;

  // state that matters to receiver
  // note: remember ot reset both src and dst
  // this isn't like tcp where we just incr seq_no_
  // we reset all state for each burst / sendfile
  num_dataPkts_received_ = 0;

  // for rate limiting control packets
  ctrl_packet_interval_ = -1;
  lastctrlpkttime_ = -1e6;
  if (nextctrlpacket_) {
    fprintf(stderr, "Error!! in reset nextctrlpacket_ is not null\n");
    exit(1);
  }

  nextctrlpacket_ = NULL;
  num_ctrl_packets_sent_ = 0;
  num_ctrl_packets_recvd_ = 0;
  
  // rate limiting data packets using SPERC
  lastpkttime_ = -1e6;
  sperc_data_interval_ = -1;
  // Tcl_Channel   channel_; // bound
  // first_log_rate_change_ = true;
  // first_log_recv_ack_ = true;
  // first_log_rate_periodic_ = true;
}
