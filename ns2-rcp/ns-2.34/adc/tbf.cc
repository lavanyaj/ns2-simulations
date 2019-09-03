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

#include <string>
#include <sstream>

#include "connector.h" 
#include "packet.h"
#include "queue.h"
#include "tbf.h"

TBF_Value::TBF_Value(TBF *tbf): tbf_(tbf), tokens_(0), tbf_timer_(this), init_(1) {
	q_ = new PacketQueue();
}

TBF_Value::~TBF_Value() {
	if (q_->length() != 0) {
		//Clear all pending timers
		tbf_timer_.cancel();
		//Free up the packetqueue
		for (Packet *p = q_->head(); p != 0; p = p->next_) 
			Packet::free(p);
	}
	delete q_;
}

// TBF::~TBF() {
// 	// TODO(ionel): Delete TBF_Values from tbfs_.
// }

void TBF::recv(Packet *p, Handler *) {
	hdr_ip* iph = hdr_ip::access(p);
	fid_tbf_itr it = tbfs_.find(iph->flowid());
	if (it == tbfs_.end()) {
		// Just send packets because the rate limiting is not active for this flow.
		target_->recv(p);
		return;
	}
	TBF_Value* tbf = it->second;
	iph->prio() = tbf->prio_;
	if (tbf->init_) {
		//start with a full bucket
		//		tbf->tokens_ = tbf->bucket_;
		tbf->tokens_ = tbf->bucket_;
		tbf->lastupdatetime_ = Scheduler::instance().clock();
		tbf->init_ = 0;
	}

	hdr_cmn *ch = hdr_cmn::access(p);
	//enque packets appropriately if a non-zero q already exists
	if (tbf->q_->length() != 0) {
		if (tbf->q_->length() < tbf->qlen_) {
			tbf->q_->enque(p);
			return;
		}
		drop(p);
		return;
	}
	double tok;
	tok = tbf->getupdatedtokens();

	int pktsize = ch->size() << 3;
	if (tbf->tokens_ >= pktsize) {
		target_->recv(p);
		tbf->tokens_ -= pktsize;
	} else {
		if (tbf->qlen_ != 0) {
			tbf->q_->enque(p);
			tbf->tbf_timer_.resched((pktsize - tbf->tokens_) / tbf->rate_);
		} else {
			drop(p);
		}
	}
}

double TBF_Value::getupdatedtokens() {
	double now = Scheduler::instance().clock();
	
	tokens_ += (now - lastupdatetime_) * rate_;
	if (tokens_ > bucket_) {
		tokens_ = bucket_;
	}
	lastupdatetime_ = Scheduler::instance().clock();
	return tokens_;
}

void TBF_Value::timeout(int) {
	if (q_->length() == 0) {
		fprintf (stderr,"ERROR in tbf\n");
		abort();
	}
	
	Packet *p = q_->deque();
	double tok;
	tok = getupdatedtokens();
	hdr_cmn *ch = hdr_cmn::access(p);
	int pktsize = ch->size() << 3;

	//We simply send the packet here without checking if we have enough tokens
	//because the timer is supposed to fire at the right time
	tbf_->target_->recv(p);
	tokens_ -= pktsize;

	if (q_->length() !=0 ) {
		p = q_->head();
		hdr_cmn *ch = hdr_cmn::access(p);
		pktsize = ch->size() << 3;
		tbf_timer_.resched((pktsize - tokens_) / rate_);
	}
}

int TBF::command(int argc, const char* const* argv) {
	if (argc == 7) {
		if (strcmp("activate-fid", argv[1]) == 0) {
			int fid = atoi(argv[2]);
			long long rate;
			long long bucket;
			string text = argv[3];
			stringstream buffer_rate(text);
			buffer_rate >> rate;
			text = argv[4];
			stringstream buffer_bucket(text);
			buffer_bucket >> bucket;
			int qlen = atoi(argv[5]);
			int prio = atoi(argv[6]);
			activate_fid(fid, rate, bucket, qlen, prio);
			return TCL_OK;
		}
	}
	return Connector::command(argc, argv);
}

void TBF::activate_fid(int fid, long long rate, long long bucket, int qlen, int prio) {
	fid_tbf_itr it = tbfs_.find(fid);
	if (it != tbfs_.end()) {
		TBF_Value* tbf_val = it->second;
		tbfs_.erase(it);
		delete tbf_val;
	}
	TBF_Value* tbf = new TBF_Value(this);
	tbf->flow_id_ = fid;
	tbf->rate_ = rate;
	tbf->bucket_ = bucket;
	tbf->qlen_ = qlen;
	tbf->prio_ = prio;
	tbfs_.insert(fid_tbf_val(fid, tbf));
}

void TBF_Timer::expire(Event* /*e*/)
{
	tbf_value_->timeout(0);
}

static class TBFClass : public TclClass {
public:
	TBFClass() : TclClass ("TBF") {}
	TclObject* create(int,const char*const*) {
		return (new TBF());
	}
}class_tbf;
