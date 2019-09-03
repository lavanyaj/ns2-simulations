#include <inttypes.h>
#include <math.h>
#include <sstream> // for std::stringstream in log_prefix()
#include <memory> // for std::unique_ptr, std::make_unique
#include "drr.h"
#include "tcp.h"
#include "random.h"
#include "ip.h"
#include <string.h>
#include "sperc-hdrs.h"
#include "sperc-host.h"
#include "sperc.h"
#include "delay.h"
#include "random.h" // for initial value of queue timeout

void SPERCQueue::log_to_channel(const std::string& str) {
  if(sperc_channel_ != NULL and str.length() < 8192) {
    char buf[16635];   
    sprintf(buf,
	    "%s", 
	    str.c_str());
    (void)Tcl_Write(sperc_channel_, buf, strlen(buf));
  }

}

double SPERCQueue::get_capacity() {
	if (ld_) {
	  double bw = (((LinkDelay *) ld_)->bandwidth());
	  return bw;
	} 
	return -1;
}

double SPERCQueue::get_sperc_capacity() {
	return sperc_capacity_;
}

double SPERCQueue::get_bytecnt() {
  //  return blength();
  return TotalByteLength();
}


void SPERCQueue::populate_path_info(Packet* pkt) {
  hdr_sperc *hdr = hdr_sperc::access(pkt);
  if (hdr->is_fwd()) {
    hdr->fwd_path_[hdr->fwd_hop_] = fromnodeid_;
    hdr->fwd_hop_++;
  } else {
    hdr->rev_path_[hdr->rev_hop_] = fromnodeid_;
    hdr->rev_hop_++;
  }
}

void SPERCQueue::enque(Packet* pkt) {
  populate_path_info(pkt);
  // TODO: avg rtt based on control packets? then bi-modal distribution
  // but queue drain numerator includes both control and data packets
  // why do we expect it to drain in avg_rtt_?

  if (hdr_cmn::access(pkt)->ptype() == PT_SPERC) {
    double rtt = hdr_sperc::access(pkt)->rtt();
    if ( rtt > 0) {
      SPERC_PKT_T stype = hdr_sperc::access(pkt)->SPERC_pkt_type();
      if (stype == SPERC_ACK || stype == SPERC_SYNACK || stype == SPERC_FINACK) {
	//fprintf(stderr, "getting rtt from ACK packet also\n");
	//	exit(1);
      } else {
	avg_rtt_ = running_avg(rtt, avg_rtt_, 0.5);
      }
    }
  }

  if (hdr_cmn::access(pkt)->ptype() == PT_SPERC or
      hdr_cmn::access(pkt)->ptype() == PT_SPERC_CTRL) {
    act_input_traffic_ += hdr_cmn::access(pkt)->size();
  }

  // update per queue stats
  int prio = hdr_ip::access(pkt)->prio();
  if (prio < 0 or prio >= queue_num_) {
    fprintf(stderr, "Error!! enqueuing pkt with invalid prio %d\n", prio);
    exit(1);
  }

  act_input_traffic_by_q_[prio] += hdr_cmn::access(pkt)->size();
  if (hdr_cmn::access(pkt)->ptype() == PT_SPERC or
      hdr_cmn::access(pkt)->ptype() == PT_SPERC_CTRL) {
    double rtt = hdr_sperc::access(pkt)->rtt();
    if ( rtt > 0) {
      avg_rtt_by_q_[prio] = running_avg(rtt, avg_rtt_by_q_[prio], 0.5);
    }
  }
  
  Priority::enque(pkt);
  // in hi prio queue (possible that enque fails- our enque stats are basically
  // about packets that arrive)
}

void SPERCQueue::drop(Packet *pkt) {
  hdr_cmn* cmn = hdr_cmn::access(pkt);
  hdr_sperc *hdr = hdr_sperc::access(pkt);
  hdr_ip *iph = hdr_ip::access(pkt);
  if (cmn->ptype() == PT_SPERC_CTRL) {
      fprintf(stderr, "warning! %lf %d %d ctrl dropping %s for flow %d prio %d\n", 
	      now(), fromnodeid_, tonodeid_, 
	      SPERC_PKT_T_STR[hdr->SPERC_pkt_type()],
	      iph->flowid(), iph->prio());
      exit(1);
  }  else {
      fprintf(stderr, "warning! %lf %d %d data dropping %s for flow %d prio %d\n", 
	      now(), fromnodeid_, tonodeid_, 
	      SPERC_PKT_T_STR[hdr->SPERC_pkt_type()],
	      iph->flowid(), iph->prio());
  }
  Priority::drop(pkt);
}

Packet* SPERCQueue::deque() {
	Packet *p;
	p = Priority::deque();

	if (p) {
	  int prio = hdr_ip::access(p)->prio();
	  if (prio < 0 or prio >= queue_num_) {
	    fprintf(stderr, "Error!! dequeuing pkt with invalid prio %d\n", prio);
	    exit(1);
	  }
	  act_output_traffic_ += hdr_cmn::access(p)->size();
	  act_output_traffic_by_q_[prio] += hdr_cmn::access(p)->size();
	  
	  return (p);
	} 
	return 0;
}

double SPERCQueue::running_avg(double var_sample, double var_last_avg, double gain)
{
	double avg;
	if (gain < 0)
	  exit(3);
	avg = gain * var_sample + ( 1 - gain) * var_last_avg;
	return avg;
}

int SPERCQueue::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();

  if ((argc == 2) and strcmp(argv[1], "start-timeout") == 0) {
    double T = Random::normal(Tq_, 0.2*Tq_);
    end_slot_ = T;
    queue_timer_.sched(T);
    return (TCL_OK);
  }  else if (argc == 3 and strcmp(argv[1], "set-delay-link") == 0) {
    if (!(ld_=(LinkDelay*)TclObject::lookup(argv[2])))
      return (TCL_ERROR);
    double bw = (((LinkDelay *) ld_)->bandwidth());
    actual_capacity_ = bw/1.0e9;
    sperc_capacity_ = actual_capacity_;

    max_sperc_capacity_ =  sperc_capacity_;
    min_sperc_capacity_ =  sperc_capacity_;
    //min_sperc_capacity_ = sperc_capacity_;
    
    return (TCL_OK);
  }  else if (argc == 3 and strcmp(argv[1], "attach") == 0) {
    int mode;
    const char* id = argv[2];
    channel_ = Tcl_GetChannel(tcl.interp(),
			      (char*) id, &mode);
    if (channel_ == NULL) {
      tcl.resultf("Tagger (%s): can't attach %s for writing",
		  name(), id);
      return (TCL_ERROR);
    } else {
      return (TCL_OK);
    }
  } else if (argc == 3 and strcmp(argv[1], "attach-sperc") == 0) {
    int mode;
    const char* id = argv[2];
    sperc_channel_ = Tcl_GetChannel(tcl.interp(),
			      (char*) id, &mode);
    if (sperc_channel_ == NULL) {
      tcl.resultf("Tagger (%s): can't attach %s for writing",
		  name(), id);
      return (TCL_ERROR);
    } else {
      return (TCL_OK);
    }
  }
  return Priority::command(argc, argv);
} 

static class SPERCClass : public TclClass {
public:
  SPERCClass() : TclClass("Queue/Priority/SPERC") {}
  TclObject* create(int, const char*const*) {
    return (new SPERCQueue);
  }
} class_sperc_queue;

std::string SPERCQueue::log_prefix() {
  std::stringstream ss;
  double now = Scheduler::instance().clock();
  ss << "SPERCQueue"
     << " link " << fromnodeid_ << "->" << tonodeid_
     << " name() " << this->name() 
     << " time " << ((double) now);
  return ss.str();
}

SPERCQueue::SPERCQueue(): Priority(),
			  ld_(NULL),
			  channel_(NULL),
			  sperc_channel_(NULL),
			  queue_timer_(this, &SPERCQueue::queue_timeout),
			  avg_rtt_(0.000012),
			  act_input_traffic_(0),
			  act_output_traffic_(0),
			  actual_capacity_(-1),
			  queue_threshold_(22500),
			  sperc_capacity_(-1),
			  end_slot_(-1),
			  Tq_(0.000012),
			  max_sperc_capacity_(-1),
			  min_sperc_capacity_(-1),
			  alpha_(0.5),
			  beta_(0.5)
{

  // TODO: move out of init_vars
  bind("fromnodeid_", &fromnodeid_);
  bind("tonodeid_", &tonodeid_);
  bind("alpha_", &alpha_);
  bind("beta_", &beta_);
  bind("Tq_", &Tq_);
  bind("queue_threshold_", &queue_threshold_);

  avg_rtt_ = Tq_;

  for (int i = 0; i < 3; i++) {
    act_output_traffic_by_q_[i] = 0;
    act_input_traffic_by_q_[i] = 0;
    avg_rtt_by_q_[i] = Tq_;
  }
 
}

void SPERCQueue::queue_timeout() {
  double Q_ = get_bytecnt();
  double input_traffic = ((act_input_traffic_/Tq_)*8)/1.0e9; // bytes/s -> Gb/s
  double output_traffic = ((act_output_traffic_/Tq_)*8)/1.0e9; // bytes/s -> Gb/s


  double input_traffic_by_q[3] = {0,0,0};
  double output_traffic_by_q[3] = {0,0,0};
  double bytes_by_q[3] = {0,0,0};
  for (int i=0; i < 3; i++) {
    bytes_by_q[i] = QueueByteLength(i);
    input_traffic_by_q[i] = ((act_input_traffic_by_q_[i]/Tq_)*8)/1.0e9; // bytes/s -> Gb/s
    output_traffic_by_q[i] = ((act_output_traffic_by_q_[i]/Tq_)*8)/1.0e9; // bytes/s -> Gb/s
  }

  if (actual_capacity_ < 0 or avg_rtt_ < 0) {
    fprintf(stderr, "actual capacity is 0\n");
    exit(1);
  }  
  double queue_drain = ((Q_ - queue_threshold_)/avg_rtt_)*8/1.0e9; // bytes/s -> Gb/s
  double ratio = 1 +  (Tq_/avg_rtt_) 
			 * (1 / actual_capacity_) 
			 * (alpha_*(actual_capacity_ - input_traffic) - beta_*(queue_drain));
			 
  double temp = sperc_capacity_ * ratio;
  char clip = 'X';
  if (temp < min_sperc_capacity_) {
    sperc_capacity_ = min_sperc_capacity_;
    clip = 'L';
  } else if (temp > max_sperc_capacity_) {
    sperc_capacity_ = max_sperc_capacity_;
    clip = 'U';
  } else {
    sperc_capacity_ = temp;
    clip = 'M';
  }

  if(channel_ != NULL) {
    char buf[8192];
    sprintf(buf,
	"QUEUE_TIMEOUT name %s link %d->%d now %f totalQKbytes %f q0Kbytes %f q1Kbytes %f q2Kbytes %f totalInput %.10lf q0Input %.10lf q1Input %.10lf q2Input %.10lf totalOutput %.10lf q0Output %.10lf q1Output %.10lf q2Output %.10lf avgRtt %f q0Rtt %f q1Rtt %f q2Rtt %f\n", 
	this->name(), fromnodeid_, tonodeid_, now(), 
	    Q_/1000, bytes_by_q[0]/1000, bytes_by_q[1]/1000, bytes_by_q[2]/1000, 
	    input_traffic, input_traffic_by_q[0], input_traffic_by_q[1], input_traffic_by_q[2],\
	    output_traffic, output_traffic_by_q[0], output_traffic_by_q[1], output_traffic_by_q[2],\
	    avg_rtt_*1e6, avg_rtt_by_q_[0]*1e6, avg_rtt_by_q_[1]*1e6, avg_rtt_by_q_[2]*1e6);

    // sprintf(buf,
    // 	"QUEUE_TIMEOUT %s %d->%d %f %f %.10lf %.10lf %.10lf %.10lf %f %f %f %c %f %f\n", 
    // 	this->name(), fromnodeid_, tonodeid_, now(), Q_, 
    // 	sperc_capacity_, actual_capacity_, act_input_traffic_,
    // 	input_traffic, avg_rtt_, (actual_capacity_ - input_traffic),
    // 	    queue_drain, clip, Tq_, temp);
       (void)Tcl_Write(channel_, buf, strlen(buf));
  }
  // reset variables
  act_input_traffic_ = 0;
  act_output_traffic_ = 0;
  for (int i = 0; i < 3; i++) {
    act_output_traffic_by_q_[i] = 0;
    act_input_traffic_by_q_[i] = 0;
  }

  end_slot_ = now() + Tq_;
  queue_timer_.resched(Tq_);
 
}

void SPERCQTimer::expire(Event *) { 
  (*a_.*call_back_)();
}
