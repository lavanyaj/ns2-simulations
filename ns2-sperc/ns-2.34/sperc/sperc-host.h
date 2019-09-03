/*
 * sperc-host.h
 *
 */

#ifndef ns_sperc_host_h
#define ns_sperc_host_h

#include "rng.h"
#include <cmath>
#include "config.h"
#include "object.h"
#include "agent.h"
#include "sperc-control-host.h"
#include "sperc-routing-host.h"
#include "timer-handler.h"
#include "ip.h"
#include <set>
#include <utility>
#include <vector>


class SPERCAgent;

// For now just a simple agent that sends control packets only

class SPERCATimer : public TimerHandler {
public: 
        SPERCATimer(SPERCAgent *a, 
		    void (SPERCAgent::*call_back)()) : 
  TimerHandler() {
    a_ = a; 
    call_back_=call_back;
  }
protected:
        virtual void expire(Event *e);
        SPERCAgent *a_;
	void (SPERCAgent::*call_back_)();
};

class SPERCAgent : public Agent, SPERCControlAgent, SPERCRoutingAgent {
 public:
        SPERCAgent();
        virtual void ctrl_timeout();
        virtual void timeout();
	virtual void syn_retransmit_timeout();
        /* For retransmissions */
        virtual void retrans_timeout(); // lavanya: is it time to RETRANSMIT?
        virtual void recv(Packet* p, Handler*); // lavanya: acks, ctrl, data
	virtual bool is_spurious(Packet* p);
	virtual void recv_data(Packet* p);
	virtual void recv_ctrl(Packet* p);
        virtual int command(int argc, const char*const* argv);
        //void advanceby(int delta);
        //virtual void sendmsg(int nbytes, const char *flags = 0);
 protected:
	virtual std::string log_prefix();	
	virtual int fid() {return fid_;}
	virtual void update_data_rate(double rate);
	virtual void log_ctrl_fin_sent_at_src();
	virtual void log_ctrl_fin_sent_at_rcvr();

	virtual int match_demand_to_flow_size() {return MATCH_DEMAND_TO_FLOW_SIZE_;}
	virtual double get_rate_from_remaining_pkts();

	virtual void populate_SPERC_base_data(Packet *p, Packet *rp, SPERC_PKT_T type);
	virtual void populate_SPERC_cmn_data(Packet *p);
	virtual void populate_SPERC_cmn_data_header(Packet *p);
	virtual void populate_SPERC_base_ctrl(Packet *p, Packet *rp);
	virtual void populate_SPERC_cmn_ctrl(Packet *p, Packet *rp);


	// logs RCP and SPERC rates for control and data pkts to I/O
	virtual void log_rates(); 
	virtual void log_rates_periodic();
	// logs RTT and packets received from ACKs (data)
	void log_ack(Packet*);
        virtual void start();
	virtual void reset(); // tcl reset cmd to reuse this host for a new flow
	// overrides Object::reset, ControlHost::reset
	// start sending
	virtual void sendfile(); 
	virtual void send_first_packet(); // actual put packets on the wire
	virtual double get_weight();
	virtual int get_prio();
	// send regular packet
        virtual void sendpkt();
	// send FIN
        virtual void sendlast();
	// when all data to send is acked or stop()	
        virtual void stop_data();
	virtual void stop_ctrl();
        virtual void finish(); 

	// Rate limiting
	void ctrl_rate_change();
        void rate_change();
	virtual double SPERC_desired_rate();
	virtual int nodeid() {return nodeid_;};	

	inline double min(double d1, double d2){ if (d1 < d2){return d1;} else {return d2;} }

	int nodeid_;
	char pathid_[4096];
	double start_time_;

	// To compare how much bandwidth was allocated v/s used
	double last_rate_;
	double last_rate_change_time_;
	double bytes_scheduled_;
	double bytes_sent_;
	
	double next_timeout_;

	bool init_log_;
	double fixed_rtt_;
	double zero_gbps_;
	// used to sched retx by line rate flows

	// never used in normal case cuz min rtt / ctrl_min_rtt_ set by SYNACK
	// / first SPERC_CTRL_REV

	double rtt_; // to schedule re-tx timer
	double min_rtt_; // for logging
	double max_rtt_;  // for logging
	double ctrl_rtt_;
	double min_ctrl_rtt_;
	double max_ctrl_rtt_; // for logging
	unsigned seqno_;
	unsigned ctrl_seqno_;       
	unsigned syn_seqno_; // used for SYN
	int SPERC_state;
	unsigned size_; // bytes on the wire for data packets
	// set to true once all data packets have been acked
	// so control packets can signal links that flow has
	// finished
	bool send_ctrl_fin_ = false;
	// set to true if tcl requests to stop
	// regardless of outstanding packets, only
	// relevant to source. we'll stop_data
	// and not send any more new data packets. 
	// We'll start wrapping up in control plane
	// by setting send_ctrl_fin_ to true.
	// we'll handle outstanding ACKs and 
	// update num_.._acked
	// as long as ACK flow matches our flow id.
	bool stop_requested_ = false;

	// set true to use rate from SPERC headers (hdr_sperc_ctrl),
	// otherwise uses rate from RCP headers (hdr_sperc)
	SPERCRateLimiting sperc_rate_limiting_;

	// state that matters to sender
	unsigned numpkts_;
	unsigned num_sent_;
	unsigned num_Pkts_to_retransmit_;           
	unsigned num_pkts_resent_;                  
	unsigned num_enter_syn_retransmit_mode_;
	unsigned num_enter_retransmit_mode_;        
	unsigned numOutRefs_; 
	unsigned num_dataPkts_acked_by_receiver_;   

	// state that matters to receiver
	unsigned num_dataPkts_received_;            

	// for rate limiting control packets
	double ctrl_packet_interval_;
	double lastctrlpkttime_;
	Packet *nextctrlpacket_;
	unsigned num_ctrl_packets_sent_;
	unsigned num_ctrl_packets_recvd_;

	// rate limiting data packets using SPERC
	double lastpkttime_;

	double sperc_data_interval_;

	// Rate limiters
	SPERCATimer sperc_ctrl_timer_;  
	SPERCATimer sperc_timer_;
	SPERCATimer syn_retransmit_timer_; 
		
	// For retransmissions, start this timer after
	// sending numpkts_, then after sending 
	// num_Pkts_to_retransmit_
	SPERCATimer rto_timer_;

	// For logging rates periodically, that we can sync across different hosts
	SPERCATimer log_timer_;

	// For starting a flow after some jitter
	SPERCATimer jitter_timer_;

	Tcl_Channel   channel_;
	Tcl_Channel   common_channel_;
	bool first_log_rate_change_;
	bool first_log_recv_ack_;
	bool first_log_rate_periodic_;

	double weight_;
	int prio_;

	double first_ack_; // to log path when we get first ack
	double get_num_pkts_this_rtt_at_gbps(double rate);
	double get_num_pkts_this_rtt_at_curr_rate();
	double get_num_pkts_left();

	double SPERC_SYN_RETX_PERIOD_SECONDS_;
	double SPERC_FIXED_RTT_ESTIMATE_AT_HOST_;
	unsigned SPERC_CONTROL_PKT_HDR_BYTES_;
	unsigned SPERC_DATA_PKT_HDR_BYTES_;
	double SPERC_INIT_SPERC_DATA_INTERVAL_;
	double MATCH_DEMAND_TO_FLOW_SIZE_;
	double SPERC_PRIO_WORKLOAD_;
	double SPERC_PRIO_THRESH_INDEX_;
	double SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES_;
	double SPERC_JITTER_;
	/* double SYN_RETX_PERIOD_SECONDS_; */
	/* double FIXED_RTT_ESTIMATE_AT_HOST_;  */
	/* double SPERC_CTRL_PKT_HDR_BYTES_; */
	/* double SPERC_DATA_PKT_HDR_BYTES_; */
	/* double INIT_SPERC_DATA_INTERVAL_; */
	/* int MATCH_DEMAND_TO_FLOW_SIZE_; */
	/* int PRIO_THRESH_INDEX_ = 1; */
	/* double PRIO_WORKLOAD_; */
	RNG jitter_;
	inline virtual int send_ctrl_fin() {
	  return send_ctrl_fin_;
	}
	inline virtual int using_sperc_rate() {
	  return sperc_rate_limiting_ == SPERC_RATE_LIMITED;
	}
	inline virtual int using_init_rate() {
	  return sperc_rate_limiting_ == INIT_RATE_LIMITED;
	}
	inline virtual int using_init_rate_until_first_sperc_rate() {
	  return sperc_rate_limiting_ == INIT_RATE_LIMITED_UNTIL_FIRST_SPERC_RATE;
	}

	double interval();

};

#endif
