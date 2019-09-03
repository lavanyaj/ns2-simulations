/*
 * rcp-routing-host.h
 *
 */

#ifndef ns_rcp_routing_host_h
#define ns_rcp_routing_host_h

#include <string>
#include "config.h"

class RCPRoutingAgent  {
 protected:
  // call this when we receive a packet
  virtual void populate_route_recvd(Packet* p);
  // cal this when we are about to send a packet
  virtual void populate_route_new(Packet* p, Packet* rp);
  virtual int nodeid() {return -1;}; // must be overriden
  std::string get_pkt_path(Packet *p);
};

#endif // ns_rcp_routing_h
