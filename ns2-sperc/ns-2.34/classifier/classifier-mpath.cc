/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/*
 * Copyright (C) 1997 by the University of Southern California
 * $Id: classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/classifier/classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $ (USC/ISI)";
#endif

#include "classifier.h"
#include "connector.h"
#include "ip.h"

class MultiPathForwarder : public Classifier {
public:
	
	MultiPathForwarder() : ns_(0), nodeid_(0),  perflow_(0), symmetric_(0), sorted_maxslot_(-1) {
		bind("nodeid_", &nodeid_); 
		bind("perflow_", &perflow_); // hash on flow id, src, dst
		bind("symmetric_", &symmetric_); // symmetric routing (in DC topo)
} 
	virtual int classify(Packet* p) {
		int cl;
		hdr_ip *h = hdr_ip::access(p);
		if (perflow_ && symmetric_) {
			// sort next hops by tonodeid_ for symmetric routing
			cond_sort_slot();      
			//print_slot();

			// per-flow multipath like in pfabric
			struct hkey {
				int src_dst_xor;
				nsaddr_t low32, hi32;
				int fid;
			};
			struct hkey buf_;		
			buf_.src_dst_xor = (h->saddr() ^ h->daddr()) & 0x0000ffff;
			buf_.low32 = 
				(h->saddr() < h->daddr()) ? 
				mshift(h->saddr()) : mshift(h->daddr()) ;
			
			buf_.hi32 = 
				(h->saddr() < h->daddr()) ? 
				mshift(h->daddr()) : mshift(h->saddr()) ;
	  
			buf_.fid = h->flowid();

			char *bufString = (char*) &buf_;
			int length = sizeof(hkey);

			//printf("nodeid = %d, low32 = %d, hi32 = %d, fid = %d, saddr = %d, daddr = %d\n", 
			//       nodeid_, buf_.low32, buf_.hi32, buf_.fid, h->saddr(), h->daddr());

			unsigned int ms_ = (unsigned int)HashString(bufString, length);
			ms_ %= (maxslot_ + 1);
			unsigned int fail = ms_;
			do {
				cl = ms_++;
				ms_ %= (maxslot_ + 1);
			} while (slot_[cl] == 0 && ms_ != fail);
			return cl;

		} else {
			fprintf(stderr, "warning! sperc should be run with perflow_ and symmetric_\n");
			int cl;
			int fail = ns_;			
			do {
				cl = ns_++;
				ns_ %= (maxslot_ + 1);
			} while (slot_[cl] == 0 && ns_ != fail);
			return cl;
		}
	}

	static int slotcmp (const void *a, const void *b) {
		int ma = -1, mb = -1;
		if (a && *((NsObject**)a)) ma = (*((Connector **) a))->tonodeid();  
		if (b && *((NsObject**)b)) mb = (*((Connector **) b))->tonodeid();  
		return ma - mb;
	}

	void cond_sort_slot();
	void print_slot();

private:
	int ns_;
	int nodeid_;
	int perflow_;
	int symmetric_;
	int sorted_maxslot_;
	static unsigned int
	HashString(register const char *bytes,int length)
	{
		register unsigned int result;
		register int i;

		result = 0;
		for (i = 0;  i < length;  i++) {
			result += (result<<3) + *bytes++;
		}
		return result;
	}

};

void MultiPathForwarder::cond_sort_slot() {
	// assuming slot objects of multi-path classifier
	// are connectors with tonodeid_ initialized correctly
	// then this sorts all the slot objects in order of their tonodeid_
	// slot_ must be populated from 0 to maxslot_
	// slot_[i] must be the address of a valid connector always (?)
	// if it is, the key used for sorting is tonodeid_
	// otherwise, the key for sorting is just -1.
	if (sorted_maxslot_ == -1 || maxslot_ > sorted_maxslot_) {
		qsort(slot_, maxslot_+1, sizeof(NsObject*), MultiPathForwarder::slotcmp);
		sorted_maxslot_ = maxslot_;
	}
}

void MultiPathForwarder::print_slot() {
    fprintf(stdout, "slots at multipath forwarder at node %d: ", nodeid_);
    for (int i = 0; i <= maxslot_; i++) {
      if (slot_[i] and *((NsObject**) slot_[i])) {
	int m = ((Connector *) slot_[i])->tonodeid();
	fprintf(stdout, "slot_[%d] = %llu, slot_[%d]->tonodeid() = %d, ", i, ((unsigned long long) slot_[i]), i, m);
      } else if (slot_[i]) {
	fprintf(stdout, "*(slot_[%d]) = %llu, ", 
		i, (unsigned long long) slot_[i]);
      } else {
	fprintf(stdout, "&slot_[%d] = %llu, ", 
		i, (unsigned long long) (&(slot_[i])));
      }
    }
    fprintf(stdout, "\n");
  }

static class MultiPathClass : public TclClass {
public:
	MultiPathClass() : TclClass("Classifier/MultiPath") {} 
	TclObject* create(int, const char*const*) {
		return (new MultiPathForwarder());
	}
} class_multipath;
