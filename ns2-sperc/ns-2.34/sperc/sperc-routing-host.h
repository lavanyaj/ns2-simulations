/*
 * sperc-routing-host.h
 *
 */

#ifndef ns_sperc_routing_host_h
#define ns_sperc_routing_host_h


#include "config.h"

class SPERCRoutingAgent  {
 protected:
  // call this when we receive a packet
  virtual void populate_route_recvd(Packet* p);
  // cal this when we are about to send a packet
  virtual void populate_route_new(Packet* p, Packet* rp);
  virtual int nodeid() {return -1;}; // must be overriden
  std::string get_forward_path(Packet *p);
  std::string get_pkt_path(Packet *p);
  std::string get_path_id(Packet *p);
};

#endif // ns_sperc_routing_h
