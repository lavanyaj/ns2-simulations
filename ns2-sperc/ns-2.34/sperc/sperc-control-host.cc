#include <stdlib.h>

#include "config.h"
#include "sperc-hdrs.h"
#include "sperc-control.h" // NEW_FLOW
#include "sperc-control-host.h"
#include "ip.h"

SPERCControlAgent::SPERCControlAgent() : rate_limited_by_(-1) {
  first_rate = std::make_pair(-1, -1);
}

bubble SPERCControlAgent::get_rate_from_SPERC_ctrl(Packet *pkt) {
  double now = Scheduler::instance().clock();
  hdr_sperc *rh = hdr_sperc::access(pkt);
  hdr_sperc_ctrl *ch = hdr_sperc_ctrl::access(pkt);
  bool rate_restricted = false;
  bubble new_rate = -1; bubble new_stretch = -1;

  int num_links = rh->fwd_hop()-2; 
  // e.g. flow from s36 to s37 would have fwd_path_ 36 36 t1 37 and fwd_hop()=4 
  // for only two links 36->t1 and t1->37
  // fwd_path src src tor1 spine tor2 dst and fwd_hop()=6 yields num_links=4
  // means there are 4 links/labels src-tor1 tor1-spine spine-tor2 tor2-dst 

  if (num_links <= 0) { 
    fprintf(stderr, "Error!! invalid num_links from fwd_hop");
    exit(1);
  }

  if (rh->fwd_hop() >= MAX_HOPS-1) {
    fprintf(stderr, "Error!! fwd_hop() %d is at least MAX_HOPS %d, %d links. can't use last hop for demand/RTT", 
	    rh->fwd_hop(),  MAX_HOPS,  num_links);
    exit(1);
  }

  int arg_min = -1;
  for (int i = 0; i < num_links; i++) {
    if (arg_min == -1 || SPERCLinkState::rate_greater_than(ch->allocations_[arg_min],
							   ch->allocations_[i])) {
      arg_min = i;
    }
  } 
  if (arg_min < 0) {
    fprintf(stderr, "Can't find a minimum allocation, %d links", num_links);
    exit(1);
  }

  if (arg_min >= 0) {
    new_rate = ch->allocations_[arg_min];
    rate_limited_by_ = arg_min;
  }

  // it is possible that alloc becomes 0 but bottleneck level is never 0

  int arg_host = MAX_HOPS-1;
  bool host_limited = (match_demand_to_flow_size() 
		       and new_rate == ch->bottleneck_rates_[arg_host]
		       and !ch->ignore_bits_[arg_host]);

  // if (match_demand_to_flow_size()) {
  //   fprintf(stderr, 
  // 	   "Error!! We shouldn't be matching demand to flow size\n");
  //   exit(1);
  // }

  if (host_limited) {
    rate_limited_by_ = arg_host;
    new_rate = SPERC_LINE_RATE_;
  }

  if (!ch->is_exit() and new_rate <= 0) {
    if (!host_limited) {
      if (ch->limit_rate(arg_host) <= 0 and ch->limit_rate(arg_host) != -1) {
	fprintf(stderr, 
		"Error!! %s: didn't get rate from pkt %s\n",
		log_prefix().c_str(), 
		hdr_sperc_ctrl::get_string(pkt).c_str());
	exit(1);
      }
    }
  } 

  if (!ch->is_exit() and new_rate <= 0) {
    fprintf(stderr, 
	    "Warning!! %s: got rate <=0 %f from pkt, using 0.0001 instead %s\n",
	    log_prefix().c_str(), new_rate,
	    hdr_sperc_ctrl::get_string(pkt).c_str());
  }


  if (new_rate <= 0 or ch->is_exit()) { 
    new_rate = 0.0001; 
    rate_restricted = true;
  }
    

  // this actually updates the sperc_data_interval_ used for rate limiting
  update_data_rate((double) new_rate);
  // this is just for logging
  return new_rate;
}

 // modeling an additional link at the end host to account for short flow demand
//  int hop = hdr->fwd_hop() - 1;
//  sch->set_bottleneck_rate(hop, bottleneckRate);
//  sch->clear_ignore_bit(hop);


void SPERCControlAgent::populate_SPERC_ctrl(Packet *send_pkt, Packet *pkt) {
 // Populate control specific fields of control packet
  hdr_sperc_ctrl *send_sch = hdr_sperc_ctrl::access(send_pkt);
  send_sch->is_exit() = false;
  send_sch->weight() = 1;

  if (!pkt) {
    // local labels are all new, local allocations are all 0
    for (int i = 0; i < MAX_HOPS; i++) {
      send_sch->set_local_labels(i, SPERCLocalLabel::NEW_FLOW);
      send_sch->set_allocations(i, 0);
      send_sch->set_bottleneck_rate(i, 0);
      send_sch->set_ignore_bit(i);
    }
  }
 
  // received pkt is not null
  if (pkt) {
    hdr_sperc_ctrl  *sch = hdr_sperc_ctrl::access(pkt);   
    // copy all fields from received pkt
    send_sch->weight() = sch->weight();
    // is exit we copy separately
    for (int i = 0; i < MAX_HOPS; i++) {
      send_sch->set_local_labels(i, sch->local_labels(i));
      send_sch->set_allocations(i, sch->allocations(i));
      send_sch->set_bottleneck_rate(i, sch->bottleneck_rate(i));
      if (sch->ignore_bit(i)) send_sch->set_ignore_bit(i);
      else send_sch->clear_ignore_bit(i);
    }
  }

  // check if it's time to exit, this can only happend at sender
  if (send_ctrl_fin() and !ctrl_fin_sent()) {
    send_sch->is_exit() = true;
    ctrl_fin_sent_ = true;
    log_ctrl_fin_sent_at_src();
  }

  // if received packet was is_exit, this can happen at receiver and at sender
  if (pkt and hdr_sperc_ctrl::access(pkt)->is_exit()) {
    ctrl_fin_rcvd_ = true;
    send_sch->is_exit() = true;

    // this can happen only at receiver, it recvs is_exit before sending
    if (not ctrl_fin_sent_) { 
      ctrl_fin_sent_ = true;
      log_ctrl_fin_sent_at_rcvr();
    }
  }

  return;
}


void SPERCControlAgent::reset()
{
  ctrl_fin_sent_ = false;
  ctrl_fin_rcvd_ = false;
  sperc_data_rate_ = 0;
  first_rate = std::make_pair(-1, -1);


  rates.clear();
}


