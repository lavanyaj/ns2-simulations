/*
 * sperc-control.h
 *
 */

#ifndef ns_sperc_control_h
#define ns_sperc_control_h

#include <string> // id
#include <sstream> // errMsg
#include <memory> // for std::unique_ptr, std::make_unique
#include "classifier-hash.h"
//#include "config.h"
//#include "object.h"
//#include "agent.h"
#include "timer-handler.h"
//#include "ip.h"
#include "sperc-hdrs.h"
#include "sperc.h"
//#include "sperc-host.h"
//#include <set>
//#include <utility>
//#include <vector>

class SPERCLinkState;

class SPERCLinkStateTimer : public TimerHandler { 
 public:
 SPERCLinkStateTimer(SPERCLinkState *a, void (SPERCLinkState::*call_back)() ) : a_(a), call_back_(call_back) {};
protected:
virtual void expire (Event *e);
SPERCLinkState *a_;
void (SPERCLinkState::*call_back_)();

}; 

class SPERCDestHashClassifier;


// bi-directional
struct ControlRateLimiting {
  int numFlows;
  double maxRtt;
  double nextMaxRtt;
  double maxFutureRtt;
  double nextMaxFutureRtt;
  ControlRateLimiting();
};

class SPERCLinkState {

friend SPERCLinkStateTimer;
 std::unique_ptr<SPERCLinkStateTimer> maxsatTimer;
 std::unique_ptr<SPERCLinkStateTimer> maxrttTimer;
 SPERCQueue* q_;
 double lastMaxSatTimeout;
 double lastMaxRttTimeout;
public:
SPERCLinkState();
// maybe we declare these in classifier/Hash/DestHash/SPERC
// and bind them to Tcl defaults
 double now() {return Scheduler::instance().clock();}
 void initialize(bubble linkCapacity, SPERCDestHashClassifier* classifier, int fromNode, int toNode, SPERCQueue* q);
 void stop();
 //,  double controlTrafficPc, double headroomPc, double minRtt, double initialMaxRtt, rate_t initialControlPacketRate, double maxsatTimeout);
 void start_timers();
 void process_for_control_rate_limiting(Packet * pkt);
 void reset_maxrtt_timeout();
 void adjust_maxsat_timeout();
 void process_ingress(Packet * pkt);
 void process_egress(Packet * pkt);
 void link_action(Packet * pkt, int hop);
 void update_local_state(bubble limitRate, int8_t oldIgnoreBit, SPERCLocalLabel oldLabel, bubble oldAlloc,
			 SPERCLocalLabel newLabel, bubble newAlloc,
			 bubble copyWeight, bubble residualLevel,
			 Packet * pkt);
 void reset_maxsat_timeout();
 void update_max_sat(bubble alloc, Packet * pkt);

 static bool rate_equal(bubble r1, bubble r2) {
   bubble diff = r1 - r2;
   if (diff < 0) diff = -diff;
   return diff < 0.0001;
 }
 static bool rate_greater_than(bubble r1, bubble r2) {
   return (!rate_equal(r1, r2) && r1 > r2);
 }

 std::string log_prefix();
 double get_link_capacity_for_control();
 bubble get_link_capacity_for_data();
 std::string id() {return id_;}
// State specific to SPERC, initialized in constructor
 bool stopped;

 bool first_update_log_;
 bool first_reset_log_;

 int from_node_;
 int to_node_;

 bubble sumSat;
 bubble numSat;
 bubble numFlows;
 bubble maxSat;
 bubble nextMaxSat;
 // Cache computed values to use when simulator checks rates etc.
 bubble lastComputedResidualLevel;
 bubble lastComputedBottleneckLevel;
 
// Store info about flows that caused maxSat etc. to change, for logging
 bubble lastMaxSatUpdateCuzFlow;
 bubble lastMaxSatUpdateCuzBn;
 bubble lastNextMaxSatUpdateCuzFlow;
 bubble lastNextMaxSatUpdateCuzBn;
 
// Control Packet Rate Limiting and RTT info
 struct ControlRateLimiting crl;
 unsigned num_reset_maxrtt_timeouts;
 bool first_pkt_seen_;
 bool initialized_;
 string id_;

 SPERCDestHashClassifier* classifier_;
};

// To log what a switch does on a control packet
  class SPERCSwitchUpdate {
  public:
    bubble oldSumSat;
    bubble oldNumSat;
    bubble oldNumFlows;
    bubble oldMaxSat;
    bubble newSumSat;
    bubble newNumSat;
    bubble newNumFlows;
    bubble newMaxSat;
    bubble capacity;

  SPERCSwitchUpdate() : oldSumSat(-123), oldNumSat(-123), oldNumFlows(-123), oldMaxSat(-123), newSumSat(-123), newNumSat(-123), newNumFlows(-123), newMaxSat(-123), capacity(-123) {}
    std::string getString() {
        std::stringstream ss;
        if (!SPERCLinkState::rate_equal(oldSumSat, newSumSat)) 
	  ss << "SumSat: " << oldSumSat << " -> " << newSumSat; 
	else ss << "SumSat: " << oldSumSat;
        if (oldNumSat != newNumSat) 
	  ss << ", NumSat: " << oldNumSat << " -> " << newNumSat; 
	else ss << ", NumSat: " << oldNumSat;
        if (oldNumFlows != newNumFlows) 
	  ss << ", NumFlows: " << oldNumFlows << " -> " << newNumFlows; 
	else ss << ", oldNumFlows: " << oldNumFlows;
        if (!SPERCLinkState::rate_equal(oldMaxSat, newMaxSat)) 
	  ss << ", MaxSat: " << oldMaxSat << " -> " << newMaxSat; 	
	else ss << ", MaxSat: " << oldMaxSat;
        ss << ", capacity: " << capacity;
        ss << ".";
        return ss.str();
    }
  };

#endif
