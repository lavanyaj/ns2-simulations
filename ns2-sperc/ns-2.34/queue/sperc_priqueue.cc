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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
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
/* Ported from CMU/Monarch's code, nov'98 -Padma.*/

/* -*- c++ -*-
   priqueue.cc
   
   A simple priority queue with a remove packet function
   $Id: priqueue.cc,v 1.8 2009/01/15 06:23:49 tom_henderson Exp $
   */

#include <object.h>
#include <queue.h>
#include <packet.h>
#include <string>
#include "sperc_priqueue.h"
#include "sperc/sperc-hdrs.h" // to identify high prio sperc ctrl pkts
#include "my_tbf.h"


static class SPERCPriQueueClass : public TclClass {
public:
  SPERCPriQueueClass() : TclClass("Queue/SPERCPriQueue") {}
  TclObject* create(int, const char*const*) {
    return (new SPERCPriQueue);
  }
} class_SPERCPriQueue;

void SPERCPriQueueTimer::expire(Event *) { 
(*a_.*call_back_)();
}

SPERCPriQueue::SPERCPriQueue() : Queue(),
				 qhi_enq_bytes_(0),
				 qhi_deq_bytes_(0),
				 qhi_drop_bytes_(0),  
				 qlo_enq_bytes_(0),
				 qlo_deq_bytes_(0),
				 qlo_drop_bytes_(0),
				 tbf_enq_bytes_(0),
				 tbf_deq_bytes_(0),
				 tbf_drop_bytes_(0),
				 summarystats(false),
				 first_summarystats(true) {
        
	
  q_ = new PacketQueue; // for all but hi prio packets
  q_hi_ = new PacketQueue; // for hi prio packets


  pq_ = q_; // inherited from Queue

  statsTimer = make_unique<SPERCPriQueueTimer >
	  (this, &SPERCPriQueue::print_summarystats);  

  // limit_ bound to qlim_ in Queue()
  bind_bool("queue_in_bytes_", &qib_);  // boolean: q in bytes?

  bind("tbf_limBytes_", &tbf_limBytes_);
  bind("qhi_limBytes_", &qhi_limBytes_);
  bind("qlimBytes_", &qlimBytes_);

  // all limits must be specified in bytes and qib should be true
  // bind("tbf_lim_", &tbf_lim_);
  // bind("qhi_lim_", &qhi_lim_);
  // bind("qlim_", &qlim_);

  bind_bool("never_drop_hi_", &never_drop_hi_);
  bind_bw("sperc_tbf_rate_",&sperc_tbf_rate_);
  bind("sperc_tbf_bucket_",&sperc_tbf_bucket_);
  bind("sperc_tbf_qlen_",&sperc_tbf_qlen_);

  // Two separate queues, data and control. Control queue has
  // priority over data queue.

  // A token bucket filter that smoothes arriving control packets
  // to sperc_tbf_rate_ and enqueues onto control queue. It has
  // a queue to buffer non-conforming packets.

  // Packets once enqueued on either TBF or data queue are never
  // dropped and served in FIFO (order in which they're enqueued
  // on TBF or data queue)

  // When a packet in enqueued on SPERCPRiQueue
  // If it's a data packet, we enqueue it on data queue
  // if total bytes of data in data queue wouldn't exceed qlimBytes
  // otherwise we drop the data packet.
  // If it's a control packet, we enqueue it on tbf
  // if total bytes of control in tbf queue + control queue
  // wouldn't exceed tbf_limBytes_;  
  // otherwise we drop the packet.
  // If never_drop_hi_ is true we don't drop control packet

  if (qib_
	  and (qlimBytes_ <= 0 || tbf_limBytes_ <= 0 
	       || qhi_limBytes_ <= 0)) {
		  fprintf(stderr,
			  "invalid values for qib_ %d qlimBytes_ %d tbf_limBytes_ %d qhi_limBytes_ %d",
			  qib_, qlimBytes_, tbf_limBytes_, qhi_limBytes_);
		  exit(1);
  } else if (!qib_ ) {
	     // and (qlim_ <= 0 || tbf_lim_ <= 0 
	     // 	  || qhi_lim_ <= 0)) {

	  fprintf(stderr,
		  "invalid values for qib_ %d  (don't handle qib_ false for SPERCPriQueue",
		  qib_);
	  exit(1);
  }

  tbf_hi_ = new MyTBF(sperc_tbf_rate_, sperc_tbf_bucket_, 
		    sperc_tbf_qlen_);

  tbf_hi_->set_target_q(q_hi_);
  tbf_hi_->set_parent_q(this);
  // set tbf qlen, bucket, rate

}

SPERCPriQueue::~SPERCPriQueue() {
	if (q_) {
		for (Packet *p=q_->head();p!=0;p=p->next_) 
			Packet::free(p);
		delete q_;
		q_ = NULL;
	}
	if (tbf_hi_) {
		delete tbf_hi_;
		tbf_hi_ = NULL;
	}
	if (q_hi_) {
		for (Packet *p=q_hi_->head();p!=0;p=p->next_) 
			Packet::free(p);

		delete q_hi_;
		q_hi_ = NULL;
	}
	statsTimer->force_cancel();
	summarystats=false;
}

int
SPERCPriQueue::command(int argc, const char*const* argv)
{
  if (argc == 2) {
    if (strcmp(argv[1], "printstats") == 0) {
	    summarystats = true;
	    statsTimer->force_cancel();
	    statsTimer->sched(0.000100);
	    //if (false) fprintf(stdout, "%s", print_summarystats().c_str());
      return (TCL_OK);
    } else if (strcmp(argv[1], "shrink-queue") == 0) {
      fprintf(stderr, "SPERCPriqueue: shrink-queue not implemented.\n");
      return (TCL_ERROR);
    } 
  } else if (argc == 3) {
    if (!strcmp(argv[1], "packetqueue-attach")) {
      fprintf(stderr, "SPERCPriqueue: packetqueue-attach not implemented.\n");
      return (TCL_ERROR);
    }
  }
  
  return Queue::command(argc, argv); // handles reset too
}

// following functions added by lavanya for SPERC priority queueing for control packets
bool SPERCPriQueue::isHighPriority(Packet *p) {
	struct hdr_cmn *ch = HDR_CMN(p);
	return (ch->ptype() == PT_SPERC_CTRL);
}

void SPERCPriQueue::print_summarystats() {
  double now = Scheduler::instance().clock();
  int qhi_pkts = q_hi_->length();
  int q_pkts = q_->length();
  int qhi_bytes = q_hi_->byteLength();
  int q_bytes = q_->byteLength();
  int tbf_pkts = tbf_hi_->length();
  int tbf_bytes = tbf_hi_->byteLength();

  if (first_summarystats) {
	  fprintf(stdout, "SPERCPriQueue %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n", 
		  "now", "fromnodeid_", "tonodeid_",
		  "qhi_pkts", "qhi_bytes",
		  "q_pkts", "q_bytes",
		  "tbf_pkts", "tbf_bytes",
		  "qhi_enq_bytes_", "qhi_deq_bytes_", "qhi_drop_bytes_",
		  "qlo_enq_bytes_", "qlo_deq_bytes_", "qlo_drop_bytes_",
		  "tbf_enq_bytes_", "tbf_deq_bytes_", "tbf_drop_bytes_");	  
	  first_summarystats = false;
  }
  
  //char buf[4098];
  fprintf(stdout, "SPERCPriQueue %f %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", 
	  now, fromnodeid_, tonodeid_,
	  qhi_pkts, qhi_bytes,
	  q_pkts, q_bytes,
	  tbf_pkts, tbf_bytes,
	  qhi_enq_bytes_, qhi_deq_bytes_, qhi_drop_bytes_,
	  qlo_enq_bytes_, qlo_deq_bytes_, qlo_drop_bytes_,
          tbf_enq_bytes_, tbf_deq_bytes_, tbf_drop_bytes_);

  qhi_pkts = 0;  qhi_bytes = 0; 
  q_pkts = 0;  q_bytes = 0; 
  tbf_pkts = 0;  tbf_bytes = 0; 
  qhi_enq_bytes_ = 0;  qhi_deq_bytes_ = 0;  qhi_drop_bytes_ = 0; 
  qlo_enq_bytes_ = 0;  qlo_deq_bytes_ = 0;  qlo_drop_bytes_ = 0; 
  tbf_enq_bytes_ = 0;  tbf_deq_bytes_ = 0;  tbf_drop_bytes_ = 0;
  

  statsTimer->resched(0.000100);
}

void SPERCPriQueue::enque(Packet *p) {
	//#ifdef LAVANYA_DEBUG
	if (false) fprintf(stdout,
		"%lf SPERCPriQueue::enque()..%s pkt %s\n",
		Scheduler::instance().clock(),this->name(),
		hdr_sperc_ctrl::get_string(p).c_str());
	//#endif // LAVANYA_DEBUG
	bool hi_prio = isHighPriority(p);
  // we have a limit on total queue size, call this qlim_ or qlimBytes
  // suppose queue in bytes
  int q_pkts = q_->length();
  int q_bytes = q_->byteLength();

  int tbfh_pkts = tbf_hi_->length();
  int tbfh_bytes = tbf_hi_->byteLength();

  int qh_pkts = q_hi_->length();
  int qh_bytes = q_hi_->byteLength();

  // TODO: add q_hi_ also?

  // check if data queue size exceeds limit
  bool data_overflow = (!hi_prio && qib_ && (q_bytes + hdr_cmn::access(p)->size()) >= qlimBytes_);
  //#ifdef LAVANYA_DEBUG
  if (!hi_prio) {
	  if (false) fprintf(stdout,
		  "%lf SPERCPriQueue::enque() %s data_overflow %d qib_ %d q_bytes %d qlimBytes_ %d pkt %s\n",
		  Scheduler::instance().clock(),this->name(), data_overflow, qib_,
		  q_bytes, qlimBytes_,
		  hdr_sperc_ctrl::get_string(p).c_str());
  }
  //#endif // LAVANYA_DEBUG


  // (!qib_ && (q_pkts + 1) >= qlim_) ||
  // check if tbf would overflow, that means control packets coming
  // at more than 1 Gb/s and we should drop them?
  bool tbf_overflow = (hi_prio && qib_ && (tbfh_bytes + hdr_cmn::access(p)->size()) >= tbf_limBytes_);

  //#ifdef LAVANYA_DEBUG
  if (hi_prio) {
	  if (false) fprintf(stdout,
		  "%lf SPERCPriQueue::enque() %s tbf_overflow %d qib_ %d tbfh_bytes %d tbf_limBytes_ %d pkt %s\n",
		  Scheduler::instance().clock(),this->name(), tbf_overflow, qib_, 
		  tbfh_bytes, tbf_limBytes_,
		  hdr_sperc_ctrl::get_string(p).c_str());
  }
  //#endif // LAVANYA_DEBUG

  // (!qib_ && (tbfh_pkts + 1) >= sperc_tbf_qlim_)
  //||

  // lo prio path
  if (never_drop_hi_ and tbf_overflow) {
    // we will not drop a hi_prio packet if never_drop_hi_ was set to true
	  fprintf(stderr, "SPERCPriQueue: warning! enqueueing ctrl packet would overflow. Set tbf_overflow to 0\n"); 
	  tbf_overflow = false;
  }

  // lo prio path
  if (!hi_prio) {
	  if (data_overflow) {
		  if (false) fprintf(stdout,
			  "%lf SPERCPriQueue::enque() %s data_overflow %d so dropping data pkt %s\n",
			  Scheduler::instance().clock(),this->name(), data_overflow,
			  hdr_sperc_ctrl::get_string(p).c_str());

		  qlo_drop_bytes_ += hdr_cmn::access(p)->size();
		  drop(p);
	  } else {
		  if (false) fprintf(stdout,
			  "%lf SPERCPriQueue::enque() %s data_overflow %d so enqueueing data pkt %s\n",
			  Scheduler::instance().clock(),this->name(), data_overflow,
			  hdr_sperc_ctrl::get_string(p).c_str());

		  qlo_enq_bytes_ += hdr_cmn::access(p)->size();
		  q_->enque(p);
	  }
  } else {
    // check if queue will overflow
    // we will never drop a packet that has been enqueued
    if (tbf_overflow) {
	    qhi_drop_bytes_ += hdr_cmn::access(p)->size(); // actually pre-tbf
	    drop(p);
	    fprintf(stderr, "SPERCPriQueue: warning! dropping control packet.\n");
	    exit(1);
    } else {

	    if (false) fprintf(stdout,
		    "%lf SPERCPriQueue::enque() %s tbf_overflow %d so enqueueing ctrl pkt %s\n",
		    Scheduler::instance().clock(),this->name(), tbf_overflow,
		    hdr_sperc_ctrl::get_string(p).c_str());

	    
	    //      q_hi_->enque(p);
	    tbf_hi_->enque(p); // update stats in MyTBF::enque() where pkt either enqueued on tbf q or q_hi or dropped
    }
  }
}

Packet* SPERCPriQueue::deque()
{
	if (q_hi_->length() > 0) {
	    if (false) fprintf(stdout,
		    "%lf SPERCPriQueue::deque() %s q_hi_length() %d so get ctrl pkt\n",
		    Scheduler::instance().clock(),this->name(), q_hi_->length());

	    
	    Packet *p = q_hi_->deque();
	    if (p) qhi_deq_bytes_ += hdr_cmn::access(p)->size();
	    return p;

	} else {
	    if (false) fprintf(stdout,
		    "%lf SPERCPriQueue::deque() %s q_hi_length() %d so get data pkt\n",
		    Scheduler::instance().clock(),this->name(), q_hi_->length());

	    Packet *p = q_->deque();
	    if (p) qlo_deq_bytes_ += hdr_cmn::access(p)->size();
	    return p;
	}
}
