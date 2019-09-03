/*
 * rcp-host.h
 *
 */

#ifndef ns_rcp_h
#define ns_rcp_h

#include "config.h"
#include "object.h"
#include "agent.h"
#include "timer-handler.h"
#include "ip.h"
#include "rcp-routing-host.h"

enum RCP_PKT_T {RCP_OTHER, 
		RCP_SYN, 
		RCP_SYNACK, 
		RCP_DATA,
		RCP_REF,
		RCP_REFACK,
		RCP_ACK,
		RCP_FIN,
		RCP_FINACK};

enum RCP_HOST_STATE {RCP_INACT,
		     RCP_SYNSENT, 
		     RCP_RUNNING,
		     RCP_FINSENT,
		     RCP_RETRANSMIT};

extern const char* RCP_PKT_T_STR[9];
extern const char* RCP_HOST_STATE_STR[7];
struct hdr_rcp {
  unsigned seqno_;
  
  int RCP_enabled_;
  int RCP_pkt_type_;
  double RCP_rate_;  // in bytes per second
  
  double rtt_;
  double ts_;
  // lavanya: note to self: where is this offset_ initialized?  
  static int offset_;
  
  unsigned num_dataPkts_received;
  // useful for retransmission

  unsigned numpkts_;
  // to track number of active flows, when link sees an ACK with num_dataPkts_received_ == numpkts_, flow's ended (assuming ack won't be dropped)
  // where active is defined as a link has seen a SYN from the flows but not an ACK acknowledging all pkts of flows have been received

  int flowId;

  // lavanya: added this so I can check that fwd path = rev path for pkts
  bool is_fwd_ = true;
  int fwd_path_[10] = {0};    
  int fwd_hop_ = 0;
  int rev_path_[10] = {0};
  int rev_hop_ = 0;

  inline static int& offset() { return offset_; }
  inline static hdr_rcp* access(const Packet* p) {
    return (hdr_rcp*) p->access(offset_);
  }

  /* per-field member functions */
  inline bool& is_fwd() { return (is_fwd_); }
  inline int& fwd_hop() { return (fwd_hop_); }
  inline int& rev_hop() { return (rev_hop_); }
  inline void set_numpkts(unsigned n) { numpkts_ = n; }
  inline unsigned& numpkts() { return (numpkts_); }

  //u_int32_t& srcid() { return (srcid_); }
  inline unsigned& seqno() { return (seqno_); }
  inline double& ts() { return (ts_); }
  inline double& rtt() { return (rtt_); }
  inline void set_RCP_rate(double rate) { RCP_rate_ = rate; }
  inline double& RCP_request_rate() { return RCP_rate_; }
  inline void set_RCP_pkt_type(int type) { RCP_pkt_type_ = type;}
  inline int& RCP_pkt_type() { return RCP_pkt_type_; }
  inline void set_RCP_numPkts_rcvd(unsigned numPkts) { num_dataPkts_received = numPkts; }
  inline int& flowIden() { return(flowId); }
};


class RCPAgent;

class RCPATimer : public TimerHandler {
public: 
        RCPATimer(RCPAgent *a, void (RCPAgent::*call_back)()) : TimerHandler() {a_ = a;call_back_=call_back;}
protected:
        virtual void expire(Event *e);
	void (RCPAgent::*call_back_)();
        RCPAgent *a_;
};

class RCPAgent : public Agent, RCPRoutingAgent {
 public:
        RCPAgent();
        virtual double now();
        virtual double get_rto();
        virtual void timeout();
	virtual void ref_timeout();
        /* For retransmissions */
        virtual void retrans_timeout();
	/* For probing rates */
	virtual void rate_probe_timeout();
	virtual void cancel_all_timers();
        virtual void recv(Packet* p, Handler*);
        virtual int command(int argc, const char*const* argv);
	virtual int nodeid() {return nodeid_;};	// lavanya: added for debugging pkt path

        //void advanceby(int delta);
        //virtual void sendmsg(int nbytes, const char *flags = 0);
 protected:
	virtual void log_ack(Packet *p);
	virtual void log_rates();
        virtual void sendpkt();
        virtual void sendlast();
        void rate_change();
        virtual void start();
        virtual void stop();
        virtual void pause();
        virtual void reset();  /* Masayoshi */
	// lavanya: tcl reset cmd to reuse this host for a new flow, overrides Object::reset()
        virtual void finish();
	/* Nandita */ 
	virtual void sendfile(); 
	virtual double RCP_desired_rate();
	inline double min(double d1, double d2){ if (d1 < d2){return d1;} else {return d2;} }

	double SYN_DELAY_;
	double REF_FACT_;  //Number of REFs per RTT
	double RCP_HDR_BYTES_;  //same as TCP for fair comparison
	double PROBE_PKT_BYTES_;
	double QUIT_PROB_;  //probability of quitting if assigned rate is zero
	double REF_INTVAL_; //

	int nodeid_;
	double lastpkttime_;
	double rtt_;
	double min_rtt_;
	double rto_abs_; // rto is exactly this if > 0
	double rto_fact_; // else rto is this times rtt_

	unsigned seqno_;
	unsigned ref_seqno_; /* Masayoshi */
	unsigned extra_probe_seqno_;

	int init_refintv_fix_; /* Masayoshi */
	double interval_;
	unsigned numpkts_;
	unsigned num_sent_;
	int RCP_state;
	unsigned numOutRefs_;

	/* for retransmissions */ 
	unsigned num_dataPkts_acked_by_receiver_;   

	unsigned num_dataPkts_received_; // at destination
	unsigned num_dataPkts_sent_; // at source
	// so we can log data goodput at the end

	unsigned num_Pkts_to_retransmit_;
	unsigned num_pkts_resent_; // since last RTO 
	unsigned num_enter_retransmit_mode_;

	unsigned num_refPkts_received_;  // at destination
	unsigned num_refPkts_sent_; // at source
	// so we can log REF packet goodput at the end
	
	RCPATimer rcp_timer_;
	/* For SYN only, send again at most once */
	RCPATimer ref_timer_;
	/* For retransmissions */ 
	RCPATimer rto_timer_;
	/* For checking if we shud probe for rate */
	RCPATimer rate_probe_timer_;
	double last_rate_probe_time_;
	double rate_probe_interval_;


	Tcl_Channel channel_;
	bool first_log_recv_ack_; 
	// True until the first log_ack()
	bool first_log_rate_change_; 
	// True until the first log_rate_change()
};


#endif
