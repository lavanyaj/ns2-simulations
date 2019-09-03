#include <stdlib.h>
#include <sstream>

#include "config.h"
#include "rcp-host.h"
#include "rcp-routing-host.h"
#include "ip.h"

std::string RCPRoutingAgent::get_pkt_path(Packet *p) {
  // For a dc topo path looks like
  // fwd_path src src tor1 spine tor2 (fwd_hops = 5)
  // rev_path dst dst tor2 spine tor1 (rev_hops = 5)
  // then relate node id to server/ tor/ spine index

  hdr_rcp *hdr = hdr_rcp::access(p);
  std::stringstream path;
  int fwd_hops = hdr->fwd_hop();
  for (int i = 1; i < fwd_hops; i++) {
    path <<  hdr->fwd_path_[i] << " ";
  }

  path <<  hdr->rev_path_[0];
  path << " / ";

  int rev_hops = hdr->rev_hop();
  for (int i = 1; i < rev_hops; i++) {
    path <<  hdr->rev_path_[i] << " ";
  }
  path <<  hdr->fwd_path_[0];

  return path.str();
}
void RCPRoutingAgent::populate_route_recvd(Packet *p) {
  hdr_rcp *rh = hdr_rcp::access(p);
  // fill in info about current path for received packet
  if (rh->is_fwd()) {
    rh->fwd_path_[rh->fwd_hop_] = nodeid();
    rh->fwd_hop_++;
  } else {
    rh->rev_path_[rh->rev_hop_] = nodeid();
    rh->rev_hop_++;
  }
}

void RCPRoutingAgent::populate_route_new(Packet *send_pkt, Packet *rp) {
  hdr_rcp *send_rh = hdr_rcp::access(send_pkt);

  if (!rp) {
    // initialize forward path
    send_rh->fwd_hop_ = 0;    
    send_rh->fwd_path_[send_rh->fwd_hop_] = nodeid();
    send_rh->fwd_hop_++;
    send_rh->is_fwd() = true;
  } else if (hdr_rcp::access(rp)->is_fwd()) {    
    hdr_rcp *rh = hdr_rcp::access(rp);
    // copy info about fwd path
    for (int i = 0; i < rh->fwd_hop_; i++) {
      send_rh->fwd_path_[i] = rh->fwd_path_[i];
    }
    send_rh->fwd_hop_ = rh->fwd_hop_;
    // fill in info about reverse path
    send_rh->is_fwd() = false;
    send_rh->rev_hop_ = 0;
    send_rh->rev_path_[send_rh->rev_hop_] = nodeid();
    send_rh->rev_hop_++;    
  } else {
    // fill in info about forward path
    send_rh->is_fwd() = true;    
    send_rh->fwd_hop_ = 0;
    send_rh->fwd_path_[send_rh->fwd_hop_] = nodeid();
    send_rh->fwd_hop_++;
  }
}
