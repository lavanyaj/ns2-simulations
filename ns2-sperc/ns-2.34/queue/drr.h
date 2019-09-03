/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*-
 *
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
 *
 * This file contributed by Sandeep Bajaj <bajaj@parc.xerox.com>, Mar 1997.
 *
 * $Header: /cvsroot/nsnam/ns-2/queue/drr.h,v 1.11 2005/08/26 05:05:29 tomh Exp $
 */

#ifndef ns_drr_h
#define ns_drr_h

#include "config.h"   // for string.h
#include <stdlib.h>
#include "queue.h"
#include "packet.h"
#include "sperc/sperc-hdrs.h"

class PacketDRR;
class DRR;

class PacketDRR : public PacketQueue {
	PacketDRR(): pkts(0),src(-1),bcount(0),prev(0),next(0),deficitCounter(0),turn(0) {}
	friend class DRR;
	protected :
	int index;
	int pkts;
	int src;    //to detect collisions keep track of actual src address
	int bcount; //count of bytes in each flow to find the max flow;
	PacketDRR *prev;
	PacketDRR *next;
	int deficitCounter; 
	int turn;
	inline PacketDRR * activate(PacketDRR *head) {
		if (head) {
			this->prev = head->prev;
			this->next = head;
			head->prev->next = this;
			head->prev = this;
			return head;
		}
		this->prev = this;
		this->next = this;
		return this;
	}
	inline PacketDRR * idle(PacketDRR *head) {
		if (head == this) {
			if (this->next == this)
				return 0;
			this->next->prev = this->prev;
			this->prev->next = this->next;
			return this->next;
		}
		this->next->prev = this->prev;
		this->prev->next = this->next;
		return head;
	}
};

class DRR : public Queue {
	public :
	DRR();
	virtual int command(int argc, const char*const* argv);
	Packet *deque(void);
	void enque(Packet *pkt);
	int hash(Packet *pkt);
	void clear();
protected:
	int buckets_ ; //total number of flows allowed
	int blimit_;    //total number of bytes allowed across all flows
	int quantum_;  //total number of bytes that a flow can send
	int mask_;     /*if set hashes on just the node address otherwise on 
			 node+port address*/
	double dbl_sperc_control_traffic_pc_;
	int mul_;
	int bytecnt ; //cumulative sum of bytes across all flows
	int pktcnt ; // cumulative sum of packets across all flows
	int flwcnt ; //total number of active flows
	PacketDRR *curr = 0; //current active flow
	PacketDRR *drr = 0; //pointer to the entire drr struct

	inline PacketDRR *getMaxflow (PacketDRR *curr) { //returns flow with max pkts
		int i;
		PacketDRR *tmp;
		PacketDRR *maxflow=curr;
		for (i=0,tmp=curr; i < flwcnt; i++,tmp=tmp->next) {
			if (maxflow->bcount < tmp->bcount)
				maxflow=tmp;
		}
		return maxflow;
	}
  
public:
	//returns queuelength in packets
	inline int length () {
		return pktcnt;
	}

	//returns queuelength in bytes
	inline int blength () {
		return bytecnt;
	}
};

static class DRRClass : public TclClass {
public:
	DRRClass() : TclClass("Queue/DRR") {}
	TclObject* create(int, const char*const*) {
		return (new DRR);
	}
} class_drr;

#endif // ns_drr_h
