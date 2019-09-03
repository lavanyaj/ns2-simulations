/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 The Regents of the University of California.
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
 * 	This product includes software developed by the Network Research
 * 	Group at Lawrence Berkeley National Laboratory.
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

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/classifier/classifier-hash.cc,v 1.30 2005/09/18 23:33:31 tomh Exp $ (LBL)";
#endif

//
// a generalized classifier for mapping (src/dest/flowid) fields
// to a bucket.  "buckets_" worth of hash table entries are created
// at init time, and other entries in the same bucket are created when
// needed
//
//

extern "C" {
#include <tcl.h>
}

#include <stdlib.h>
#include <utility> // pair
#include "config.h"
#include "packet.h"
#include "ip.h"
#include "classifier.h"
#include "classifier-hash.h"
#include "sperc/sperc-hdrs.h" // sperc_hdr fields
#include "connector.h" // to get nodeid form link head_


/****************** HashClassifier Methods ************/

int HashClassifier::classify(Packet * p) {
	int slot= lookup(p);
	if (slot >= 0 && slot <=maxslot_)
		return (slot);
	else if (default_ >= 0)
		return (default_);
	return (unknown(p));
} // HashClassifier::classify

int HashClassifier::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	/*
	 * $classifier set-hash $hashbucket src dst fid $slot
	 */
	if (argc == 7) {
		if (strcmp(argv[1], "set-hash") == 0) {
			//xxx: argv[2] is ignored for now
			nsaddr_t src = atoi(argv[3]);
			nsaddr_t dst = atoi(argv[4]);
			int fid = atoi(argv[5]);
			int slot = atoi(argv[6]);
			if (0 > set_hash(src, dst, fid, slot))
				return TCL_ERROR;
			return TCL_OK;
		}
	} else if (argc == 6) {
		/* $classifier lookup $hashbuck $src $dst $fid */
		if (strcmp(argv[1], "lookup") == 0) {
			nsaddr_t src = atoi(argv[3]);
			nsaddr_t dst = atoi(argv[4]);
			int fid = atoi(argv[5]);
			int slot= get_hash(src, dst, fid);
			if (slot>=0 && slot <=maxslot_) {
				tcl.resultf("%s", slot_[slot]->name());
				return (TCL_OK);
			}
			tcl.resultf("");
			return (TCL_OK);
		}
                // Added by Yun Wang to set rate for TBFlow or TSWFlow
                if (strcmp(argv[1], "set-flowrate") == 0) {
                        int fid = atoi(argv[2]);
                        nsaddr_t src = 0;  // only use fid
                        nsaddr_t dst = 0;  // to classify flows
                        int slot = get_hash( src, dst, fid );
                        if ( slot >= 0 && slot <= maxslot_ ) {
                                Flow* f = (Flow*)slot_[slot];
                                tcl.evalf("%u set target_rate_ %s",
                                        f, argv[3]);
                                tcl.evalf("%u set bucket_depth_ %s",
                                        f, argv[4]);
                                tcl.evalf("%u set tbucket_ %s",
                                        f, argv[5]);
                                return (TCL_OK);
                        }
                        else {
                          tcl.evalf("%s set-rate %u %u %u %u %s %s %s",
                          name(), src, dst, fid, slot, argv[3], argv[4],argv[5])
;
                          return (TCL_OK);
                        }
                }  
	} else if (argc == 5) {
		/* $classifier del-hash src dst fid */
		if (strcmp(argv[1], "del-hash") == 0) {
			nsaddr_t src = atoi(argv[2]);
			nsaddr_t dst = atoi(argv[3]);
			int fid = atoi(argv[4]);
			
			Tcl_HashEntry *ep= Tcl_FindHashEntry(&ht_, 
							     hashkey(src, dst,
								     fid)); 
			if (ep) {
				long slot = (long)Tcl_GetHashValue(ep);
				Tcl_DeleteHashEntry(ep);
				tcl.resultf("%lu", slot);
				return (TCL_OK);
			}
			return (TCL_ERROR);
		}
	}
	return (Classifier::command(argc, argv));
}

/**************  TCL linkage ****************/
static class SrcDestHashClassifierClass : public TclClass {
public:
	SrcDestHashClassifierClass() : TclClass("Classifier/Hash/SrcDest") {}
	TclObject* create(int, const char*const*) {
		return new SrcDestHashClassifier;
	}
} class_hash_srcdest_classifier;

static class FidHashClassifierClass : public TclClass {
public:
	FidHashClassifierClass() : TclClass("Classifier/Hash/Fid") {}
	TclObject* create(int, const char*const*) {
		return new FidHashClassifier;
	}
} class_hash_fid_classifier;

static class DestHashClassifierClass : public TclClass {
public:
	DestHashClassifierClass() : TclClass("Classifier/Hash/Dest") {}
	TclObject* create(int, const char*const*) {
		return new DestHashClassifier;
	}
} class_hash_dest_classifier;

static class SrcDestFidHashClassifierClass : public TclClass {
public:
	SrcDestFidHashClassifierClass() : TclClass("Classifier/Hash/SrcDestFid") {}
	TclObject* create(int, const char*const*) {
		return new SrcDestFidHashClassifier;
	}
} class_hash_srcdestfid_classifier;


// DestHashClassifier methods
int DestHashClassifier::classify(Packet *p)
{
	int slot= lookup(p);
	if (slot >= 0 && slot <=maxslot_)
		return (slot);
	else if (default_ >= 0)
		return (default_);
	return -1;
} // HashClassifier::classify

void DestHashClassifier::do_install(char* dst, NsObject *target) {
	nsaddr_t d = atoi(dst);
	int slot = getnxt(target);
	install(slot, target); 
	if (set_hash(0, d, 0, slot) < 0)
		fprintf(stderr, "DestHashClassifier::set_hash from within DestHashClassifier::do_install returned value < 0");
}

int DestHashClassifier::command(int argc, const char*const* argv)
{
	if (argc == 4) {
		// $classifier install $dst $node
		if (strcmp(argv[1], "install") == 0) {
			char dst[SMALL_LEN];
			strcpy(dst, argv[2]);
			NsObject *node = (NsObject*)TclObject::lookup(argv[3]);
			//nsaddr_t dst = atoi(argv[2]);
			do_install(dst, node); 
			return TCL_OK;
			//int slot = getnxt(node);
			//install(slot, node);
			//if (set_hash(0, dst, 0, slot) >= 0)
			//return TCL_OK;
			//else
			//return TCL_ERROR;
		} // if
	}
	return(HashClassifier::command(argc, argv));
} // command

// SPERC Classifier - symmetric routing, SPERC processing based on ingress/ egress link
static class SPERCDestHashClassifierClass : public TclClass {
	public:
	       SPERCDestHashClassifierClass() : TclClass("Classifier/Hash/Dest/SPERC") {}
	       TclObject* create(int, const char*const*) {
		               return new SPERCDestHashClassifier;
		       }
} class_hash_spercdest_classifier;

int SPERCDestHashClassifier::command(int argc, const char*const* argv)
{

#ifdef LAVANYA_DEBUG
	fprintf(stdout, "%s SPERCDestHashClassifier::command(%d,[",name(), argc);
        for (int i = 0; i < argc; i++) {
		fprintf(stdout, "%s,", argv[i]);
	}
        fprintf(stdout, "])\n");
#endif // LAVANYA_DEBUG
	// Log when a multi-path classifier is inserted in a slot (in ns-node.tcl)
	// so we know there's one more level of indirection to get to head of
	// egress link
	if (argc == 3) {
		if (strcmp(argv[1], "log-mpath-slot") == 0) {
			NsObject *node = (NsObject*)TclObject::lookup(argv[2]);
			#ifdef LAVANYA_DEBUG
			if (node) fprintf(stdout, "logging object %llu as multi-path\n", node);
			
			#endif // LAVANYA_DEBUG
			if (node) {
				is_mp_slot_[node] = true;
				return (TCL_OK);
			} else {
				fprintf(stderr,
					"couldn't find object %llu (log-mpath-slot)\n",	node);
				return (TCL_ERROR);
			}
		} else if (strcmp(argv[1], "log-demux") == 0) {
			NsObject *node = (NsObject*)TclObject::lookup(argv[2]);

			if (!demux_ and node) {
				demux_ = node;
				demux_name_.assign(argv[2]);
				return (TCL_OK);
			} else if (demux_ and node) {
				fprintf(stderr,
					"already set object %llu (%s) as demux, can't set new object %llu (log-demux %s) \n",\
					demux_, demux_name_.c_str(), node, argv[2]);
				return (TCL_ERROR);
			} else if (demux_ and !node) {
				fprintf(stderr,
					"already set object %llu (%s) as demux. anyways couldn't find object %s \n",\
					demux_, demux_name_.c_str(), argv[2]);
				return (TCL_ERROR);

			} else if (!demux_ and !node) {
				fprintf(stderr,
					"haven't set any objext as demux and couldn't find object %s (log-demux)\n", argv[2]);
				return (TCL_ERROR);
			}
		}

	} else if (argc == 5) {
		if (strcmp(argv[1], "setup-sperc") == 0) {
			// called in ns-lib.tcl as part of instproc simplex-link
			int neighbor_id = atoi(argv[2]);
			double bw = atof(argv[3]);
			SPERCQueue * q;
			if (!(q = (SPERCQueue*) TclObject::lookup(argv[4])))
				return (TCL_ERROR);
			#ifdef LAVANYA_DEBUG
			fprintf(stdout, "classifier-hash.cc: set up sperc link between this node id %d to neighbor node id %d with link capacity %f\n", nodeid_, neighbor_id, bw);
			#endif
			auto egress = make_pair(nodeid_, neighbor_id);
			auto ingress = make_pair(neighbor_id, nodeid_);
			if (cltable.count(egress) or cltable.count(ingress)) {
				fprintf(stderr, "setting up duplicate link between node id %d and neighbor node id %d\n", nodeid_, neighbor_id);
			}
			// some way to store map from egress to actual link object??

			// we will set up a single link state for duplex link
			// which has ingress link = (neighbor_id, nodeid_)
			// and egress link = (nodeid_, neighbor_id)
			// we will use the egress link name (nodeid_, neighbor_id)
			// to index into this link state
			//cltable[ingress].initialize(bw, this, neighbor_id, nodeid_);
			cltable[egress].initialize((bubble) bw, this, nodeid_, neighbor_id, q);
			//cltable[egress] = SPERCLinkState();
			// bw is in Gb/s - maybe we need a function to go from b/s
			// some rate units based on how much precision we have??
			// assuming links have same bw and delay in both directions
			//cltable.at(egress).initialize(bw, 
			//			      this, nodeid_, neighbor_id);
			//cltable.at(ingress).initialize(bw,
			//			       this, neighbor_id, nodeid_);
			return (TCL_OK);
		}
			// TODO: setup-sperc neighbor_id
	}
        return(DestHashClassifier::command(argc, argv));
} // command

/*
 * objects only ever see "packet" events, which come either
 * from an incoming link or a local agent (i.e., packet source).
 * this is where we do SPERC ingress/ egress link processing
 * i.e., at the entry_ of a node, recv is typically called by last
 * element of the ingress link into this node, or by the agent
 * on this node. it's not trivial to get ingress and egress link at node.
 * we get ingress link from path_[hop] in hdr_sperc
 * we get egress link using the slot/ NsObject* returned by classify()/find(), if there's
 * only one path towards the packet's dst, the object returend is the head_
 * of the egress link, we stored the fromnodeid_ and tonodeid_ in that object (in Tcl).
 * if there are multiple paths, the object returned is the mutlipath classifier
 * towards the dst, in that case, using *its* find() method will return the head_
 *  at the egress link.
 */
void SPERCDestHashClassifier::recv(Packet* p, Handler*h)
{

	NsObject* node = find(p);
	if (node == NULL) {
		/*
		 * XXX this should be "dropped" somehow.  Right now,
		 * these events aren't traced.
		 */
		Packet::free(p);
		return;
	}
	// #ifdef LAVANYA_DEBUG
	// fprintf(stdout, 
	//      "%s SPERCDestHashClassifier::recv() find() on packet with uid_ %d returned object %llu\n",
	// 		name(), hdr_cmn::access(p)->uid_, (unsigned long long) node);
	// #endif // LAVANYA_DEBUG
	int ingress_fromnodeid = get_ingress_fromnodeid(p);
	// Sometimes node might be the next classifier in a chain of classifiers
	// at this node, in this case, we can to use node to look up the next hop
	// For now, we only handle the case where the next node is either a multi-path
	// classifier or not.
	int egress_tonodeid = -1;
	int cl1 = classify(p);
	NsObject* node1 = NULL;
	if (cl1 < 0 || cl1 >= nslot_ || (node1 = slot_[cl1]) == 0) { 
		fprintf(stderr,
			"Error! %s SPERC..Classifier::recv find() on packet with uid_ %d returned slot %d to object %llu, is_mp_slot_.count(cl1) is %d\n",
			name(), hdr_cmn::access(p)->uid_, cl1,
			(unsigned long long) node1, is_mp_slot_.count(node1));
		exit(1);
	} else {
		// fprintf(stdout,
		// 	"%s SPERC..Classifier::recv find() on packet with uid_ %d returned slot %d to object %llu, nslot_ is %d, is_mp_slot_.count(cl1) is %d\n",
		// 	name(), hdr_cmn::access(p)->uid_, cl1,
		// 	(unsigned long long) node1, nslot_, is_mp_slot_.count(node1));

	}

	if (demux_ and node1 == demux_) {
		#ifdef LAVANYA_DEBUG
		fprintf(stdout,
			"%s SPERCClassifier::recv(uid_=%d) got  slot %d to demux classifier %llu\n", 
			name(), hdr_cmn::access(p)->uid_, cl1,
			(unsigned long long) node1);
		#endif // LAVANYA_DEBUG
		egress_tonodeid = nodeid_;
	}
	else if (is_mp_slot_.count(node1)) {
		#ifdef LAVANYA_DEBUG
		fprintf(stdout,
			"%s SPERCClassifier::recv(uid_=%d) got  slot %d to multi-path object %llu\n", 
			name(), hdr_cmn::access(p)->uid_, cl1,
			(unsigned long long) node1);
		#endif // LAVANYA_DEBUG
		// node1 is a multipath classifier, we can look it up to find next  node
		// note that this could modify MultiPathForwarder state (sort slots)
		
		NsObject* node2 = ((Classifier *) node1)->find(p);
		egress_tonodeid = ((Connector *) node2)->tonodeid();

		#ifdef LAVANYA_DEBUG
		fprintf(stdout,
			".. which returned object %llu which we hope is a Connector and which got us tonode %d\n",			
			(unsigned long long) node2, 
			egress_tonodeid);
		#endif // LAVANYA_DEBUG
	} else {
		egress_tonodeid = ((Connector *) node1)->tonodeid();
		#ifdef LAVANYA_DEBUG
		fprintf(stdout,
			"%s SPERCClassifier::recv(uid_=%d) got non-multipath slot %d to %llu, nodeid_: %d, egress_tonodeid: %d\n",
			name(), hdr_cmn::access(p)->uid_, cl1,
			(unsigned long long) node1,
			nodeid_,
			egress_tonodeid);
		#endif // LAVANYA_DEBUG
	}

	hdr_cmn* cmn = hdr_cmn::access(p);
	hdr_sperc* hdr = hdr_sperc::access(p);
	const auto egress = std::make_pair(nodeid_, egress_tonodeid);
	const auto ingress = std::make_pair(ingress_fromnodeid, nodeid_);

	// some things to check about egress link

	// catch errors when either node is -1

	// if this a fwd packet received at the destination node i
	// then it's going towards portclassifier w egress link i,i
	// we don't want to process_fwd_pkt in this case

	// if this is a rev packet received at the destination node i
	// then it's coming from the Agent w ingress link i, i
	// we don't want to process_rev_pkt in this case

	// if this is a fwd packet received at the source node i
	// then it's going towards next link (and coming from agent)
	// we do want to process_fwd_pkt

	// if this is a rev packet received at the source node i
	// then it's coming from the previous link
	// we do want to process_rev_pkt
	if (cmn->ptype() == PT_SPERC_CTRL) {
		process_for_control_rate_limiting(egress, ingress, p);	
	}

	if (cmn->ptype() == PT_SPERC_CTRL and hdr->is_fwd() and egress.first != egress.second) {
		#ifdef LAVANYA_DEBUG_PATH
		fprintf(stdout, "SPERCClassifier at node %d: Forward egress processing for pkt with ingress: (%d, %d); egress: (%d, %d); pkt: %s\n", 
			nodeid_, ingress.first, ingress.second, egress.first, egress.second, hdr_sperc_ctrl::get_string(p).c_str());
		#endif // LAVANYA_DEBUG_PATH
		process_forward_pkt(egress, p);

	// some things to check about ingress link
	// if this is a packet received at source node
	} else if (cmn->ptype() == PT_SPERC_CTRL and !hdr->is_fwd() and ingress.first != ingress.second) {
		#ifdef LAVANYA_DEBUG_PATH
		fprintf(stdout, "SPERCClassifier at node %d: Reverse ingress processing for pkt with ingress: (%d, %d); egress: (%d, %d); pkt: %s\n", 
			nodeid_, ingress.first, ingress.second, egress.first, egress.second, hdr_sperc_ctrl::get_string(p).c_str());
		#endif // LAVANYA_DEBUG_PATH
		process_reverse_pkt(ingress, p);
	} else {
		#ifdef LAVANYA_DEBUG_PATH
		fprintf(stdout, "SPERCClassifier at node %d: No SPERC processing ingress: (%d, %d); egress: (%d, %d); pkt: %s\n",
			nodeid_, ingress.first, ingress.second, egress.first, egress.second, hdr_sperc_ctrl::get_string(p).c_str());
		#endif // LAVANYA_DEBUG_PATH
	}

	node->recv(p,h);
}

int SPERCDestHashClassifier::get_ingress_fromnodeid(Packet *p) {
	
	hdr_sperc* hdr = hdr_sperc::access(p);
	if (hdr->is_fwd()) {
		int hop = hdr->fwd_hop();
		#ifdef LAVANYA_DEBUG_PATH
		fprintf(stdout, "SPERCClassifier at node %d: get_ingress_from_nodeid() for fwd pkt, use hop fwd_path_[%d]; pkt: %s\n",
			nodeid_, 
			(hop-1),
			hdr_sperc_ctrl::get_string(p).c_str());
		#endif // LAVANYA_DEBUG_PATH
		if (hop > 0) {
			int nodeid = hdr->fwd_path_[hop-1];
			return nodeid;
		}
	} else {
		int hop = hdr->rev_hop();
		#ifdef LAVANYA_DEBUG_PATH
		fprintf(stdout, "SPERCClassifier at node %d: get_ingress_from_nodeid() for rev pkt, use hop rev_path_[%d]; pkt: %s\n",
			nodeid_, 
			(hop-1), 
			hdr_sperc_ctrl::get_string(p).c_str());
		#endif // LAVANYA_DEBUG_PATH
		if (hop > 0) {
			int nodeid = hdr->rev_path_[hop-1];
			return nodeid;
		}
	}

	#ifdef LAVANYA_DEBUG_PATH
	fprintf(stdout, 
		"SPERCClassifier at node %d: get_ingress_from_nodeid() return -1 for  pkt: %s\n",
		nodeid_, 
		hdr_sperc_ctrl::get_string(p).c_str());
	#endif // LAVANYA_DEBUG_PATH

 return -1;

}
void SPERCDestHashClassifier::process_forward_pkt(const std::pair<int, int>& link_id, Packet *p) {
#ifdef LAVANYA_DEBUG
	fprintf(stdout, "SPERCDestHashClassifier::process_forward_pkt(link=(%d,%d), uid=%d )\n",
		link_id.first, link_id.second, hdr_cmn::access(p)->uid_);
#endif // LAVANYA_DEBUG
	if (!cltable.count(link_id)) {
		fprintf(stderr, "process_forward_pkt: no link state for %d->%d\n",
			link_id.first, link_id.second);
		exit(1);
	}
	cltable.at(link_id).process_egress(p);
}

void SPERCDestHashClassifier::process_reverse_pkt(const std::pair<int, int>& link_id, Packet *p) {
#ifdef LAVANYA_DEBUG
	fprintf(stdout, "SPERCDestHashClassifier::process_reverse_pkt(link=(%d,%d), uid=%d )\n",
		link_id.first, link_id.second, hdr_cmn::access(p)->uid_);
#endif // LAVANYA_DEBUG
	// we will use the name of the egress link to index into the correct link state
	auto egress = make_pair(link_id.second, link_id.first);
	if (!cltable.count(egress)) {
		fprintf(stderr, 
			"process_reverse_pkt: no link state for ingress %d->%d/ egress %d-%d\n",
			link_id.first, link_id.second, egress.first, egress.second);
		exit(1);
	}

	cltable.at(egress).process_ingress(p);
}

void SPERCDestHashClassifier::process_for_control_rate_limiting
(const std::pair<int, int>& egress_id, const std::pair<int, int>& ingress_id,Packet *p) {
	#ifdef LAVANYA_DEBUG
	fprintf(stdout, "SPERCDestHashClassifier::process_for_control_rate_limiting(egress=(%d,%d), ingress=(%d,%d), uid=%d, src=%d, dst=%d ) %s\n",
		egress_id.first, egress_id.second, ingress_id.first, ingress_id.second,
		hdr_cmn::access(p)->uid_, hdr_ip::access(p)->src(), hdr_ip::access(p)->dst(), 
		hdr_sperc_ctrl::get_string(p).c_str());
	#endif // LAVANYA_DEBUG

	// we will use the name of the egress link to index into the correct link state
	
	// update link_state corresponding to (ingress_id.second, ingress_id.first)
	if (ingress_id.first != ingress_id.second) {
		auto link_name = make_pair(ingress_id.second, ingress_id.first);
		if (!cltable.count(link_name)) {
		fprintf(stderr, 
			"process_for_control_rate_limiting: no link state for ingress %d->%d/ egress %d-%d at classifier at node %d\n",
			ingress_id.first, ingress_id.second,
			link_name.first, link_name.second, nodeid_);
		exit(1);
		}
		cltable.at(link_name).process_for_control_rate_limiting(p);	
	}


	// update link_state corresponding to egress_id
	if (egress_id.first != egress_id.second) {
		if (!cltable.count(egress_id)) {
		fprintf(stderr, 
			"process_for_control_rate_limiting: no link state for egress %d-%d/  ingress %d->%d at classifier at node %d\n",
			egress_id.first, egress_id.second, 
			egress_id.second, egress_id.first,  nodeid_);
		exit(1);
		}
		cltable.at(egress_id).process_for_control_rate_limiting(p);	
	}

}
