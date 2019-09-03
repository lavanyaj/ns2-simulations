/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the MASH Research
 * 	Group at the University of California Berkeley.
 * 4. Neither the name of the University nor of the Research Group may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* rcp-host5.cc
 * -------
 * rtp1.cc: rate is fixed at the beginning and does not change during flow duration.
 * rtp2.cc: rate is reset every round trip time. (Too much probing)
 * rtp3.cc: rate is reset every (RTT + SYN_DELAY). To disable subsequent SYNs, set SYN_DELAY larger than all flow durations.
 * rtp4.cc: SYN(ACK) packets are of different size from data packets. Rate is reset every (RTT + SYN_DELAY).
 * rtp5.cc: Rate is reset every SYN_DELAY time.
 */


//#define DELETE_AGENTS  //xxxxrui

// #define MASAYOSHI_DEBUG 1

#include <stdlib.h>

#include "config.h"
#include "agent.h"
#include "random.h"
#include "rcp-host.h"
#include "ip.h" /* Nandita Rui */

//#define SYN_DELAY 0.05
// #define SYN_DELAY 0.5
// #define REF_FACT 4  //Number of REFs per RTT
// #define RCP_HDR_BYTES 40  //same as TCP for fair comparison
// #define QUIT_PROB 0.1  //probability of quitting if assigned rate is zero
// #define REF_INTVAL 1.0 //

const char* RCP_PKT_T_STR[9] = 
	{"RCP_OTHER", 
		"RCP_SYN", 
		"RCP_SYNACK", 
		"RCP_DATA",
	        "RCP_REF",
	 "RCP_REFACK",
		"RCP_ACK",
		"RCP_FIN",
		"RCP_FINACK"};

const char* RCP_HOST_STATE_STR[7] = 
	{"RCP_INACT",
	 "RCP_SYNSENT",
	 "RCP_RUNNING",
	 "RCP_FINSENT",
	 "RCP_RETRANSMIT"};

int hdr_rcp::offset_;
// lavanya: added static declaration, copying from XCP, this is a mapping class that maps C++ hdr_rcp to Tcl PacketHeader/RCP
// more instructions here- http://www.ece.ubc.ca/~teerawat/publications/NS2/15-Packets.pdf, slide 95 onwards
static class RCPHeaderClass : public PacketHeaderClass {
public: 
	RCPHeaderClass() : PacketHeaderClass("PacketHeader/RCP", sizeof(hdr_rcp)) {
		bind_offset(&hdr_rcp::offset_);
	}
} class_rcphdr;

static class RCPAgentClass : public TclClass {
public:
	RCPAgentClass() : TclClass("Agent/RCP") {}
	TclObject* create(int, const char*const*) {
		return (new RCPAgent());
	}
} class_rcp_agent;

RCPAgent::RCPAgent() : 
	Agent(PT_RCP), nodeid_(-1), lastpkttime_(-1e6),	
	rtt_(-1),
	min_rtt_(-1), 
	rto_abs_(-1), rto_fact_(-1),
	seqno_(0),
	ref_seqno_(0),
	extra_probe_seqno_(0),
	interval_(-1e6),
	num_sent_(0),
	RCP_state(RCP_INACT),
	 numOutRefs_(0),
	num_dataPkts_acked_by_receiver_(0), 
	num_dataPkts_received_(0), 
	num_dataPkts_sent_(0),
	num_Pkts_to_retransmit_(0), 
	num_pkts_resent_(0),
	num_enter_retransmit_mode_(0),
	num_refPkts_received_(0),
	num_refPkts_sent_(0),
	rcp_timer_(this, &RCPAgent::timeout),
	ref_timer_(this, &RCPAgent::ref_timeout),
	rto_timer_(this, &RCPAgent::retrans_timeout),
	rate_probe_timer_(this, &RCPAgent::rate_probe_timeout),
	last_rate_probe_time_(-1),
	channel_(NULL),
	first_log_recv_ack_(true), 
	first_log_rate_change_(true)
{
	bind("rto_abs_", &rto_abs_); // rto is exactly this if > 0
	bind("rto_fact_", &rto_fact_); // else rto is this times rtt_

	bind("rate_probe_interval_", 
	     &rate_probe_interval_);

	bind("seqno_", &seqno_);
	bind("packetSize_", &size_); // include header also
	/* numpkts_ has file size, need not be an integer */
	bind("numpkts_",&numpkts_);
	bind("nodeid_", &nodeid_);
	//	bind("init_refintv_fix_",&init_refintv_fix_);
	bind("fid_",&fid_);
	bind("syn_delay_", &SYN_DELAY_);
	bind("rcp_hdr_bytes_", &RCP_HDR_BYTES_);
	//	bind("quit_prob_", &QUIT_PROB_);
	bind("probe_pkt_bytes_", &PROBE_PKT_BYTES_);
	min_rtt_ = -1;


	if (rate_probe_interval_ == 0) {
		fprintf(stderr,
			"set rate_probe_interval_ explicitly in tcl! set to -1 to not use REF\n");
		exit(1);
	}

	if (RCP_HDR_BYTES_ == 0 || size_ <= RCP_HDR_BYTES_) {
		fprintf(stderr, 
			"requested size %d too small for headers %f, or invalid header size", 
			size_, RCP_HDR_BYTES_);
		exit(1);
	}
}


double RCPAgent::get_rto() {
	if (rto_abs_ > 0) return rto_abs_;
	else return rto_fact_ * rtt_;
        //return 0.1;
        //return 2 * rtt_;
        //return 0.1;
}

void RCPAgent::cancel_all_timers() {
	rcp_timer_.force_cancel();
	ref_timer_.force_cancel();
	/* For retransmissions */ 
	rto_timer_.force_cancel();
	rate_probe_timer_.force_cancel();

}

void RCPAgent::reset() /* reset() is added by Masayoshi */
{
	//	*(int*)0 = 0;	

	cancel_all_timers();
	//	Tcl::instance().evalf("%s reset-tcl", this->name()); 
	fprintf(stdout, "%lf %s fid_ %d reset\n", Scheduler::instance().clock(), 
		this->name(), fid_);

	// A source should be RCP_INACT on reset but receiver could be running.
	// Maybe use numpkts_ to check? Either RCP_state is INACT or numpkts_ = 0.
	if (RCP_state != RCP_INACT and numpkts_ > 0) {
		// It's possible that old flow got lost, in that case make this a warning
		fprintf(stderr, "%lf %s fid_ %d: RCPAgent::reset() called when in state %s numpkts_ %u\n",
			Scheduler::instance().clock(), 
			this->name(), fid_,
			RCP_HOST_STATE_STR[RCP_state], numpkts_);
		exit(1);
	}

	// reset all variables that are not bound to tcl variables
	// to whatever is in constructor
	// int nodeid_; // bound
	lastpkttime_ = -1e6;
	rtt_ = -1;
	min_rtt_ = -1;
	// rto_abs_ rto_fact_ are tcl bound
	seqno_ = 0;
	ref_seqno_ = 0; /* Masayoshi */
	extra_probe_seqno_ = 0;
	//	init_refintv_fix_; /* Masayoshi */ // not used
	interval_ = -1e6;
	// numpkts_; bound
	num_sent_ = 0;
	RCP_state = RCP_INACT;
	numOutRefs_ = 0;

	/* for retransmissions */ 
	num_dataPkts_acked_by_receiver_ = 0;   // number of packets acked by receiver 
	num_dataPkts_received_ = 0;            // Receiver keeps track of number of packets it received
	num_dataPkts_sent_ = 0;
	num_Pkts_to_retransmit_ = 0;           // Number of data packets to retransmit
	num_pkts_resent_ = 0;                  // Number retransmitted since last RTO 
	num_enter_retransmit_mode_ = 0;        // Number of times we are entering retransmission mode 
	num_refPkts_received_ = 0;
	num_refPkts_sent_ = 0;

	last_rate_probe_time_ = -1;
	// rate_probe_interval_ bound
	// Tcl_Channel channel_; // bound
	first_log_recv_ack_ = true; // True until the first log_ack()
	first_log_rate_change_ = true; // True until the first log_rate_change()
}

double RCPAgent::now() {
    return  Scheduler::instance().clock();
}
void RCPAgent::start()
{

	RCP_state = RCP_RUNNING;
	Tcl::instance().evalf("%s begin-datasend", this->name()); /* Added by Masayoshi */

	// rcp_timer_.resched(interval_); // Masayosi. 
	// // Always start sending after SYN-ACK receive 
	// // is not good when gamma is very very small.

	RCP_state = RCP_RUNNING;
	ref_timer_.force_cancel();

	// timeout should be later than the above RCP_state change.
	// This is because if numpkts_ is one (1 pkt file transfer),
	// the above state change may harmfully overwrite
	// the change to RCP_FINSENT in timeout().
	timeout();  
}

void RCPAgent::stop()
{
	cancel_all_timers();
	finish();
}

void RCPAgent::pause()
{
	cancel_all_timers();
	RCP_state = RCP_INACT;
}

/* Nandita: RCP_desired_rate sets the senders initial desired rate
 */
double RCPAgent::RCP_desired_rate()
{
	double RCP_rate_ = -1; 
	return(RCP_rate_);
}

/* Nandita: sendfile() 
 * Sends a SYN packet and enters RCPS_LISTEN state
 * On receiving the SYN_ACK packet (with the rate) from
 * RCP receiver, the recv() function calls start() and
 * packets are sent at the desired rate
 */
void RCPAgent::sendfile()
{
	// Sending SYN packet: 
	Packet* p = allocpkt();
	hdr_rcp *rh = hdr_rcp::access(p);
	
	// SYN and SYNACK packets should be smaller than data packets.
	hdr_cmn *cmnh = hdr_cmn::access(p);

	cmnh->size_ = RCP_HDR_BYTES_;
	
	populate_route_new(p, NULL); // lavanya: path debug
	rh->set_numpkts(numpkts_); // lavanya: to track active flows

	rh->seqno() = ref_seqno_++; /* Masayoshi */
	rh->ts_ = Scheduler::instance().clock();
	rh->rtt_ = rtt_;
       
	rh->set_RCP_pkt_type(RCP_SYN);
	rh->set_RCP_rate(RCP_desired_rate());

	rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
	rh->flowIden() = fid_;

	fprintf(stdout,"RCPAgent sendfile(): %lf %s nodeid_ %d flow fid_ %d num_dataPkts_received_ %u numpkts_ %u state %s\n", 
		Scheduler::instance().clock(), this->name(), nodeid_, fid_, num_dataPkts_received_, numpkts_, RCP_HOST_STATE_STR[RCP_state]);
	last_rate_probe_time_ = now();
 	target_->recv(p, (Handler*)0);

	// Change state of sender: Sender is in listening state 
	RCP_state = RCP_SYNSENT;

        double rto =  SYN_DELAY_; // get_rto();
        if (rto <= 0) {
   	    fprintf(stderr, "RCPAgent sendfile(): %lf %s nodeid_ %d flow fid_ %d rto is 0. num_dataPkts_received_ %u numpkts_ %u state %s\n", 
		    now(), this->name(), nodeid_, fid_, 
                                    num_dataPkts_received_, numpkts_, RCP_HOST_STATE_STR[RCP_state]);
                                exit(1); 
	}

	ref_timer_.resched(rto);
	lastpkttime_ =  now();
	last_rate_probe_time_ = now();

	if (rate_probe_interval_ > 0) {
		// note this comes after we set 
		// last rate probe time above
		// also check that it's not already 
		// running?
		rate_probe_timer_.resched(rate_probe_interval_);
	}

	Tcl::instance().evalf("%s syn-sent", this->name()); /* Added by Masayoshi */
}


/* Nandita Rui
 * This function has been changed.
 */
void RCPAgent::timeout() 
{
	if (RCP_state == RCP_RUNNING) {
		if (num_sent_ < numpkts_ - 1) {
			sendpkt();
			rcp_timer_.resched(interval_);

		} else {			
			sendlast();
			Tcl::instance().evalf("%s finish-datasend", this->name()); /* Added by Masayoshi */
			RCP_state = RCP_FINSENT;
			rcp_timer_.force_cancel();
			ref_timer_.force_cancel();
			//lavanya: TODO? currently retransmissions start 2*rtt_ after last packet
			// ideally, retransmission timer should start after each packet
			// and if packet hasn't been acked at expiration, it should be added
			// to retransmission queue, and sent at the current RCP rate.
                        double rto = get_rto();
                        if (rto <= 0) {
   	                    fprintf(stderr, "RCPAgent timeout(): %lf %s nodeid_ %d flow fid_ %d rto is <=0. num_dataPkts_received_ %d numpkts_ %u state %s\n", 
				    now(), this->name(), nodeid_, fid_, 
                                    num_dataPkts_received_, numpkts_, RCP_HOST_STATE_STR[RCP_state]);
                                exit(1); 
			}
			rto_timer_.resched(rto); // 2 * rtt_
		}

	} else if (RCP_state == RCP_RETRANSMIT) {
		if (num_pkts_resent_ < num_Pkts_to_retransmit_ - 1) {
			sendpkt();
			rcp_timer_.resched(interval_);
		} else if (num_pkts_resent_ == num_Pkts_to_retransmit_ - 1) {
			sendpkt();
			rcp_timer_.force_cancel();
			ref_timer_.force_cancel();
			// lavanya: TODO? currently state machine
			// is RCP_RUNNING -> RCP_RETRANSMIT (entered once) -> RCP_RETRANSMIT (Entered twice) etc.
			// where condition for entering next state is if 2 rtts after last packet was sent in
			// current state, the numPackets acked doesn't equal num packets that were sent
			// (or retransmitted) in the current state. ideally want RCP_RUNNING interleaved with
			// RCP_RETRANSMIT with as few idle times (such as when we wait 2 rtts before checking)
			// as possible
                        double rto = get_rto();
                        if (rto <= 0) {
   	                    fprintf(stderr,"RCPAgent timeout(): %lf %s nodeid_ %d flow fid_ %d rto is <=0. num_dataPkts_received_ %u numpkts_ %u state %s\n", 
				    now(), this->name(), nodeid_, fid_, 
                                    num_dataPkts_received_, numpkts_, RCP_HOST_STATE_STR[RCP_state]);
                                exit(1); 
			}

			rto_timer_.resched(rto);
		}
	}
}

void RCPAgent::retrans_timeout()
{ 
	RCP_state = RCP_RETRANSMIT;
	num_enter_retransmit_mode_++; 
    
	num_pkts_resent_ = 0;
	num_Pkts_to_retransmit_ = numpkts_ - num_dataPkts_acked_by_receiver_;
 	fprintf(stderr, "warning! %lf %s fid %d entered retransmit mode %d, num_Pkts_to_retransmit_ %u numpkts %u num_data_pkts_sent_ %u\n", Scheduler::instance().clock(), this->name(), fid_, num_enter_retransmit_mode_, num_Pkts_to_retransmit_, numpkts_, num_dataPkts_sent_);

	if (num_Pkts_to_retransmit_ > 0)
		rcp_timer_.resched(interval_);
}


void RCPAgent::rate_probe_timeout() 
{
	rate_probe_timer_.force_cancel();
	if (RCP_state == RCP_INACT) {
		return;
	// don't reschedule except in sendfile()
	// when next flow starts
	}
	if (last_rate_probe_time_ < 0) {
		fprintf(stderr, "no probe sent\n");
		exit(1);
	}

	if (now() - last_rate_probe_time_ > rate_probe_interval_) {
		// time to send an extra probe
		// we don't care about outstanding
		// extra probes?
		Packet* send_p = allocpkt();
		hdr_rcp *rh = hdr_rcp::access(send_p);				
		hdr_cmn *cmnh = hdr_cmn::access(send_p);

		cmnh->size_ = PROBE_PKT_BYTES_;
		populate_route_new(send_p, NULL); // lavanya: path debug
		rh->set_numpkts(numpkts_); // lavanya: to track active flows
		rh->seqno() = extra_probe_seqno_++;
		rh->ts_ = now();
		rh->rtt_ = -1;
		// rh->rtt_ = rtt_; 
		// don't want switches to react t
		// to these extra probe packets
		rh->set_RCP_pkt_type(RCP_REF);
		rh->set_RCP_rate(RCP_desired_rate());
		// rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
		rh->flowIden() = fid_;		
		last_rate_probe_time_ = now();
		numOutRefs_++;
		num_refPkts_sent_++;
		target_->recv(send_p, (Handler*)0);
	}
	rate_probe_timer_.resched(rate_probe_interval_);	    		
}

/* Rui
 * This function 
 */
void RCPAgent::ref_timeout() 
{
        ref_timer_.force_cancel();
	if (RCP_state!=RCP_SYNSENT) {
		fprintf(stderr, "%lf %s fid_ %d: ref_timeout() (to retransmit SYN) but RCP_state %s",
			now(), this->name(), fid_, RCP_HOST_STATE_STR[RCP_state]);
                exit(1);
	}
	Packet* send_p = allocpkt();
	hdr_rcp *rh = hdr_rcp::access(send_p);		
	// SYN and SYNACK packets should be smaller than data packets.
	hdr_cmn *cmnh = hdr_cmn::access(send_p);
	cmnh->size_ = RCP_HDR_BYTES_;
	
	populate_route_new(send_p, NULL); // lavanya: path debug
	rh->set_numpkts(numpkts_); // lavanya: to track active flows
	
	// cmnh->size_ = 1;		
	//		rh->seqno() = ref_seqno_ ++;  /* added by Masayoshi */
	rh->seqno() = ref_seqno_;
	ref_seqno_++;  /* added by Masayoshi */
	rh->ts_ = now();
	rh->rtt_ = rtt_;
	rh->set_RCP_pkt_type(RCP_SYN);
	rh->set_RCP_rate(RCP_desired_rate());
	rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
	rh->flowIden() = fid_;		
	last_rate_probe_time_ = now();
	target_->recv(send_p, (Handler*)0);        	

	// try re-transmitting SYN only up to 3 times
	double rto = SYN_DELAY_;
	// send SYN now ref_seqno_ is 1
	// send SYN again now ref_seqno_ is 2
	// send SYN third time now ref_seqno_ is 3
	if (ref_seqno_ < 10) {
		fprintf(stderr, "RCPAgent ref_timeout(): to retransmit SYN  %lf %s nodeid_ %d flow fid_ %d  num_dataPkts_received_ %d num_dataPkts_sent_ %d num_dataPkts_acked_by_receiver_ %d numpkts_ %u state %s ref_seqno_ %d rto=SYN_DELAY_ %f\n", 
			now(), this->name(), nodeid_, fid_, 
			num_dataPkts_received_, num_dataPkts_sent_, num_dataPkts_acked_by_receiver_,
			numpkts_, RCP_HOST_STATE_STR[RCP_state],
			ref_seqno_, rto);

		ref_timer_.resched(rto);
	} else {

		fprintf(stderr, "RCPAgent ref_timeout(): to retransmit SYN, but we've re-tranmitted %d times already. Giving up now. %lf %s nodeid_ %d flow fid_ %d  num_dataPkts_received_ %d num_dataPkts_sent_ %d num_dataPkts_acked_by_receiver_ %d numpkts_ %u state %s ref_seqno_ %d rto=SYN_DELAY_ %f\n", 
		ref_seqno_, now(), this->name(), nodeid_, fid_, 
		num_dataPkts_received_,   num_dataPkts_sent_, num_dataPkts_acked_by_receiver_,
			numpkts_, RCP_HOST_STATE_STR[RCP_state],
		ref_seqno_, rto);

		fprintf(stderr, "RCPAgent ref_timeout(): actually syn retx %lf %s nodeid_ %d flow fid_ %d rto is set to SYN_DELAY. num_dataPkts_received_ %d numpkts_ %u state %s ref_seqno_ %d rto/ SYN_DELAY_ %f\n", 
		now(), this->name(), nodeid_, fid_, 
		num_dataPkts_received_, numpkts_, RCP_HOST_STATE_STR[RCP_state],
		ref_seqno_, rto);

	}

}

/*
 * finish() is called when we must stop (either by request or because
 * we're out of packets to send.
 */
void RCPAgent::finish()
{



	// maybe log num_dataPkts_sent_, num_refPkts_sent_, numPkts_
	// or even bytes?
	RCP_state = RCP_INACT;

        fprintf(stderr, "%s fid %d done_stats %u %u %u %u %u\n", this->name(), fid_,
			      numpkts_, num_dataPkts_sent_, num_refPkts_sent_,
		num_enter_retransmit_mode_, num_dataPkts_acked_by_receiver_);

	Tcl::instance().evalf("%s done_stats %u %u %u %u %u", this->name(),
			      numpkts_, num_dataPkts_sent_, num_refPkts_sent_,
			      num_enter_retransmit_mode_, num_dataPkts_acked_by_receiver_); /* Added by Masayoshi */
	// will call agent_aggr_pair fin_notify, which will reset source and receiver
	Tcl::instance().evalf("%s done", this->name()
);

	fid_      = 0; 

#ifdef DELETE_AGENTS
	Tcl::instance().evalf("delete %s", this->name());
#endif
}


/* Nandita Rui
 * This function has been changed.
 */
void RCPAgent::recv(Packet* p, Handler*)
{
	hdr_rcp* rh = hdr_rcp::access(p);

	if ( (rh->RCP_pkt_type() == RCP_SYN) || ((RCP_state != RCP_INACT) && (rh->flowIden() == fid_)) ) {
		//    if ((rh->RCP_pkt_type() == RCP_SYN) || (rh->flowIden() == fid_)) {
		populate_route_recvd(p); // lavanya: for debugging path

		switch (rh->RCP_pkt_type()) {
		case RCP_REF:
			{
				double copy_rate;
				num_refPkts_received_++;
				Packet *send_pkt = allocpkt();
				hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
				hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
				cmnh->size_ = RCP_HDR_BYTES_;	      
				populate_route_new(send_pkt, p); // lavanya: path debug
				copy_rate = rh->RCP_request_rate();
				// Can modify the rate here.
				send_rh->seqno() = rh->seqno();
				send_rh->ts() = rh->ts();
				// send_rh->rtt() = rh->rtt();
				send_rh->rtt() = -1; // no RTT from ACKs
				send_rh->set_RCP_pkt_type(RCP_REFACK);
				send_rh->set_RCP_rate(copy_rate);
				send_rh->set_RCP_numPkts_rcvd(num_refPkts_received_);
				send_rh->flowIden() = rh->flowIden();		
				target_->recv(send_pkt, (Handler*)0);
				break;
		}

		case RCP_REFACK: 
		{
			log_ack(p);
			numOutRefs_--;
			if (numOutRefs_ != 0) {
				fprintf(stderr, 
					"RCPAgent recv():  %lf %s nodeid_ %d flow fid_ %d rto is 0. num_dataPkts_received_ %d numpkts_ %u state %s ref_seqno_ %d numOutRefs_ %d !!\n", 
					now(), this->name(), nodeid_, fid_, 
					num_dataPkts_received_, numpkts_, RCP_HOST_STATE_STR[RCP_state],
					ref_seqno_, numOutRefs_);
				//exit(1);
			}
			// don't update RTT, just update rate for now
			if (rh->RCP_request_rate() > 0) {
				double new_interval = (size_)/(rh->RCP_request_rate());
				if( new_interval != interval_ ){ 
					interval_ = new_interval;
					rate_change();
					log_rates();
				}
			} else {
				fprintf(stderr, 
					"Error: RCP rate < 0: %f in REFACK for %d\n",
					rh->RCP_request_rate(), fid_);
                                exit(1);
			}
			break;
		}
		case RCP_SYNACK:
			log_ack(p);
			fprintf(stdout, "path of flow %d is %s\n",
				hdr_ip::access(p)->flowid(),
				get_pkt_path(p).c_str());
			rtt_ = Scheduler::instance().clock() - rh->ts();

			if (min_rtt_ == -1 or min_rtt_ > rtt_)
				min_rtt_ = rtt_;

			if (rh->RCP_request_rate() > 0) {
				interval_ = (size_)/(rh->RCP_request_rate());
				log_rates();

#ifdef MASAYOSHI_DEBUG
				fprintf(stdout,"%lf recv_synack..rate_change_1st %s %lf %lf\n",now(),this->name(),interval_,((size_)/interval_)/(150000000.0 / 8.0));
#endif
			
				if (RCP_state == RCP_SYNSENT)
					start();

			}
			else {
					fprintf(stderr, "Error: RCP rate <= 0 in SYNACK for %d: %f\n", fid_, rh->RCP_request_rate());
					exit(1);
			}
			break;

		case RCP_ACK:
			log_ack(p);
			num_dataPkts_acked_by_receiver_ = rh->num_dataPkts_received; 
			if (num_dataPkts_acked_by_receiver_ >= numpkts_) {
				// fprintf(stdout, "%lf %d RCP_ACK: Time to stop \n", Scheduler::instance().clock(), rh->flowIden());
				stop();
			}

			rtt_ = Scheduler::instance().clock() - rh->ts();
			if (min_rtt_ == -1 or min_rtt_ > rtt_)
				min_rtt_ = rtt_;

			if (rh->RCP_request_rate() > 0) {
				double new_interval = (size_)/(rh->RCP_request_rate());
				if( new_interval != interval_ ){
					interval_ = new_interval;
                                       {
						rate_change();
						log_rates();
					}
				}
			}
			else {
				fprintf(stderr, "Error: RCP rate < 0: %f in ACK for %d\n",rh->RCP_request_rate(), fid_);
                                exit(1);
			}
			break;

		case RCP_FIN:
			{double copy_rate;
				num_dataPkts_received_++; // because RCP_FIN is piggybacked on the last packet of flow
				Packet* send_pkt = allocpkt();
				hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
				hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
				cmnh->size_ = RCP_HDR_BYTES_;
				populate_route_new(send_pkt, p); // lavanya: path debug
				send_rh->set_numpkts(rh->numpkts()); // lavanya: to track active flows

				copy_rate = rh->RCP_request_rate();
				// Can modify the rate here.
				send_rh->seqno() = rh->seqno();
				send_rh->ts() = rh->ts();
				//send_rh->rtt() = rh->rtt();
				send_rh->rtt() = -1; // no RTT from ACKs
				send_rh->set_RCP_pkt_type(RCP_FINACK);
				send_rh->set_RCP_rate(copy_rate);
				send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
				send_rh->flowIden() = rh->flowIden();
						       
				target_->recv(send_pkt, (Handler*)0);
				Tcl::instance().evalf("%s fin-received", this->name()); /* Added by Masayoshi */
				break;
			}


		case RCP_SYN:
			{double copy_rate;
		
				Packet* send_pkt = allocpkt();
				hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
				hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
				cmnh->size_ = RCP_HDR_BYTES_;
		
				populate_route_new(send_pkt, p); // lavanya: path debug
				send_rh->set_numpkts(rh->numpkts()); // lavanya: to track active flows

				copy_rate = rh->RCP_request_rate();
				// Can modify the rate here.
				send_rh->seqno() = rh->seqno();
				send_rh->ts() = rh->ts();
				//send_rh->rtt() = rh->rtt();
				send_rh->rtt() = -1; // no RTT from ACKs
				send_rh->set_RCP_pkt_type(RCP_SYNACK);
				send_rh->set_RCP_rate(copy_rate);
				send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
				send_rh->flowIden() = rh->flowIden();
			
				target_->recv(send_pkt, (Handler*)0);
				RCP_state = RCP_RUNNING; // Only the receiver changes state here

				break;}

		case RCP_FINACK:
			log_ack(p);
			num_dataPkts_acked_by_receiver_ = rh->num_dataPkts_received;

			if (num_dataPkts_acked_by_receiver_ == numpkts_){
				// fprintf(stdout, "%lf %d RCP_FINACK: Time to stop \n", Scheduler::instance().clock(), rh->flowIden());
				stop();
			}
			break;

		case RCP_DATA:
			{
				double copy_rate;
				num_dataPkts_received_++;
			
				Packet* send_pkt = allocpkt();
				hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
				hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
				cmnh->size_ = RCP_HDR_BYTES_;
		
				populate_route_new(send_pkt, p); // lavanya: path debug
				send_rh->set_numpkts(rh->numpkts()); // lavanya: to track active flows

				copy_rate = rh->RCP_request_rate();
				// Can modify the rate here.
				send_rh->seqno() = rh->seqno();
				send_rh->ts() = rh->ts();
				// send_rh->rtt() = rh->rtt();
				send_rh->rtt() = -1; // no RTT from ACKs
				send_rh->set_RCP_pkt_type(RCP_ACK);
				send_rh->set_RCP_rate(copy_rate);
				send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
				send_rh->flowIden() = rh->flowIden();
		
				target_->recv(send_pkt, (Handler*)0);
				break;}

		case RCP_OTHER:
			fprintf(stderr, "received RCP_OTHER\n");
			exit(1);
			break;

		default:
			fprintf(stderr, "Unknown RCP packet type!\n");
			exit(1);
			break;
		}
	}

	Packet::free(p);
}

void RCPAgent::log_rates() {
  if(channel_ != NULL){
    if (first_log_rate_change_) {
      char header[8192];
      sprintf(header, "RATE_CHANGE fid time(s) rcp_data(mb/s)\n");	    
      (void)Tcl_Write(channel_, header, strlen(header));
      first_log_rate_change_ = false;
    } 
    char buf[8192];
    // "%s : rcp_data %g us %g Mb/s, rcp_data %g us %g Mb/s, rcp_ctrl %g us = %g Mb/s\n"

    // data packet size is
    //Packet *p = allocpkt() , will set size() to size_ = pktSize (which we set in TCL)
    // so that it includes both data and all headers

    sprintf(buf, "RATE_CHANGE %d %f %g\n",
	    fid_,	       
	    ((double) Scheduler::instance().clock()),
	    (((size_*8)/(interval_))*1e-6));
    
    (void)Tcl_Write(channel_, buf, strlen(buf));

  } 

// else {
  //   fprintf(stdout, "%d : rcp_data %g us %g Mb/s\n",
  // 	    fid_, 
  // 	    (interval_ * 1e6), (((size_*8)/(interval_))*1e-6));
  // }
}

void RCPAgent::log_ack(Packet *p) {
	// don't log ACKs
	return;

  if(channel_ != NULL){
    if (first_log_recv_ack_) {
      char header[8192];
      sprintf(header, "@RECV_ACK fid time(s) ack_type is_exit rtt(us) num_dataPkts_received rate\n");	    
      (void)Tcl_Write(channel_, header, strlen(header));
      first_log_recv_ack_ = false;
    } 
    char buf[8192];
    // "%s : rcp_data %g us %g Mb/s, rcp_data %g us %g Mb/s, rcp_ctrl %g us = %g Mb/s\n"
    hdr_rcp* rh = hdr_rcp::access(p);
    double rtt =  (Scheduler::instance().clock() - rh->ts())*1e6;
    bool is_exit = (hdr_cmn::access(p)->ptype() == PT_RCP && hdr_rcp::access(p)->RCP_pkt_type() == RCP_FINACK);    
    sprintf(buf, "@RECV_ACK %d %f %s %d %f %u %f\n",
	    fid_,	       
	    ((double) Scheduler::instance().clock()),
	    RCP_PKT_T_STR[rh->RCP_pkt_type()],
	    is_exit,
	    rtt,
	    rh->num_dataPkts_received,
	    rh->RCP_request_rate());
    
    (void)Tcl_Write(channel_, buf, strlen(buf));
  } // else {
  //   hdr_rcp* rh = hdr_rcp::access(p);
  //   double rtt =  (Scheduler::instance().clock() - rh->ts())*1e6;
  //   bool is_exit = (hdr_cmn::access(p)->ptype() == PT_RCP && hdr_rcp::access(p)->RCP_pkt_type() == RCP_FINACK);    
  //   fprintf(stdout, "RECV_ACK %d %f %s %d %f %d\n",
  // 	    fid_,	       
  // 	    ((double) Scheduler::instance().clock()),
  // 	    RCP_PKT_T_STR[rh->RCP_pkt_type()],
  // 	    is_exit,
  // 	    rtt,
  // 	    rh->num_dataPkts_received);   
  // }
}

int RCPAgent::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "rate-change") == 0) {
			rate_change();
			return (TCL_OK);
		} else if (strcmp(argv[1], "start") == 0) {
			start();
			return (TCL_OK);
		} else if (strcmp(argv[1], "stop") == 0) {
			stop();
			return (TCL_OK);
		} else if (strcmp(argv[1], "pause") == 0) {
			pause();
			return (TCL_OK);
		} else if (strcmp(argv[1], "sendfile") == 0) {
			sendfile();
			return(TCL_OK);
		}		
	} else if (argc == 3) {
		if (strcmp(argv[1], "attach") == 0) {
			Tcl& tcl = Tcl::instance();
			int mode;
			const char* id = argv[2];
			channel_ = Tcl_GetChannel(tcl.interp(),
						  (char*) id, &mode);
			if (channel_ == NULL) {
				tcl.resultf("Tagger (%s): can't attach %s for writing\n",
					    name(), id);
				return (TCL_ERROR);
			}
			fprintf(stdout, "Tagger (%s): attached %s to RCP Agent for writing\n", name(), id);
			return (TCL_OK);
		}
	}
	// 	else if (argc == 3) {
	// 		// if (strcmp(argv[1], "session") == 0) {
	//  			session_ = (RCPSession*)TclObject::lookup(argv[2]);
	//  			return (TCL_OK);
	//  		} else 
	// 			if (strcmp(argv[1], "advance") == 0) {
	//                         int newseq = atoi(argv[2]);
	//                         advanceby(newseq - seqno_);
	//                         return (TCL_OK); 
	//                 } else if (strcmp(argv[1], "advanceby") == 0) {
	//                         advanceby(atoi(argv[2]));
	//                         return (TCL_OK);
	//                 }
	// 	}
	return (Agent::command(argc, argv));
}

/* 
 * We modify the rate in this way to get a faster reaction to the a rate
 * change since a rate change from a very low rate to a very fast rate may 
 * take an undesireably long time if we have to wait for timeout at the old
 * rate before we can send at the new (faster) rate.
 */
void RCPAgent::rate_change()
{
	if (RCP_state == RCP_RUNNING) {
		rcp_timer_.force_cancel();		
		double t = lastpkttime_ + interval_;		
		double _now = Scheduler::instance().clock();
		if ( t > _now) {
			rcp_timer_.resched(t - _now);
		} else {
			timeout(); // send a packet immediately and reschedule timer 
		}
	}
}

void RCPAgent::sendpkt()
{
	Packet* p = allocpkt();
	hdr_rcp *rh = hdr_rcp::access(p);
	rh->set_RCP_rate(0);
	rh->seqno() = seqno_++;

	// allocpkt() , sets size() to size_ = pktSize (which we set in TCL)
	// so that it includes both data and all headers
	hdr_cmn *cmnh = hdr_cmn::access(p);
	// cmnh->size_ += RCP_HDR_BYTES_;
	if (cmnh->size_ != size_) {
		fprintf(stderr, 
			" size of pkt from allocpkt() %d != packetSize_/size_ %d\n",
			cmnh->size_, size_);
		exit(1);
	} else if (cmnh->size_ < RCP_HDR_BYTES_) {
	  fprintf(stderr, 
		  "requested size %d of packet on wire too small for headers %g", 
		  size_, RCP_HDR_BYTES_);
	  exit(1);
	}

	populate_route_new(p, NULL); // lavanya: path debug
	rh->set_numpkts(numpkts_); // lavanya: to track active flows

	rh->ts_ = Scheduler::instance().clock();
	rh->rtt_ = rtt_;
	rh->set_RCP_pkt_type(RCP_DATA);
	rh->set_RCP_rate(RCP_desired_rate());
	rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
	rh->flowIden() = fid_;
	/////////////////////////////


	lastpkttime_ = now();
	last_rate_probe_time_ = now();
	num_dataPkts_sent_++;
	target_->recv(p, (Handler*)0);

	if (RCP_state == RCP_RETRANSMIT)
		num_pkts_resent_++;
	else
		num_sent_++;

}

void RCPAgent::sendlast()
{
	Packet* p = allocpkt();
	hdr_rcp *rh = hdr_rcp::access(p);
	rh->set_RCP_rate(0);
	rh->seqno() = seqno_++;
	rh->set_RCP_pkt_type(RCP_FIN);
	rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
	rh->flowIden() = fid_;

	hdr_cmn *cmnh = hdr_cmn::access(p);
	cmnh->size_ = size_;

	populate_route_new(p, NULL); // lavanya: path debug
	rh->set_numpkts(numpkts_); // lavanya: to track active flows

	lastpkttime_ = now();
	last_rate_probe_time_ = now();
	num_dataPkts_sent_++;
	target_->recv(p, (Handler*)0);
	num_sent_++;
}

void RCPATimer::expire(Event* /*e*/) {
	(*a_.*call_back_)();
}

// void RCPAgent::makepkt(Packet* p)
// {
// 	hdr_rcp *rh = hdr_rcp::access(p);

// 	rh->set_RCP_rate(0);
// 	/* Fill in srcid_ and seqno */
// 	rh->seqno() = seqno_++;
// }

// void RCPAgent::sendmsg(int nbytes, const char* /*flags*/)
// {
//         Packet *p;
//         int n;

//         //if (++seqno_ < maxpkts_) {
//                 if (size_)
//                         n = nbytes / size_;
//                 else
//                         printf("Error: RCPAgent size = 0\n");

//                 if (nbytes == -1) {
//                         start();
//                         return;
//                 }
//                 while (n-- > 0) {
//                         p = allocpkt();
//                         hdr_rcp* rh = hdr_rcp::access(p);
//                         rh->seqno() = seqno_;
//                         target_->recv(p);
//                 }
//                 n = nbytes % size_;
//                 if (n > 0) {
//                         p = allocpkt();
//                         hdr_rcp* rh = hdr_rcp::access(p);
//                         rh->seqno() = seqno_;
//                         target_->recv(p);
//                 }
//                 idle();
// //         } else {
// //                 finish();
// //                 // xxx: should we deschedule the timer here? */
// //         }
// }
// void RCPAgent::advanceby(int delta)
// {
//         maxpkts_ += delta;
//         if (seqno_ < maxpkts_ && !running_)
//                 start();
// }               

