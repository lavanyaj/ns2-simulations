/*
 * sperc-host.h
 *
 */

#ifndef ns_sperc_control_host_h
#define ns_sperc_control_host_h


#include "sperc-hdrs.h"
#include <list>
#include <utility> // pair
class SPERCControlAgent  {
 protected:
  SPERCControlAgent();
 virtual bubble get_rate_from_SPERC_ctrl(Packet *pkt);
 virtual void populate_SPERC_ctrl(Packet *p, Packet *rp);
 
 virtual void update_data_rate(double rate) = 0;
 virtual void log_ctrl_fin_sent_at_src() = 0;
 virtual void log_ctrl_fin_sent_at_rcvr() = 0;

 
 virtual int match_demand_to_flow_size() = 0; // value of config param (0 or 1) w same name

 virtual bubble sperc_data_rate()  {return sperc_data_rate_;};
 virtual int send_ctrl_fin() = 0;
 virtual int nodeid() = 0;
 virtual int fid() = 0;
 virtual std::string log_prefix() = 0;
 virtual void reset();
bool ctrl_fin_sent() {return ctrl_fin_sent_;};
bool ctrl_fin_rcvd() {return ctrl_fin_rcvd_;};
protected:
bool ctrl_fin_sent_ = false;
bool ctrl_fin_rcvd_ = false;
int rate_limited_by_ = -1;
bubble sperc_data_rate_ = 0; // in bytes/s
 double SPERC_LINE_RATE_ = 0;
std::pair< double, bubble > first_rate;
std::list<std::pair<double, bubble> > rates;

//virtual std::string name() = 0; // must be overridden
};

#endif // ns_sperc_control_h
