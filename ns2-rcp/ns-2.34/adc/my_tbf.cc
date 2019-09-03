/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) Xerox Corporation 1997. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linking this file statically or dynamically with other modules is making
 * a combined work based on this file.  Thus, the terms and conditions of
 * the GNU General Public License cover the whole combination.
 *
 * In addition, as a special exception, the copyright holders of this file
 * give you permission to combine this file with free software programs or
 * libraries that are released under the GNU LGPL and with code included in
 * the standard release of ns-2 under the Apache 2.0 license or under
 * otherwise-compatible licenses with advertising requirements (or modified
 * versions of such code, with unchanged license).  You may copy and
 * distribute such a system following the terms of the GNU GPL for this
 * file and the licenses of the other code concerned, provided that you
 * include the source code of that other code when and as the GNU GPL
 * requires distribution of source code.
 *
 * Note that people who make modified versions of this file are not
 * obligated to grant this special exception for their modified versions;
 * it is their choice whether to do so.  The GNU General Public License
 * gives permission to release a modified version without this exception;
 * this exception also makes it possible to release a modified version
 * which carries forward this exception.
 */

/* Token Bucket filter which has  3 parameters :
   a. Token Generation rate
   b. Token bucket depth
   c. Max. Queue Length (a finite length would allow this to be used as  policer as packets are dropped after queue gets full)
   */

//#include "connector.h" 
#include "packet.h"
#include "queue.h"
#include "my_tbf.h"
#include "sperc/sperc-hdrs.h" // to log dropped packets


MyTBF::MyTBF(double rate, int bucket, int qlen) :
	tokens_(0), rate_(rate), bucket_(bucket), qlen_(qlen),
	tbf_timer_(this), init_(1)       
{
	q_ = new PacketQueue();
	// bind_bw("rate_",&rate_);
	// bind("bucket_",&bucket_);
	// bind("qlen_",&qlen_);
}
	
MyTBF::~MyTBF()
{
	if (q_->length() != 0) {
		//Clear all pending timers
		tbf_timer_.cancel();
		//Free up the packetqueue
		for (Packet *p=q_->head();p!=0;p=p->next_) 
			Packet::free(p);
	}
	delete q_;
}

int MyTBF::length() {
	return q_->length();
}

int MyTBF::byteLength() {
	return q_->byteLength();
}

void MyTBF::recv(Packet *p, Handler *)
{
	fprintf(stderr,
		"%lf MyTBF::recv()  not implemented pkt %s\n",
		Scheduler::instance().clock(), hdr_sperc_ctrl::get_string(p).c_str());
	exit(1);


}

void MyTBF::enque(Packet *p)
{
	//start with a full bucket
	if (init_) {
		tokens_=bucket_;
		lastupdatetime_ = Scheduler::instance().clock();
		init_=0;

		if (false) fprintf(stdout,
			"%lf MyTBF::recv()  init tokens_ %f \n",
			Scheduler::instance().clock(),
			tokens_);
		//			hdr_sperc_ctrl::get_string(p).c_str());
	}

	
	hdr_cmn *ch=hdr_cmn::access(p);

	//enque packets appropriately if a non-zero q already exists
	if (q_->length() !=0) {
		if (q_->length() < qlen_) {
			if (false) fprintf(stdout, 
			"%lf MyTBF::enque() %s q_->length() %d qlen_ %d enqueueing pkt.\n",
			Scheduler::instance().clock(),this->name(),
				q_->length(), qlen_);
			//				hdr_sperc_ctrl::get_string(p).c_str());
			
			parent_q_->tbf_enq_bytes_ += hdr_cmn::access(p)->size();
			q_->enque(p);
			return;
		}

		if (false) fprintf(stdout, 
			"%lf MyTBF::enque() %s q_->length() %d qlen_ %d dropping pkt.\n",
			Scheduler::instance().clock(),this->name(),
			q_->length(), qlen_);
		//			hdr_sperc_ctrl::get_string(p).c_str());
		parent_q_->tbf_drop_bytes_ += hdr_cmn::access(p)->size();
		parent_q_->drop(p);
		return;
	}

	double tok;
	tok = getupdatedtokens();

	int pktsize = ch->size()<<3;
	if (tokens_ >=pktsize) {

		if (false) fprintf(stdout, 
			"%lf MyTBF::enque() %s q_->length() %d qlen_ %d tokens_ %f pktsize %d calling target_q_->enque pkt.\n",
			Scheduler::instance().clock(),this->name(),
			q_->length(), qlen_, tokens_, pktsize);
		//			hdr_sperc_ctrl::get_string(p).c_str());

		parent_q_->qhi_enq_bytes_ += hdr_cmn::access(p)->size();
		parent_q_->tbf_deq_bytes_ += hdr_cmn::access(p)->size();
		target_q_->enque(p);
		tokens_-=pktsize;
	}
	else {
		
		if (qlen_!=0) {

			if (false) fprintf(stdout, 
				"%lf MyTBF::enque() %s q_->length() %d qlen_ %d tokens_ %f pktsize %d enqueueing pkt and rescheduling for %f us.\n",
				Scheduler::instance().clock(),this->name(),
				q_->length(), qlen_, tokens_, pktsize,
				((pktsize-tokens_)/rate_)*1e6);
			//				hdr_sperc_ctrl::get_string(p).c_str());

			parent_q_->tbf_enq_bytes_ += hdr_cmn::access(p)->size();
			q_->enque(p);
			tbf_timer_.resched((pktsize-tokens_)/rate_);
		}
		else {
			if (false) fprintf(stdout, "%lf MyTBF::enque() %s dropping pkt.\n",
				Scheduler::instance().clock(),this->name());
			//				hdr_sperc_ctrl::get_string(p).c_str());
			parent_q_->tbf_drop_bytes_ += hdr_cmn::access(p)->size();
			parent_q_->drop(p);			
		}
	}
}

double MyTBF::getupdatedtokens(void)
{
	double now=Scheduler::instance().clock();
	
	tokens_ += (now-lastupdatetime_)*rate_;
	if (tokens_ > bucket_)
		tokens_=bucket_;
	lastupdatetime_ = Scheduler::instance().clock();
	return tokens_;
}

void MyTBF::timeout(int)
{
	if (q_->length() == 0) {
		fprintf (stderr,"ERROR in tbf\n");
		abort();
	}
	
	Packet *p=q_->deque();
	double tok;
	tok = getupdatedtokens();
	hdr_cmn *ch=hdr_cmn::access(p);
	int pktsize = ch->size()<<3;

	//We simply send the packet here without checking if we have enough tokens
	//because the timer is supposed to fire at the right time
	parent_q_->qhi_enq_bytes_ += hdr_cmn::access(p)->size();
	parent_q_->tbf_deq_bytes_ += hdr_cmn::access(p)->size();
	target_q_->enque(p);
	tokens_-=pktsize;

	if (false) fprintf(stdout, 
		"%lf MyTBF::timeout() %s q_->length() %d qlen_ %d tokens_ %f pktsize %d dequeueing pkt and calling target_q_->enque which is now %d bytes  %s.\n",
		Scheduler::instance().clock(),this->name(),
		q_->length(), qlen_, tokens_, pktsize,
		target_q_->byteLength(),
		hdr_sperc_ctrl::get_string(p).c_str());

	if (q_->length() !=0 ) {
		p=q_->head();
		hdr_cmn *ch=hdr_cmn::access(p);
		pktsize = ch->size()<<3;
		tbf_timer_.resched((pktsize-tokens_)/rate_);

	if (false) fprintf(stdout, 
		"%lf MyTBF::timeout() %s  qlen_ %d tokens_ %f pktsize %d dequeueing pkt and calling target_q_->enque  %s.\n",
		Scheduler::instance().clock(),this->name(),
		qlen_, tokens_, pktsize,
		hdr_sperc_ctrl::get_string(p).c_str());
		
	}
}

void MyTBF_Timer::expire(Event* /*e*/)
{
	tbf_->timeout(0);
}


// static class MyTBFClass : public TclClass {
// public:
// 	MyTBFClass() : TclClass ("MyTBF") {}
// 	TclObject* create(int,const char*const*) {
// 		return (new MyTBF());
// 	}
// }class_tbf;
