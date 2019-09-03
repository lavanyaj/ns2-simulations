/*
 * sperc-hdrs.h
 *
 */

#ifndef ns_sperc_hdrs_h
#define ns_sperc_hdrs_h

#include "config.h"
#include "object.h"
#include "agent.h"
#include "timer-handler.h"
#include "ip.h"
#include <set>
#include <utility>
#include <vector>
#include <string>

extern const char* SPERC_HOST_STATE_STR[8];
extern const char* SPERC_PKT_T_STR[13];
extern const char* SPERCLocalLabelStr[5];
extern const char* SPERCIgnoreBitStr[2];
extern const char* SPERC_RATE_LIMITING_STR[3];
enum SPERCRateLimiting {SPERC_RATE_LIMITED,
			INIT_RATE_LIMITED, 
			INIT_RATE_LIMITED_UNTIL_FIRST_SPERC_RATE};
enum SPERCLocalLabel {UNDEFINED, NEW_FLOW, SAT, UNSAT, FINISHED};
enum SPERCQueueType {UNDEFINED_QUEUE_TYPE, CONTROL_QUEUE_TYPE, DATA_QUEUE_TYPE};

/* enum SPERC_PKT_T {SPERC_OTHER, */
/* 		SPERC_SYN, */
/* 		SPERC_SYNACK, */
/* 		SPERC_REF, */
/* 		SPERC_REFACK, */
/* 		SPERC_DATA, */
/* 		SPERC_ACK, */
/* 		SPERC_FIN, */
/* 		  SPERC_FINACK, */
/* 		  SPERC_CTRL, */
/* 		  SPERC_CTRL_REV, */
/* 		  SPERC_CTRL_SYN, */
/* 		  SPERC_CTRL_SYN_REV}; */

enum SPERC_PKT_T {SPERC_OTHER,
		SPERC_SYN,
		SPERC_SYNACK,
		SPERC_DATA,
		SPERC_ACK,
		SPERC_FIN,
		  SPERC_FINACK,
		  SPERC_CTRL,
		  SPERC_CTRL_REV,
		  SPERC_CTRL_SYN,
		  SPERC_CTRL_SYN_REV};

/* enum SPERC_HOST_STATE {SPERC_INACT, */
/* 		     SPERC_SYNSENT,  */
/* 		     SPERC_CONGEST, */
/* 		     SPERC_RUNNING, */
/* 		     SPERC_RUNNING_WREF, */
/* 		     SPERC_FINSENT, */
/* 		     SPERC_RETRANSMIT}; */

enum SPERC_HOST_STATE {SPERC_INACT,
		       SPERC_SYNSENT, 
		       SPERC_SYNACK_RECVD,  
		       SPERC_RUNNING,
		       SPERC_FINSENT,
		       SPERC_RETRANSMIT};

typedef float bubble;
//typedef char int8_t;
typedef std::pair< unsigned, unsigned > uint_pair_t;
#define MAX_HOPS 20

/*
  FYI header sizes based on c++ struct (TODO: keep this info in sync)
  offset of PacketHeader/Common is 0, size in bytes is 128
  offset of PacketHeader/IP is 128, size in bytes is 28
  offset of PacketHeader/SPERC is 160, size in bytes is 48 + 32B (fwd_path)
  offset of PacketHeader/SPERC_CTRL is 208, size in bytes is 96
*/
#define CMN_HDR_BYTES 128
#define IP_HDR_BYTES 28
#define SPERC_CMN_HDR_BYTES 80
#define SPERC_CTRL_HDR_BYTES 96 
#define SPERC_DATA_PKT_HDR_BYTES  (CMN_HDR_BYTES+IP_HDR_BYTES+SPERC_CMN_HDR_BYTES)
#define SPERC_CTRL_PKT_HDR_BYTES (CMN_HDR_BYTES+IP_HDR_BYTES+SPERC_CMN_HDR_BYTES+SPERC_CTRL_HDR_BYTES)

struct hdr_sperc_ctrl {
  unsigned seqno_;
  bubble stretch_[MAX_HOPS];
  bubble allocations_[MAX_HOPS];  // in bytes per second (when double)
  int8_t local_labels_[MAX_HOPS];
  int8_t ignore_bits_[MAX_HOPS];
  bubble bottleneck_rates_[MAX_HOPS];

  //bubble current_requested_bandwidth_;
  //int current_bottleneck_;
  int8_t weight_;
  bool is_exit_;
  static int offset_;
  //bubble min_fair_shares_[MAX_HOPS];

  inline static int& offset() { return offset_; }
  inline static hdr_sperc_ctrl* access(const Packet* p) {
    return (hdr_sperc_ctrl*) p->access(offset_);
  }

  /* per-field member functions */
  // maybe we'll need more fields for source routing??
  inline unsigned& seqno() {return(seqno_); }
  inline bubble& allocations(unsigned index) { return(allocations_[index]);}
  //  inline bubble& min_fair_shares(unsigned index) { return(min_fair_shares_[index]);}

  inline int8_t& local_labels(unsigned index) {return(local_labels_[index]);}
  inline int8_t& ignore_bit(unsigned index) {return(ignore_bits_[index]);}
  inline bubble& bottleneck_rate(unsigned index) { return(bottleneck_rates_[index]);}

  bubble limit_rate(unsigned index) {
    bubble e = -1;
    for (unsigned i = 0; i < MAX_HOPS; i++) {
      if (ignore_bits_[i]==0 and i != index) {
	assert(bottleneck_rates_[i] > 0);
	if (e == -1 or bottleneck_rates_[i] < e) {
	  e = bottleneck_rates_[i];
	}
      }
    }
  return e;
  }
  /* inline bubble& current_requested_bandwidth() */
  /* {return current_requested_bandwidth_;} */
  inline bool& is_exit() { return is_exit_;};
  /* inline int& current_bottleneck() {return current_bottleneck_;} */
  inline int8_t& weight() {return weight_;}
  inline void set_allocations(unsigned index, bubble alloc) {
    allocations_[index] = alloc;
  }
  /* inline void set_min_fair_shares(unsigned index, bubble alloc) { */
  /*   min_fair_shares_[index] = alloc; */
  /* } */

  inline void set_stretch(unsigned index, bubble s) {
    stretch_[index] = s;
  }

  inline void set_local_labels(unsigned index, int label) {
    local_labels_[index] = label;
  }

  inline void set_ignore_bit(unsigned index) {
    ignore_bits_[index] = 1;
  }

  inline void clear_ignore_bit(unsigned index) {
    ignore_bits_[index] = 0;
  }

  inline void set_bottleneck_rate(unsigned index, bubble b) {
    bottleneck_rates_[index] = b;
  }

  static std::string get_string(const Packet* p);  
  //  inline void set_current_requested_bandwidth(rate_t request) {
  //  current_requested_bandwidth = request;
  //}
  //  inline void set_current_bottleneck(int8_t bn) {
  //  current_bottleneck = bn;
  //}
};

struct hdr_sperc {
  unsigned seqno_;

  double SPERC_rate_;  // in bytes per second (control packet rate)
  
  int SPERC_enabled_;
  SPERC_PKT_T SPERC_pkt_type_;
  
  double rtt_;
  double ts_;
  static int offset_;
  
  unsigned num_dataPkts_received_; // useful for retransmission
  unsigned numpkts_; 
  // to track number of active flows, when link sees an ACK with num_dataPkts_received_ == numpkts_, flow's ended (assuming ack won't be dropped)
  // where active is defined as a link has seen a SYN from the flows but not an ACK acknowledging all pkts of flows have been received

  bool is_fwd_ = true;
  int fwd_path_[MAX_HOPS] = {0};    
  int rev_path_[MAX_HOPS] = {0};
  int fwd_hop_ = 0;
  int rev_hop_ = 0;

  inline static SPERC_PKT_T get_ack_type(SPERC_PKT_T recv) {
    if (recv == SPERC_SYN) return SPERC_SYNACK;
    else if (recv == SPERC_DATA) return SPERC_ACK;
    else if (recv == SPERC_FIN) return SPERC_FINACK;
    else {
      fprintf(stderr, "get_ack_type called with invalid type %d\n", recv);
      return SPERC_OTHER;
    }
  }

  inline static SPERC_PKT_T get_control_rev_type(SPERC_PKT_T recv) {
    if (recv == SPERC_CTRL) return SPERC_CTRL_REV;
    else if (recv == SPERC_CTRL_SYN) return SPERC_CTRL_SYN_REV;
    else if (recv == SPERC_CTRL_SYN_REV) return SPERC_CTRL;
    else if (recv == SPERC_CTRL_REV) return SPERC_CTRL;
    else {
      fprintf(stderr, "get_control_rev_type called with invalid type %d\n", 
	      recv);
      return SPERC_OTHER;
    }
  }

  inline static int& offset() { return offset_; }
  inline static hdr_sperc* access(const Packet* p) {
    return (hdr_sperc*) p->access(offset_);
  }

  /* per-field member functions */
  inline void set_is_fwd(bool b) {  is_fwd_ = b; }
  inline bool& is_fwd() { return (is_fwd_); }
  inline int& fwd_hop() { return (fwd_hop_); }
  inline int& rev_hop() { return (rev_hop_); }
  inline void set_numpkts(unsigned n) { numpkts_ = n; }
  inline unsigned& numpkts() { return (numpkts_); }

  inline unsigned& seqno() { return (seqno_); }
  inline double& ts() { return (ts_); }
  inline double& rtt() { return (rtt_); }
  inline void set_SPERC_rate(double rate) { SPERC_rate_ = rate; }
  inline double& SPERC_request_rate() { return SPERC_rate_; }
  inline void set_SPERC_pkt_type(SPERC_PKT_T type) { SPERC_pkt_type_ = type;}
  inline SPERC_PKT_T& SPERC_pkt_type() { return SPERC_pkt_type_; }
  inline void set_num_dataPkts_received(unsigned num) {num_dataPkts_received_ = num;}
  inline unsigned& num_dataPkts_received() {return  num_dataPkts_received_;}
};

#endif
