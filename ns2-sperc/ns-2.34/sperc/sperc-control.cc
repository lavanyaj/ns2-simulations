/* 
 * Author: Lavanya Jose
 * This file will implement the control packet processing at a SPERC router
 */


#include <math.h>
#include "random.h"
#include "sperc-control.h"
#include "drr.h"

void SPERCLinkStateTimer::expire(Event *) { 
(*a_.*call_back_)();
}


SPERCLinkState::SPERCLinkState():
  lastMaxSatTimeout(-1), lastMaxRttTimeout(-1),\
  stopped(false), sumSat(0), numSat(0), numFlows(0),			\
  maxSat(0), nextMaxSat(0), lastComputedResidualLevel(0),		\
  lastComputedBottleneckLevel(0), lastMaxSatUpdateCuzFlow(-1),		\
  lastMaxSatUpdateCuzBn(-1), lastNextMaxSatUpdateCuzFlow(-1),		\
  lastNextMaxSatUpdateCuzBn(-1), first_pkt_seen_(false),
  initialized_(false), classifier_(NULL),
  id_("LinkState"), crl(), num_reset_maxrtt_timeouts(0), q_(NULL),
  first_reset_log_(true), first_update_log_(true)
  {}

ControlRateLimiting::ControlRateLimiting(): numFlows(0), maxRtt(-1), nextMaxRtt(-1), maxFutureRtt(-1), nextMaxFutureRtt(-1) {};

// I'm assuming most of these values will be common to all SPERCLinkState instances
// so we can just declare defaults in tcl in the SPERCDestHashClassifier Tcl/ C++ objects
// and each link state instance can refer to them. Whenever we find a value that will
// have different values for different links (like linkCapacity), add it to the list
// of arguments for initialize
void SPERCLinkState::initialize(bubble linkCapacity, SPERCDestHashClassifier* classifier, 
				int fromNode, int toNode, SPERCQueue* q) {
  // check that linkCapacity is same as (q_->get_capacity())/1.0e9

  // double controlTrafficPc, double headroomPc, double minRtt, double initialMaxRtt, bubble initialControlPacketRate, double maxsatTimeout) {
  from_node_ = fromNode;
  to_node_ = toNode;
  q_ = q;

  if ((q_->get_capacity())/1.0e9 != linkCapacity) { 
    fprintf(stderr, "Error!! %f:  link_capacity from queue %f Gb/s different from classifier %f Gb/s.\n", 
	    now(), (q_->get_capacity())/1.0e9, linkCapacity);
    exit(1);
  } 

  classifier_ = classifier;

  // initialize maxRtt and maxFutureRtt here?
  // or it's okay for them to be -1 because the first
  // time we use them is after maxRttTimeout when we
  // would have seen control packets from all active flows
  // crl.maxRtt = classifier_->maxSatTimeout_;
  // crl.maxFutureRtt = classifier_->maxSatTimeout_;

  std::stringstream idStr;
  //  idStr << classifier->name() 
  //	<< "-egress=" << fromNode << "-" << toNode 
  //	<< "-ingress=" << toNode << "-" << fromNode;
  idStr << fromNode << "->" << toNode;
  id_ = idStr.str();
  // partitionLinkCapacity??
  // minRtt = minRtt;;
  // maxRtt = initialMaxRtt;
  // controlPacketRate = initialControlPacketRate;
  // maxSatTimeout = maxSatTimeout;
  
  // maybe this will go in classifier and some way to find correct link state?
  maxsatTimer = std::make_unique<SPERCLinkStateTimer >
    (this, &SPERCLinkState::reset_maxsat_timeout);  

  maxrttTimer = std::make_unique<SPERCLinkStateTimer >
    (this, &SPERCLinkState::reset_maxrtt_timeout);  

  initialized_ = true;
}
    
void SPERCLinkState::stop() {
  if (maxsatTimer) maxsatTimer->force_cancel();
  if (maxrttTimer) maxrttTimer->force_cancel();

  stopped = true;
}
    
void SPERCLinkState::start_timers() {
  if (!first_pkt_seen_) {
    //#ifdef LAVANYA_DEBUG
    // fprintf(stdout, "%s: %lf start_timers reset maxsat (%f us) reset max rtt (%f us) \n", 
    // 	    log_prefix().c_str(), now(),\
    // 	    classifier_->SPERC_MAXSAT_TIMEOUT_*1e6,
    // 	    classifier_->SPERC_MAXRTT_TIMEOUT_*1e6);
    //#endif

    if (maxsatTimer) {
      lastMaxSatTimeout = now();
      maxsatTimer->force_cancel();
      maxsatTimer->sched(classifier_->SPERC_MAXSAT_TIMEOUT_);
    }
    if (maxrttTimer) {
      lastMaxRttTimeout = now();
      maxrttTimer->force_cancel();
      maxrttTimer->sched(classifier_->SPERC_MAXRTT_TIMEOUT_);
    }
    first_pkt_seen_ = true;
  }
}
// called from classifier/classifier-hash.cc
void SPERCLinkState::process_for_control_rate_limiting(Packet *pkt) {
  hdr_cmn* cmn = hdr_cmn::access(pkt);
  if (hdr_cmn::access(pkt)->ptype() != PT_SPERC_CTRL) {
    return;
  }


  start_timers();

  int oldNumFlows = crl.numFlows;
  double oldRate = get_link_capacity_for_control();;
  if (oldNumFlows > 0) {
    oldRate /= oldNumFlows; 
  } else {
    oldRate /= 2;
  }
  

  hdr_sperc* hdr = hdr_sperc::access(pkt);
  hdr_ip* ip = hdr_ip::access(pkt);
  hdr_sperc_ctrl* sch = hdr_sperc_ctrl::access(pkt);

  //fprintf(stdout, "\n");

  bool numFlowsChanged = false;
  if (hdr->SPERC_pkt_type() == SPERC_CTRL_SYN) {    
    numFlowsChanged = true;
    crl.numFlows++;
  } else if (sch->is_exit() and 
	     hdr->SPERC_pkt_type() == SPERC_CTRL_REV) {
    numFlowsChanged = true;
    crl.numFlows--;
  }

  double rate = 0;
  double cap = get_link_capacity_for_control();
  
  if (crl.numFlows > 0) {
    rate = cap/crl.numFlows;
  } else {
    rate = cap/2;
  }

  double request = hdr->SPERC_request_rate();
  if (request < 0 || request > rate) {
    hdr->set_SPERC_rate(rate);
  }

  // packer carries RTT of last control packet
  // sent by flow. if multiple flows start in
  // the same RTT, they'll be sent at cap total
  // RTT of last flow will be correct
  double actual_rtt = hdr->rtt();
  // size is in bytes, rate is in Gb/s
  double future_rtt = (cmn->size()*8)/(rate*1e9);

  // if actual_rtt is 0 the don't update rtt info


  // fprintf(stdout,							\
  // 	  "%s: %f s in process_for_control.., actual_rtt %f us, future_rtt %f us, size %d B, cap %f Gb/s, rate per flow %f Gb/s,  ptype %d\n",
  // 	  log_prefix().c_str(), now(), actual_rtt*1e6, future_rtt*1e6,
  // 	  cmn->size(), cap, rate, cmn->ptype());    

  // crl.maxRtt is an upper bound to the actual RTTs of control packets
  // seen during the last "maxRttTimeout" seconds.
  // crl.maxFutureRtt is an upper bound to the future RTTs estimated
  // from control packets seen during the last "maxRttTimeout" seconds
  // based on the rate assigned to the flows.
  bool maxRttChanged = false;
  bool maxFutureRttChanged = false;  

  double oldMaxRtt = crl.maxRtt;
  double oldNextMaxRtt = crl.nextMaxRtt;
  if (actual_rtt > 0) {
    if (actual_rtt > crl.maxRtt) {
      maxRttChanged = true;
      crl.maxRtt = actual_rtt;
    }
    if (actual_rtt > crl.nextMaxRtt) {
      maxRttChanged = true;
      crl.nextMaxRtt = actual_rtt;
    }
    if (actual_rtt > classifier_->SPERC_MAXRTT_TIMEOUT_) {
      fprintf(stderr,
	      "Warning!! (Error) %f SPERCLinkState %s: actual rtt (%f us) exceeds maxRttTimeout (%f us), future rtt %f us because %d B @ %f Gb/s %d)\n",
	      now(), id().c_str(), 
	      actual_rtt*1e6, classifier_->SPERC_MAXRTT_TIMEOUT_*1e6, future_rtt*1e6, \
	      cmn->size(), rate, cmn->ptype());
    }
  }

  double oldMaxFutureRtt = crl.maxFutureRtt;
  double oldNextMaxFutureRtt = crl.nextMaxFutureRtt;
 
  if (future_rtt > 0) {
    if (future_rtt > crl.maxFutureRtt) {
      maxFutureRttChanged = true;
      crl.maxFutureRtt = future_rtt;
    }
    if (future_rtt > crl.nextMaxFutureRtt) {
      maxFutureRttChanged = true;
      crl.nextMaxFutureRtt = future_rtt;
    }
    
    if (future_rtt > classifier_->SPERC_MAXRTT_TIMEOUT_) {
      fprintf(stderr,
	      "Warning!! (Error) %f SPERCLinkState %s: future rtt (%f us) exceeds maxRttTimeout (%f us), future rtt %f us because %d B @ %f Gb/s %d), actual rtt %f us\n",
	      now(), id().c_str(), 
	      future_rtt*1e6, classifier_->SPERC_MAXRTT_TIMEOUT_*1e6, future_rtt*1e6, \
	      cmn->size(), rate, cmn->ptype(), actual_rtt*1e6);
    }

  }

  if (maxRttChanged or maxFutureRttChanged) {
    // fprintf(stdout,
    // 	    "%s: %fs crl.maxRtt %f -> %f us; crl.maxFutureRtt %f -> %f us; crl.nextMaxRtt %f -> %f us; crl.nextMaxFutureRtt %f -> %f us; actual rtt %f us; future rtt %f us because %d B @ %f Gb/s %d)\n",
    // 	    log_prefix().c_str(), now(),\
    // 	    oldMaxRtt*1e6, crl.maxRtt*1e6,\
    // 	    oldMaxFutureRtt*1e6, crl.maxFutureRtt*1e6,\
    // 	    oldNextMaxRtt*1e6, crl.nextMaxRtt*1e6,\
    // 	    oldNextMaxFutureRtt*1e6, crl.nextMaxFutureRtt*1e6,\
    // 	    actual_rtt*1e6, future_rtt*1e6,\
    // 	    cmn->size(), rate, cmn->ptype());
    // maxsat_timeout_ increases here
    adjust_maxsat_timeout();
  }
 

  int nodeid = -1;
  if (classifier_) 
    nodeid = classifier_->nodeid_;
  

  if ((crl.numFlows < 0 or crl.numFlows > 1000) and numFlowsChanged) {
    fprintf(stderr,
	    "%f SPERCLinkState %s at node %d::process_for..() pkt %s HIGHLY SUSPICIOUS numFlows %d->%d rate %f->%f request %f->%f maxRtt %f->%f timeout %f\n",
	  now(), id().c_str(), nodeid, hdr_sperc_ctrl::get_string(pkt).c_str(),
	  oldNumFlows, crl.numFlows, oldRate, rate, request, hdr->SPERC_request_rate(),
	  oldMaxRtt, crl.maxRtt, classifier_->SPERC_MAXSAT_TIMEOUT_);
  }
}

std::string SPERCLinkState::log_prefix() {
  std::stringstream ss;
  int nodeid = -1;
  if (classifier_) 
    nodeid = classifier_->nodeid_;

  ss << "SPERCLinkState id " << id() << ", node " << nodeid
     << ", time " << ((double) Scheduler::instance().clock()*1e6);
  return ss.str();

}
void SPERCLinkState::process_ingress(Packet *pkt) {
  start_timers();

  hdr_cmn* cmn = hdr_cmn::access(pkt);
  hdr_sperc* hdr = hdr_sperc::access(pkt);
  hdr_ip* ip = hdr_ip::access(pkt);
  // ingress link processes reverse packet
  // what hop do we use? hops are changed at Link:queue
  // number of reverse hops at this point: one at destination, one at each link until
  // before classifier: so it includes this ingress link
  // TODO: check
  int hop = hdr->fwd_hop() - hdr->rev_hop() - 1;
  // when pkt leaves agent fwd_hop is 1 so at src node fwd_hop is 1, hop = 0
  // when pkt leaves link 0-1 fwd_hop is 2 so at node 1 fwd_hop is 2, hop = 1
  // when pkt leaves link 1-3 fwd_hop is 3 so at node 3 fwd_hop is 3, hop = 2
  // when pkt leaves dst agent fwd_hop is 4 and rev_hop is 1 
  //  so at dst node rev_hop is 1, hop = 2
  // but we're not calling process_reverse at dst node
  // when pkt leaves link 3-1 rev_hop is 2 so at node 1 rev_hop is 2, hop = 1
  // when pkt leaves link 1-0 rev_hop is 3 so at node 0 rev_hop is 3, hop = 0

  // so this works if we index starting at 0 and we have two array slots for 2 links.

  int nodeid = -1;
  if (classifier_) 
    nodeid = classifier_->nodeid_;

  if (hdr->is_fwd() or hdr->fwd_hop() <= 0 or hdr->rev_hop() <= 0) {
    fprintf(stderr, "Error!! SPERCLinkState %s at Classifier at node %d::process_ingress() invalid pkt %s (cuz hdr->is_fwd or hdr_fwd_hop() <= 0 or hdr->rev_hop() <= 0) \n",
	    id().c_str(), nodeid, hdr_sperc_ctrl::get_string(pkt).c_str());
    exit(-1);
  }

  /*#ifdef LAVANYA_DEBUG_PATH
  fprintf(stdout, "SPERCLinkState %s at Classifier at node %d::process_ingress for pkt %s  using hop_index %d\n",
	  id().c_str(), nodeid, hdr_sperc_ctrl::get_string(pkt).c_str(),
	  hop);
  #endif */ // LAVANYA_DEBUG_PATH


  // Commenting out ingress link since fwd path neq revers in WAN possibly
  if (classifier_ and classifier_->SPERC_UPDATE_IN_REV_)
    link_action(pkt, hop);
}  

// void SPERCLinkState::update_local_state(SPERCLocalLabel oldLabel, bubble oldAlloc,
// 					SPERCLocalLabel newLabel, bubble newAlloc, bubble copyWeight,
// 					bubble residualLevel,
// 			Packet * pkt) {

void SPERCLinkState::update_local_state(
bubble limitRate, int8_t oldIgnoreBit, 
SPERCLocalLabel oldLabel, bubble oldAlloc,
SPERCLocalLabel newLabel, bubble newAlloc,
bubble copyWeight,bubble bottleneckRate,
Packet * pkt) {
  hdr_ip *ip = hdr_ip::access(pkt);
  hdr_sperc_ctrl* sch = hdr_sperc_ctrl::access(pkt);
  double now = Scheduler::instance().clock();
  
 SPERCSwitchUpdate upd;
 upd.oldSumSat = this->sumSat;
 upd.oldNumSat = this->numSat;
 upd.oldNumFlows = this->numFlows;
 upd.oldMaxSat = this->maxSat;

 // Update Switch Local State Based on Label and Message
 if (oldLabel == SPERCLocalLabel::NEW_FLOW and
     (newLabel == SPERCLocalLabel::SAT or
      newLabel == SPERCLocalLabel::UNSAT)) {
   //#ifdef LAVANYA_DEBUG
   // if (from_node_ == 0 or from_node_ == 145) {
   //   fprintf(stdout, "%s fid %d NEW_FLOW -> SAT/ UNSAT, increment numFlows\n", 
   // 	     log_prefix().c_str(), ip->flowid());
   // }
   //#endif */ // LAVANYA_DEBUG
   this->numFlows += (1 * copyWeight);
  }
  if (newLabel == SPERCLocalLabel::FINISHED and		\
      (oldLabel == SPERCLocalLabel::SAT or			\
       oldLabel == SPERCLocalLabel::UNSAT)){
    //#ifdef LAVANYA_DEBUG
    // if (from_node_ == 0 or from_node_ == 145) {
    //   fprintf(stdout, "%s fid %d SAT/ UNSAT -> FINISHED, decrement numFlows\n",
    // 	      log_prefix().c_str(), ip->flowid());
    // }
    //#endif */ // LAVANYA_DEBUG
    this->numFlows -= (1 * copyWeight);  
  }

  if (oldLabel == SPERCLocalLabel::NEW_FLOW and
      newLabel == SPERCLocalLabel::SAT) {
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout, " NEW_FLOW -> SAT , increment sumSat, numSat\n");
#endif */ // LAVANYA_DEBUG
    this->sumSat = upd.oldSumSat + (newAlloc * copyWeight);
    this->numSat = upd.oldNumSat + (1 * copyWeight);
  } 
  if (oldLabel == SPERCLocalLabel::UNSAT and
      newLabel == SPERCLocalLabel::SAT) {
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout, " UNSAT -> SAT , increment sumSat, numSat\n");
#endif */ // LAVANYA_DEBUG
    this->sumSat = upd.oldSumSat + (newAlloc * copyWeight);
    this->numSat = upd.oldNumSat + (1 * copyWeight);
  }

  if (oldLabel == SPERCLocalLabel::SAT) {
    if (newLabel == SPERCLocalLabel::SAT) {
      this->sumSat = (upd.oldSumSat - (oldAlloc * copyWeight) ) + (newAlloc * copyWeight);
/*#ifdef LAVANYA_DEBUG
      {
	double now = Scheduler::instance().clock();
	fprintf(stdout, "%s: %f SAT -> SAT , change sumSat (%f - %f) + %f = %f\n", 
		log_prefix().c_str(), now, upd.oldSumSat, oldAlloc, newAlloc, 
		this->sumSat);
      }
#endif */ // LAVANYA_DEBUG
    } else {
      if (newLabel == SPERCLocalLabel::UNSAT or
	  newLabel == SPERCLocalLabel::FINISHED) {
/*#ifdef LAVANYA_DEBUG
      fprintf(stdout, " SAT -> UNSAT/ FINISHED , decrement sumSat, numSat\n");
#endif */ // LAVANYA_DEBUG
	this->sumSat = upd.oldSumSat - (oldAlloc * copyWeight);
	this->numSat = upd.oldNumSat - (1 * copyWeight);
      }
    }
  }

  if (newLabel == SPERCLocalLabel::SAT) {
/*#ifdef LAVANYA_DEBUG	
	fprintf(stdout, " XX -> SAT , update maxSat\n");
#endif */ // LAVANYA_DEBUG
	this->update_max_sat(newAlloc, pkt);
  }

  upd.newSumSat = this->sumSat;
  upd.newNumSat = this->numSat;
  upd.newMaxSat = this->maxSat;
  upd.newNumFlows = this->numFlows;

  upd.capacity = this->get_link_capacity_for_data();
  
  int8_t newIgnoreBit = 0;
  if (rate_greater_than(upd.oldMaxSat, 
      bottleneckRate)) {
    assert(!rate_equal(copyMaxSat, bottleneckRate));
    newIgnoreBit = 1;
  }
  // Log any change to state at switch
  if (!rate_equal(upd.newSumSat, upd.oldSumSat) or
      !rate_equal(upd.newNumSat, upd.oldNumSat) or
      !rate_equal(upd.newMaxSat, upd.oldMaxSat) or
      (upd.newNumFlows != upd.oldNumFlows) or
      (!rate_equal(oldAlloc, newAlloc)) or
      oldLabel != newLabel or
      oldIgnoreBit != newIgnoreBit) {

    std::stringstream ssOld, ssNew;
    switch (oldLabel) {
    case SPERCLocalLabel::UNSAT: ssOld << " UNSAT"; break;
    case SPERCLocalLabel::SAT: ssOld << " SAT"; break;
    case SPERCLocalLabel::FINISHED: ssOld << " FINISHED"; break;
    case SPERCLocalLabel::NEW_FLOW: ssOld << " NEW_FLOW"; break;
    case SPERCLocalLabel::UNDEFINED: ssOld << " UNDEFINED"; break;
    default: ssOld << "N/A";
    }
    switch (newLabel) {
    case SPERCLocalLabel::UNSAT: ssNew << " UNSAT"; break;
    case SPERCLocalLabel::SAT: ssNew << " SAT"; break;
    case SPERCLocalLabel::FINISHED: ssNew << " FINISHED"; break;
    case SPERCLocalLabel::NEW_FLOW: ssNew << " NEW_FLOW"; break;
    case SPERCLocalLabel::UNDEFINED: ssNew << " UNDEFINED"; break;
    default: ssNew << "N/A";
    }  
  
    //#ifdef LAVANYA_DEBUG
  double now = Scheduler::instance().clock();

  if (q_) {
    std::stringstream ss;
    if (first_update_log_) {
    ss << "@update_local_state"
       << " link"
       << " time"
       << " limit_rate"
       << " bottleneck_rate"
       << " max_e"
       << " label_flow"
       << " alloc_flow"
       << " ignore_bit"
       << " numb"
       << " sume"
       << " numsat"
       << " numflows"
       << " old label_flow"
       << " old alloc_flow"
       << " old ignore_bit"
       << " weight"
       << " flow_id"
       << "\n";
      first_update_log_ = false;
  } 
    ss << "@update_local_state " 
       << id()
       << " " << now
       << " " <<  limitRate
       << " " << bottleneckRate
       << " " << upd.oldMaxSat
       << " " << ssNew.str()
       << " " << newAlloc
       << " " << (newIgnoreBit > 0)
       << " " << (upd.newNumFlows - upd.newNumSat)
       << " " << upd.newSumSat
       << " " << upd.newNumSat
       << " " << upd.newNumFlows
       << " " << ssOld.str()
       << " " << oldAlloc
       << " " << (oldIgnoreBit > 0)
       << " " << copyWeight
       << " " << ip->flowid()
       << "\n";

    q_->log_to_channel(ss.str());
  }

  int host_limited = (sch->bottleneck_rates_[4] == 0 and !sch->ignore_bits_[4]);

  if (rate_equal(newAlloc,0) and newLabel != SPERCLocalLabel::FINISHED and !host_limited) {
    std::stringstream ss;
    ss << "@update_local_state " 
       << id()
       << " now " << now
       << " limitRate " <<  limitRate
	 << " bottleneckRate " << bottleneckRate
	 << " old MaxE " << upd.oldMaxSat
	 << " new state " << ssNew.str()
	 << " new alloc " << newAlloc
	 << " new ignore " << (newIgnoreBit > 0)
	 << " new numUnsat " << (upd.newNumFlows - upd.newNumSat)
	 << " new sumSat " << upd.newSumSat
	 << " new numSat " << upd.newNumSat
	 << " new numFlows " << upd.newNumFlows
	 << " old state " << ssOld.str()
	 << " old alloc " << oldAlloc
	 << " old ignore " << (oldIgnoreBit > 0)
	 << " weight" << copyWeight
	 << " flow " << ip->flowid()
	 << "\n";
    
    fprintf(stderr, "Warning!! pkt leaving with new alloc 0 %s", ss.str().c_str());
  }
  }
  }
      
void SPERCLinkState::process_egress(Packet *pkt) {
  // start maxsatTime only after we've seen a control packet TODO: when to stop?
	start_timers();
  hdr_cmn* cmn = hdr_cmn::access(pkt);
  hdr_ip* ip = hdr_ip::access(pkt);
  hdr_sperc* hdr = hdr_sperc::access(pkt);

  // egress link processes forward packet
  // what hop do we use? hops are changed at Link:queue
  // number of forward hops at this point: one at source, one at each link until
  // before classifier: so it excludes next egress link
  // assuming we have vars at src, each link, at dst
  // TODO: check
  int hop = hdr->fwd_hop() - 1;
  // when pkt leaves agent fwd_hop is 1 so at src node fwd_hop is 1, hop = 0
  // when pkt leaves link 0-1 fwd_hop is 2 so at node 1 fwd_hop is 2, hop = 1
  // when pkt leaves link 1-3 fwd_hop is 3 so at node 3 fwd_hop is 3, hop = 2
  // but we're not calling process_forward at node 3.
  // so this works if we index starting at 0 and we have two array slots for 2 links.

  int nodeid = -1;
  if (classifier_) 
    nodeid = classifier_->nodeid_;

  if (!hdr->is_fwd() or hdr->fwd_hop() <= 0 or hdr->rev_hop() > 0) {
    fprintf(stderr, "Error!! SPERCLinkState %s at Classifier at node %d::process_egress() invalid pkt %s (cuz !hdr->is_fwd or hdr_fwd_hop() <= 0 or hdr->rev_hop() > 0) \n",
	    log_prefix().c_str(), nodeid, hdr_sperc_ctrl::get_string(pkt).c_str());
    exit(-1);
  }

  /*#ifdef LAVANYA_DEBUG_PATH
  fprintf(stdout, "SPERCLinkState %s at Classifier at node %d::process_egress for pkt %s  using hop_index %d\n",
	  log_prefix().c_str(), nodeid, hdr_sperc_ctrl::get_string(pkt).c_str(),
	  hop);
  #endif */ // LAVANYA_DEBUG_PATH
  
  link_action(pkt, hop);
}  

void SPERCLinkState::link_action(Packet *pkt, int hop) {
  hdr_cmn *cmn = hdr_cmn::access(pkt);
  hdr_ip* ip = hdr_ip::access(pkt);
  hdr_sperc *sh = hdr_sperc::access(pkt);
  hdr_sperc_ctrl *sch = hdr_sperc_ctrl::access(pkt);

  // Get local state- aggregate and for this flow
  bubble copySumSat = this->sumSat;
  bubble copyNumSat = this->numSat;
  bubble copyNumFlows = this->numFlows;
  bubble copyMaxSat = this->maxSat;
  bubble copyWeight = sch->weight();
  bubble limitRate = sch->limit_rate(hop);
  SPERCLocalLabel oldLabel = static_cast<SPERCLocalLabel>(sch->local_labels(hop));
  bubble oldAlloc = sch->allocations(hop);


  // Fill in stretch regardless of what flow label will be 
  double stretch = 1;
  if (q_) {
    double sperc_cap = q_->get_sperc_capacity(); // already in Gb/s
    double actual_cap = (q_->get_capacity())/1.0e9; // b/s -> Gb/s
    stretch = sperc_cap/actual_cap;
  }
  double old_stretch = sch->stretch_[hop];
  sch->set_stretch(hop, stretch);

  
  if (oldLabel == SPERCLocalLabel::FINISHED) {
    assert(sch->is_exit() and !sh->is_fwd());
    return;
    // nothing to do, we just ignore reverse exit packets
  }

  if (oldLabel == SPERCLocalLabel::UNDEFINED) {
    oldLabel = SPERCLocalLabel::NEW_FLOW;
    oldAlloc = 0;
  }

  // Default values ..
  bubble newAlloc = oldAlloc;
  SPERCLocalLabel newLabel = oldLabel;

  // If flow id leaving, we remove flow from local labels and allocations
  // Destination is going to bounce back exit packet as is, we only
  // process an exit packet on the forward path.
  if (sch->is_exit()) {    
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout, "Link %s processing exit packet of flow %d\n",
	    log_prefix().c_str(), ip->flowid());
#endif */
    assert(sh->is_forward());
    newLabel = SPERCLocalLabel::FINISHED;
    newAlloc = 0;
    sch->set_local_labels(hop, static_cast<int>(newLabel));
    sch->set_allocations(hop, newAlloc);
    // Edge case NEW_FLOW -> FINISHED .. assert local state is neither used nore modified
    int8_t oldIgnoreBit = sch->ignore_bit(hop);
    this->update_local_state(-1, oldIgnoreBit, oldLabel, oldAlloc, newLabel, newAlloc, copyWeight, -1, pkt);
    return;
  }

  // If flow is not leaving, we update packet and link state
  // based on flow's previous label/ alloc, current requested bandwidth
  // and current bottleneck link

  // Compute bottleneck rate treating flow as UNSAT
  if (oldLabel == SPERCLocalLabel::SAT) {
    copySumSat -= oldAlloc * copyWeight;
    copyNumSat -= copyWeight;
  }
  if (oldLabel == SPERCLocalLabel::NEW_FLOW) {
    copyNumFlows += copyWeight;
  }
  bubble availCap = get_link_capacity_for_data();
  bubble remCap = availCap - copySumSat;
  bubble numUnsat = copyNumFlows - copyNumSat;
  if (copyNumFlows <= 0 or numUnsat <= 0) {
  fprintf(stderr, "Error!! adjusted NumFlows is %f, NumSat is %f, NumUnsat is %f, pkt is %s\n",
	  copyNumFlows, copyNumSat, numUnsat, hdr_sperc_ctrl::get_string(pkt).c_str());
    exit(1);
  } else if (not rate_greater_than(remCap, 0)) {
    fprintf(stderr, "Warning!! (Error) Link %s action, flow %d: remCap %f <= 0\n", log_prefix().c_str(), ip->flowid(), remCap);
    //exit(1);
  } 
  bubble bottleneckRate = remCap / numUnsat;
  double bytecnt = 0;
  if (q_) bytecnt = q_->get_bytecnt();
  // fprintf(stdout, 
  //     "Link %s action, flow %d: given cap: %f queue size: %f remCap: %f, numUnsat: %f, bottleneckRate: %f\n",
  //     log_prefix().c_str(), ip->flowid(), availCap, bytecnt, remCap, numUnsat, bottleneckRate);     
  this->lastComputedResidualLevel = bottleneckRate;
  
  // Find out whether flow is bottlenecked here or elsewhere and allocate bandwidth
  if (limitRate != -1 and rate_greater_than(bottleneckRate, limitRate)) {
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout, "Link %s: residuaLevel %f is greater than requestedBandwidth %f, so flow should be SAT\n",
	    log_prefix().c_str(), bottleneckRate, limitRate);
#endif */ // LAVANYA_DEBUG
    newLabel = SPERCLocalLabel::SAT;
    if (oldLabel != newLabel)
      sch->set_local_labels(hop, static_cast<int>(newLabel));
    if (!rate_equal(oldAlloc, limitRate)) {
/*#ifdef LAVANYA_DEBUG
      fprintf(stdout, "Link %s: oldAlloc %f is not equal to requestedBandwidth %f, so update alloc in pkt to requestedBandwidth\n", 
	      log_prefix().c_str(), oldAlloc, limitRate);
#endif */ // LAVANYA_DEBUG
      sch->set_allocations(hop, limitRate);
      newAlloc = limitRate;
    }
    // if (rate_greater_than(newAlloc, copyMaxSat)) {
    //   copyMaxSat = newAlloc;
    // }
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout,
	    "Link %s: link_action on packet of flow %d labeled SAT at %f (hop=%d), pkt: %s\n",
	    log_prefix().c_str(), ip->flowid(), newAlloc, hop, hdr_sperc_ctrl::get_string(pkt).c_str());
#endif */ // LAVANYA_DEBUG
    int8_t oldIgnoreBit = sch->ignore_bit(hop);
    this->update_local_state(limitRate, oldIgnoreBit, oldLabel, oldAlloc, newLabel, newAlloc, copyWeight, bottleneckRate, pkt);
  } else { 
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout, "Link %s: residuaLevel %f is not greater than requestedBandwidth %f, so flow should be UNSAT at residual level\n", 
	    log_prefix().c_str(), bottleneckRate, limitRate);
#endif */ // LAVANYA_DEBUG
    
    // Flow should be labeled UNSAT at bottleneckRate
    newLabel = SPERCLocalLabel::UNSAT;
    
    if (oldLabel != newLabel)
      sch->set_local_labels(hop, newLabel);	
    if (!rate_equal(oldAlloc, bottleneckRate)) {
/*#ifdef LAVANYA_DEBUG
      fprintf(stdout, "Link %s: oldAlloc %f is not equal to bottleneckRate %f, so update alloc in pkt to bottleneckRate\n",
	      log_prefix().c_str(), oldAlloc, bottleneckRate);
#endif */ // LAVANYA_DEBUG
      sch->set_allocations(hop, bottleneckRate);
      newAlloc = bottleneckRate;
    }
/*#ifdef LAVANYA_DEBUG
    fprintf(stdout,
	    "Link %s: link_action on packet of flow %d labeled UNSAT at %f (hop=%d), pkt: %s\n",
	    log_prefix().c_str(), ip->flowid(), newAlloc, hop, hdr_sperc_ctrl::get_string(pkt).c_str());
#endif */ // LAVANYA_DEBUG
    int8_t oldIgnoreBit = sch->ignore_bit(hop);
    this->update_local_state(limitRate, oldIgnoreBit, oldLabel, oldAlloc, newLabel, newAlloc, copyWeight, bottleneckRate, pkt);
  }

  int arg_host = MAX_HOPS - 1;
  int host_limited = (sch->bottleneck_rates_[arg_host] == 0 and !sch->ignore_bits_[arg_host]);
  if (rate_equal(newAlloc,0) and !host_limited) {
    fprintf(stderr, "Warning!! %lf link %s: pkt leaving SPERC LinkState (hop=%d) w alloc 0 pkt: %s, limitRate %f, bottleneckRate %f, remCap %f, numUnsat %f, numFlows %f, sumSat %f, availCap %f\n", 
	    Scheduler::instance().clock(), log_prefix().c_str(), hop, hdr_sperc_ctrl::get_string(pkt).c_str(), limitRate, bottleneckRate, remCap, numUnsat, copyNumFlows, copySumSat, availCap);
    //exit(1);
  }

  assert(!sch->is_exit());

  
  sch->set_bottleneck_rate(hop, bottleneckRate);

  // Calculate ignore bit
  if (rate_greater_than(copyMaxSat, bottleneckRate)) {
    assert(!rate_equal(copyMaxSat, bottleneckRate));
    sch->set_ignore_bit(hop);
  } else {
    sch->clear_ignore_bit(hop);
  }

  if (not rate_greater_than(copyMaxSat, bottleneckRate) and bottleneckRate <= 0) {
    fprintf(stderr, 
      "Error!! %lf link %s: pkt leaving SPERC LinkState (hop=%d) w propagated rate <= 0 pkt: %s",
         Scheduler::instance().clock(), log_prefix().c_str(), hop, hdr_sperc_ctrl::get_string(pkt).c_str());
    exit(1);
  }  

  // Reset maxsat if it's time, done using timer
  return;
}
  
void SPERCLinkState::update_max_sat(bubble alloc, Packet *pkt) {
    if (rate_greater_than(alloc, this->maxSat)) {
      this->maxSat = alloc;
    }
    if (rate_greater_than(alloc, this->nextMaxSat)) {
      this->nextMaxSat = alloc;
    }
}

void SPERCLinkState::reset_maxrtt_timeout() {
  num_reset_maxrtt_timeouts++;
  double oldMaxRtt = this->crl.maxRtt;
  double oldNextMaxRtt = this->crl.nextMaxRtt;

  this->lastMaxRttTimeout = now();
  this->crl.maxRtt = this->crl.nextMaxRtt;
  this->crl.nextMaxRtt = 0;

  fprintf(stdout, "%s: %fs reset maxRtt %f -> %f us\n",
	  log_prefix().c_str(), now(),\
	  oldMaxRtt*1e6, crl.maxRtt*1e6);

  double oldMaxFutureRtt = this->crl.maxFutureRtt;
  double oldNextMaxFutureRtt = this->crl.nextMaxFutureRtt;

  this->crl.maxFutureRtt = this->crl.nextMaxFutureRtt;
  this->crl.nextMaxFutureRtt = 0;

  fprintf(stdout, "%s: %fs reset maxFutureRtt %f -> %f us\n",
	  log_prefix().c_str(), now(),\
	  oldMaxFutureRtt*1e6, crl.maxFutureRtt*1e6);

  // maxsat_timeout_ decreases here
  adjust_maxsat_timeout();
  if (!stopped and maxrttTimer and classifier_ and first_pkt_seen_) {
    maxrttTimer->resched(classifier_->SPERC_MAXRTT_TIMEOUT_);
  }
}

void SPERCLinkState::adjust_maxsat_timeout() {
  // we start with some initial value -1 for the maximum RTT
  // we look at actual RTTs over some duration (maxtttTimeout_)
  // this way we can get an upper bound to the actual RTT
  // at any time after the first SPERC_MAXRTT_TIMEOUT_

  // we also look at the actual number of flows
  // this can determine the RTT completely when there are too
  // many flows, we get an upper bound to this number as well

  // maxsat_timeout should be the bigger of these two numbers

  // if there is a flash crowd, we want to use the number of flows
  // to adjust maxsat timeout proactively


  double elapsed = -1;
  if (lastMaxSatTimeout > 0) elapsed = now() - lastMaxSatTimeout;
  double oldMaxsatTimeout = classifier_->SPERC_MAXSAT_TIMEOUT_;

  double bestMaxRtt = crl.maxRtt;
  if (bestMaxRtt < crl.maxFutureRtt) bestMaxRtt = crl.maxFutureRtt;

  // decrease maxsat Tiemout only once we've seen every flow's packet
  // re-schedule maxsat timeout if timeout interval changes 
  if (bestMaxRtt > 0 and classifier_->SPERC_MAXSAT_TIMEOUT_ > 2 * bestMaxRtt\
    and num_reset_maxrtt_timeouts > 0) {
    classifier_->SPERC_MAXSAT_TIMEOUT_ /= 2.0;
    //#ifdef LAVANYA_DEBUG
    fprintf(stdout,\
	    "%s: %fs adjust maxsatTimeout %f -> %f us. Old value too big. 2 * maxRtt (= %f us). %f us elapsed.\n",
	    log_prefix().c_str(), now(), 
	    oldMaxsatTimeout*1e6, classifier_->SPERC_MAXSAT_TIMEOUT_*1e6, 
	    2 * crl.maxRtt * 1e6, elapsed*1e6);
    //#endif
    // classifier timeout just decreased, see if we can timeout already?
    if (elapsed > 0 and elapsed > classifier_->SPERC_MAXSAT_TIMEOUT_) {
      if (maxsatTimer) maxsatTimer->force_cancel();
      maxsatTimer->sched(0);
    }
  } else if (bestMaxRtt > 0 and classifier_->SPERC_MAXSAT_TIMEOUT_ <  bestMaxRtt) {
    classifier_->SPERC_MAXSAT_TIMEOUT_ *= 2;

    //#ifdef LAVANYA_DEBUG
    fprintf(stdout,\
	    "%s: %lf adjust maxsatTimeout %f -> %f us. Old value too small. maxRtt (= %f us). %f us elapsed.\n",
	    log_prefix().c_str(), now(), 
	    oldMaxsatTimeout*1e6, classifier_->SPERC_MAXSAT_TIMEOUT_*1e6, 
	    2 * crl.maxRtt * 1e6, elapsed*1e6);
    //#endif
    // classifier timeout just increased, re-schedule to new timeout since last one
    if (elapsed > 0 and classifier_->SPERC_MAXSAT_TIMEOUT_ > elapsed) {
      if (maxsatTimer) maxsatTimer->force_cancel();
      maxsatTimer->sched(classifier_->SPERC_MAXSAT_TIMEOUT_ - elapsed);
    }

  }
}

void SPERCLinkState::reset_maxsat_timeout() {
  this->lastMaxSatTimeout = now();
  double oldMaxSat = this->maxSat;
  double oldNextMaxSat = this->nextMaxSat;

  this->maxSat = this->nextMaxSat;
  this->nextMaxSat = 0;

  if (!rate_equal(oldMaxSat, this->maxSat) or
      !rate_equal(oldNextMaxSat, this->nextMaxSat)) {
    double now = Scheduler::instance().clock();
    if (q_) {
      std::stringstream ss;
      if (first_reset_log_) {
	ss << "#reset_maxsat"
	   << " link"
	   << " time"
	   << " maxsat"
	   << " next_maxsat"
	   << " old maxsat"
	   << " old next_maxsat"
	   << "\n";
	first_reset_log_ = false;
      }
    
      ss << "#reset_maxsat "
	 << id()
	 << " " << now
	 << " " << this->maxSat
	 << " " << this->nextMaxSat
	 << " " << oldMaxSat
	 << " " << oldNextMaxSat
	 << "\n";

      q_->log_to_channel(ss.str());

    }
  }
  if (!stopped and maxsatTimer and classifier_ and first_pkt_seen_) {
    maxsatTimer->resched(classifier_->SPERC_MAXSAT_TIMEOUT_);
  }
}

double SPERCLinkState::get_link_capacity_for_control() {
  if (!classifier_) {
    fprintf(stderr, "Error!! get_link_capacity_for_control: classifier_ is not initialized.\n");
    exit(1);
  }
  if (!q_) {
    fprintf(stderr, "Error!! get_link_capacity_for_control: q_ is not initialized.\n");
    exit(1);
  }

  double link_capacity = (q_->get_capacity())/1.0e9; // get_capacity() is in b/s

  double fraction_control = (classifier_->SPERC_CONTROL_TRAFFIC_PC_/100.0);
  if (fraction_control <= 0 || fraction_control > 1) {
    fprintf(stderr, 
	    "Error!! fraction_control in invalid- controlTrafficPc: %.2f, headroomPc: %.2f, fraction_control: %.2f, link_capacity: %.2f.\n", 
	   classifier_-> SPERC_CONTROL_TRAFFIC_PC_, classifier_->SPERC_HEADROOM_PC_, fraction_control, link_capacity);
    exit(1);
  } else if (link_capacity <= 0) {
    fprintf(stderr, "Error!! link_capacity is not positive- link_capacity: %.2f.\n", link_capacity);
    exit(1);
  } 
  return ((bubble) fraction_control * link_capacity);
  
}
  
bubble SPERCLinkState::get_link_capacity_for_data() {
  if (!classifier_) {
    fprintf(stderr, "Error!! get_link_capacity_for_data: classifier_ is not initialized.\n");
    exit(1);
  }
  if (!q_) {
    fprintf(stderr, "Error!! get_link_capacity_for_data: q_ is not initialized.\n");
    exit(1);
  }

  double link_capacity = (q_->get_capacity())/1.0e9; // get_capacity() is in b/s
  //double link_capacity = (q_->get_sperc_capacity()); // get_sperc_capacity() is in Gb/s
  //link_capacity = sperc_link_capacity;

  double fraction_data = 1 - (classifier_->SPERC_CONTROL_TRAFFIC_PC_/100.0) - (classifier_->SPERC_HEADROOM_PC_/100.0);

  if (fraction_data <= 0 || fraction_data > 1) {
    fprintf(stderr, 
	    "Error!! fraction_data in invalid- controlTrafficPc: %.2f, headroomPc: %.2f, fraction_data: %.2f, link_capacity: %.2f.\n", 
	   classifier_-> SPERC_CONTROL_TRAFFIC_PC_, classifier_->SPERC_HEADROOM_PC_, fraction_data, link_capacity);
    exit(1);
  } 
  return ((bubble) fraction_data * link_capacity);
  
}
