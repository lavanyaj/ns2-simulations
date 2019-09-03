#include <stdlib.h>
#include <sstream>

#include "config.h"
#include "sperc-hdrs.h"
#include "sperc-routing-host.h"
#include "ip.h"

std::string SPERCRoutingAgent::get_forward_path(Packet *p) {
  // For a dc topo path looks like
  // fwd_path src src tor1 spine tor2 (fwd_hops = 5)
  // rev_path dst dst tor2 spine tor1 (rev_hops = 5)
  // then relate node id to server/ tor/ spine index

  hdr_sperc *hdr = hdr_sperc::access(p);
  std::stringstream path;
  int fwd_hops = hdr->fwd_hop();
  for (int i = 1; i < fwd_hops; i++) {
    path << (int) hdr->fwd_path_[i] << " ";
  }
  return path.str();
}
std::string SPERCRoutingAgent::get_pkt_path(Packet *p) {
  // For a dc topo path looks like
  // fwd_path src src tor1 spine tor2 (fwd_hops = 5)
  // rev_path dst dst tor2 spine tor1 (rev_hops = 5)
  // then relate node id to server/ tor/ spine index

  hdr_sperc *hdr = hdr_sperc::access(p);
  std::stringstream path;
  int fwd_hops = hdr->fwd_hop();
  for (int i = 1; i < fwd_hops; i++) {
    path << (int) hdr->fwd_path_[i] << " ";
  }
  path << (int) hdr->rev_path_[0];

  path << " / ";

  int rev_hops = hdr->rev_hop();
  for (int i = 1; i < rev_hops; i++) {
    path << (int) hdr->rev_path_[i] << " ";
  }
  path << (int) hdr->fwd_path_[0];

  return path.str();
}

std::string SPERCRoutingAgent::get_path_id(Packet *p) {
  // For a dc topo path looks like
  // fwd_path src src tor1 spine tor2 (fwd_hops = 5)
  // rev_path dst dst tor2 spine tor1 (rev_hops = 5)
  // then relate node id to server/ tor/ spine index

  hdr_sperc *hdr = hdr_sperc::access(p);
  std::stringstream path;
  int fwd_hops = hdr->fwd_hop();
  for (int i = 1; i < fwd_hops; i++) {
    path << (int) hdr->fwd_path_[i] << "->";
  }
  path << (int) hdr->rev_path_[0];
  return path.str();
}

void SPERCRoutingAgent::populate_route_recvd(Packet *p) {
  hdr_sperc *rh = hdr_sperc::access(p);
  // fill in info about current path for received packet
  if (rh->is_fwd()) {
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "SPERCAgent at node %d: recv_data(). for recvd (fwd) pkt, we set fwd_path_[%d] to %d and increment fwd_path to %d  pkt: %s\n", nodeid(),
      rh->fwd_hop_, nodeid(), rh->fwd_hop_+1,
      hdr_sperc_ctrl::get_string(p).c_str());
#endif */ // LAVANYA_DEBUG_PATH	  
    rh->fwd_path_[rh->fwd_hop_] = nodeid();
    rh->fwd_hop_++;
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "after change pkt is %s\n", hdr_sperc_ctrl::get_string(p).c_str());
#endif */ // LAVANYA_DEBUG_PATH
  } else {
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "SPERCAgent at node %d: recv_data(). for recvd (rev) pkt,  we set rev_path_[%d] to %d and increment rev_hop to %d  pkt: %s\n", nodeid(),
      rh->rev_hop_, nodeid(), rh->rev_hop_+1,
      hdr_sperc_ctrl::get_string(p).c_str());
#endif */ // LAVANYA_DEBUG_PATH	  
    rh->rev_path_[rh->rev_hop_] = nodeid();
    rh->rev_hop_++;
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "after change pkt is %s\n", hdr_sperc_ctrl::get_string(p).c_str());
#endif */ // LAVANYA_DEBUG_PATH
  }
}

void SPERCRoutingAgent::populate_route_new(Packet *send_pkt, Packet *rp) {
  hdr_sperc *send_rh = hdr_sperc::access(send_pkt);

  if (!rp) {
    // initialize forward path
    send_rh->fwd_hop_ = 0;    
    send_rh->fwd_path_[send_rh->fwd_hop_] = nodeid();
    send_rh->fwd_hop_++;
    send_rh->set_is_fwd(true);
  } else if (hdr_sperc::access(rp)->is_fwd()) {    
    hdr_sperc *rh = hdr_sperc::access(rp);
    // copy info about fwd path
    for (int i = 0; i < rh->fwd_hop_; i++) {
      send_rh->fwd_path_[i] = rh->fwd_path_[i];
    }
    send_rh->fwd_hop_ = rh->fwd_hop_;
    // fill in info about reverse path
    send_rh->set_is_fwd(false);
    send_rh->rev_hop_ = 0;
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "SPERCAgent at node %d: recv_ctrl(). for new (rev) pkt we're sending out,  we set rev_path_[%d] to %d and increment rev_hop to %d  pkt: %s\n", nodeid(),
      send_rh->rev_hop_, nodeid(), send_rh->rev_hop_+1,
      hdr_sperc_ctrl::get_string(send_pkt).c_str());
#endif */ // LAVANYA_DEBUG_PATH	  
    send_rh->rev_path_[send_rh->rev_hop_] = nodeid();
    send_rh->rev_hop_++;
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "after change pkt is %s\n", hdr_sperc_ctrl::get_string(send_pkt).c_str());
#endif */ // LAVANYA_DEBUG_PATH
    
  } else {
    // fill in info about forward path
    send_rh->set_is_fwd(true);
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "SPERCAgent at node %d: recv_ctrl(). for new (fwd) pkt we're sending out,  we set fwd_path_[%d] to %d and increment fwd_hop to %d  pkt: %s\n", nodeid(),
      send_rh->fwd_hop_, nodeid(), send_rh->fwd_hop_+1,
      hdr_sperc_ctrl::get_string(send_pkt).c_str());
#endif */ // LAVANYA_DEBUG_PATH	  
    
    send_rh->fwd_hop_ = 0;
    send_rh->fwd_path_[send_rh->fwd_hop_] = nodeid();
    send_rh->fwd_hop_++;
/*#ifdef LAVANYA_DEBUG_PATH
    fprintf(stdout, "after change pkt is %s\n", hdr_sperc_ctrl::get_string(send_pkt).c_str());
#endif */ // LAVANYA_DEBUG_PATH
  }
}
