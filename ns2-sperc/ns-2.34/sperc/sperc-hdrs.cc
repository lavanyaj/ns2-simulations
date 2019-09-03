#include <stdlib.h>
#include <string>
#include <sstream>

#include "config.h"
#include "agent.h"
#include "random.h"
#include "sperc-hdrs.h"

const char* SPERC_RATE_LIMITING_STR[3] = {
  "SPERC_RATE_LIMITED",
  "INIT_RATE_LIMITED", "INIT_RATE_LIMITED_UNTIL_FIRST_SPERC_RATE"
};
const char* SPERC_HOST_STATE_STR[8] = {
  "SPERC_INACT",
  "SPERC_SYNSENT", 
  "SPERC_SYNACK_RECVD",
  "SPERC_RUNNING",
  "SPERC_FINSENT",
  "SPERC_RETRANSMIT"};

const char* SPERCLocalLabelStr[5] = 
  {"UNDEFINED", "NEW", "SAT", "UNSAT", "FIN"};

const char* SPERCIgnoreBitStr[2] = 
  {"FALSE", "TRUE"};

const char* SPERC_PKT_T_STR[13] = 
  {"SPERC_OTHER",
   "SPERC_SYN",
   "SPERC_SYNACK",
   "SPERC_DATA",
   "SPERC_ACK",
   "SPERC_FIN",
   "SPERC_FINACK",
   "SPERC_CTRL",
   "SPERC_CTRL_REV",
   "SPERC_CTRL_SYN",
   "SPERC_CTRL_SYN_REV"};

int hdr_sperc::offset_;
int hdr_sperc_ctrl::offset_;
// lavanya: added static declaration, copying from XCP, this is a mapping class that maps C++ hdr_sperc to Tcl PacketHeader/SPERC
// more instructions here- http://www.ece.ubc.ca/~teerawat/publications/NS2/15-Packets.pdf, slide 95 onwards
static class SPERCHeaderClass : public PacketHeaderClass {
public: 
	SPERCHeaderClass() : PacketHeaderClass("PacketHeader/SPERC", sizeof(hdr_sperc)) {
		bind_offset(&hdr_sperc::offset_);
	}
} class_sperchdr;

static class SPERCCtrlHeaderClass : public PacketHeaderClass {
public: 
	SPERCCtrlHeaderClass() : PacketHeaderClass("PacketHeader/SPERC_CTRL", sizeof(hdr_sperc_ctrl)) {
		bind_offset(&hdr_sperc_ctrl::offset_);
	}
} class_spercctrlhdr;

std::string hdr_sperc_ctrl::get_string(const Packet* p) {
  hdr_cmn* cmn = hdr_cmn::access(p);
  hdr_ip* ip = hdr_ip::access(p);
  hdr_sperc* sh = hdr_sperc::access(p);
  hdr_sperc_ctrl* sch = hdr_sperc_ctrl::access(p);
  std::stringstream str;
  str << "flowid: " << ip->flowid()
      << ", uid: " << cmn->uid()
      << ", ptype: " << cmn->ptype();
  if (cmn->ptype() == PT_SPERC or cmn->ptype() == PT_SPERC_CTRL) {
    str << ", SPERC_rate: " << sh->SPERC_rate_
	<< ", type: " << SPERC_PKT_T_STR[sh->SPERC_pkt_type()]
	<< ", rtt: " << sh->rtt_
	<< ", ts: " << sh->ts_
	<< ", num_dataPkts_received_: " << sh->num_dataPkts_received()
	<< ", numpkts_: " << sh->numpkts()
        << ", is_fwd: " << sh->is_fwd()
	<< ", seqno: " << sh->seqno()     
	<< ", fwd_hop: " << sh->fwd_hop()
	<< ", rev_hop: " << sh->rev_hop()
	<< ", fwd_path: "
	<< sh->fwd_path_[0] << "," 
	<< sh->fwd_path_[1] << "," 
	<<  sh->fwd_path_[2] << "," 
	<<  sh->fwd_path_[3] << "," 
	<< sh->fwd_path_[4] << "," 
	<< sh->fwd_path_[5] << "," 
	<< sh->fwd_path_[6]
	<< ", rev_path: " << sh->rev_path_[0] << "," << sh->rev_path_[1] 
	<< "," <<  sh->rev_path_[2] << "," <<  sh->rev_path_[3] 
	<< "," << sh->rev_path_[4] << "," << sh->rev_path_[5] << "," << sh->rev_path_[6];
    if (cmn->ptype() == PT_SPERC_CTRL) {
      str << ", weight: " << ((int) sch->weight())
	  << ", allocations: "
	  << ((bubble) sch->allocations_[0])
	  << "," << ((bubble) sch->allocations_[1]) 
	  << "," <<  ((bubble) sch->allocations_[2]) 
	  << "," <<  ((bubble) sch->allocations_[3]) 
	  << "," << ((bubble) sch->allocations_[4])
	  << ", bottleneck_rates: "
	  << ((bubble) sch->bottleneck_rates_[0]) 
	  << "," << ((bubble) sch->bottleneck_rates_[1]) 
	  << "," <<  ((bubble) sch->bottleneck_rates_[2])
	  << "," <<  ((bubble) sch->bottleneck_rates_[3])
	  << "," << ((bubble) sch->bottleneck_rates_[4]) 
	  << ", ignore_bits: "
	  << (SPERCIgnoreBitStr[sch->ignore_bits_[0]])
	  << "," << (SPERCIgnoreBitStr[sch->ignore_bits_[1]])
	  << "," << (SPERCIgnoreBitStr[sch->ignore_bits_[2]])
	  << "," << (SPERCIgnoreBitStr[sch->ignore_bits_[3]])
	  << "," << (SPERCIgnoreBitStr[sch->ignore_bits_[4]])
	  << ", local_labels: "
	  << (SPERCLocalLabelStr[sch->local_labels_[0]]) 
	  << "," << (SPERCLocalLabelStr[sch->local_labels_[1]]) 
	  << "," <<  (SPERCLocalLabelStr[sch->local_labels_[2]]) 
	  << "," <<  (SPERCLocalLabelStr[sch->local_labels_[3]]) 
	  << "," << (SPERCLocalLabelStr[sch->local_labels_[4]])
	  << ", stretch: "
	  << ((bubble) sch->stretch_[0]) 
	  << "," << ((bubble) sch->stretch_[1]) 
	  << "," <<  ((bubble) sch->stretch_[2]) 
	  << "," <<  ((bubble) sch->stretch_[3]) 
	  << "," << ((bubble) sch->stretch_[4])
	  << ", is_exit: "
	  << ((int) sch->is_exit());
    }

  }
  return str.str();
}

// std::string hdr_sperc_ctrl::get_string(const Packet* p) {
//   hdr_cmn* cmn = hdr_cmn::access(p);
//   hdr_ip* ip = hdr_ip::access(p);
//   hdr_sperc* sh = hdr_sperc::access(p);
//   hdr_sperc_ctrl* sch = hdr_sperc_ctrl::access(p);
//   std::stringstream str;
//   str << "flowid: " << ip->flowid()
//       << ", uid: " << cmn->uid()
//       << ", ptype: " << cmn->ptype();
//   if (cmn->ptype() == PT_SPERC or cmn->ptype() == PT_SPERC_CTRL) {
//     str << ", SPERC_rate: " << sh->SPERC_rate_
// 	<< ", type: " << SPERC_PKT_T_STR[sh->SPERC_pkt_type()]
// 	<< ", rtt: " << sh->rtt_
// 	<< ", ts: " << sh->ts_
// 	<< ", num_dataPkts_received_: " << sh->num_dataPkts_received()
// 	<< ", numpkts_: " << sh->numpkts()
//         << ", is_fwd: " << sh->is_fwd()
// 	<< ", seqno: " << sh->seqno()     
// 	<< ", fwd_hop: " << sh->fwd_hop()
// 	<< ", rev_hop: " << sh->rev_hop()
// 	<< ", fwd_path: "
// 	<< sh->fwd_path_[0] << "," 
// 	<< sh->fwd_path_[1] << "," 
// 	<<  sh->fwd_path_[2] << "," 
// 	<<  sh->fwd_path_[3] << "," 
// 	<< sh->fwd_path_[4] << "," 
// 	<< sh->fwd_path_[5] << "," 
// 	<< sh->fwd_path_[6]
// 	<< ", rev_path: " << sh->rev_path_[0] << "," << sh->rev_path_[1] 
// 	<< "," <<  sh->rev_path_[2] << "," <<  sh->rev_path_[3] 
// 	<< "," << sh->rev_path_[4] << "," << sh->rev_path_[5] << "," << sh->rev_path_[6];
//     if (cmn->ptype() == PT_SPERC_CTRL) {
//       str << ", current_requested_bandwidth: " << ((bubble) sch->current_requested_bandwidth())
// 	  << ", current_bottleneck: " << ((int) sch->current_bottleneck())
// 	  << ", weight: " << ((int) sch->weight())
// 	  << ", allocations: "
// 	  << ((bubble) sch->allocations_[0]) << "," << ((bubble) sch->allocations_[1]) << "," <<  ((bubble) sch->allocations_[2]) << "," <<  ((bubble) sch->allocations_[3]) << "," << ((bubble) sch->allocations_[4])
// 	  << ", local_labels: "
// 	  << (SPERCLocalLabelStr[sch->local_labels_[0]]) 
// 	  << "," 
// 	  << (SPERCLocalLabelStr[sch->local_labels_[1]]) 
// 	  << "," 
// 	  <<  (SPERCLocalLabelStr[sch->local_labels_[2]]) 
// 	  << "," 
// 	  <<  (SPERCLocalLabelStr[sch->local_labels_[3]]) 
// 	  << "," 
// 	  << (SPERCLocalLabelStr[sch->local_labels_[4]])
// 	  << ", stretch: "
// 	  << ((bubble) sch->stretch_[0]) << "," << ((bubble) sch->stretch_[1]) << "," <<  ((bubble) sch->stretch_[2]) << "," <<  ((bubble) sch->stretch_[3]) << "," << ((bubble) sch->stretch_[4])
// 	  << ", is_exit: "
// 	  << ((int) sch->is_exit());
//     }

//   }
//   return str.str();
// }
