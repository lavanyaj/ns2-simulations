
/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved. 2017 Stanford Univeristy.
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
 * Author : Lavanya Jose
 * A priority queue with two priority classess- high priority packets are dequeued first.
 * One can configure a minimum buffer size that is reserved for high priority packets
 * out of a total buffer size. Currently, only PT_SPERC_CTRL packets are high prio.
 * There is also an option to never drop high priority packets.
 */

#ifndef _sperc_priqueue_h
#define _sperc_priqueue_h

#include <memory>
#include <string>
#include "object.h"
#include "timer-handler.h"
#include "queue.h"
#include "packet.h"
#include "my_tbf.h"

class SPERCPriQueue;
class MyTBF;

class SPERCPriQueueTimer : public TimerHandler {
 public:
 SPERCPriQueueTimer(SPERCPriQueue *a, void (SPERCPriQueue::*call_back)() ) : a_(a), call_back_(call_back) {};
protected:
virtual void expire (Event *e);
SPERCPriQueue *a_;
void (SPERCPriQueue::*call_back_)();
};

class SPERCPriQueue : public Queue {
  friend SPERCPriQueueTimer;
  friend MyTBF;
  unique_ptr<SPERCPriQueueTimer> statsTimer;
public:
        SPERCPriQueue();
  ~SPERCPriQueue();
        int     command(int argc, const char*const* argv);
        void enque(Packet *p);
	Packet* deque();

        // TBF tbf_hi_ with target_ = q_hi_
        // this will ensure that rate of arrival at q_hi_
        // is exactly 1 Gb/s. What about length of q_hi_
        // And we serve q_hi_ whenever it's not empty.
        // What we want is to serve q_hi_ at no more than
        // 1 Gb/s over  experiment duration timescale.
        // Remove TCL deps from TBF to make MyTBF
        
        // Make TBF queue length 0 and q_hi_ length non zero
        // so it drops packets if there are no tokens
        // Since q_hi_ is served w prio at line rate
        // and tokens are generated at 1 G/bs by TBF
        // q_hi_ fills at rate no more than 1 Gb/s.
        // So control packets are served at rate 1 Gb/s?
        // q_hi_ would fill up when a data packet is being
        // served 1500 bytes takes 1.2us. At 1 Gb/s at most 3.75
        // control packets.
        // But this means control packet rates are not smoothed out
        // they are jut dropped if their rate exceeds 1 Gb/s.

        //  Make TBF queue length non zero and q_hi_ length 0
        //  So TBF drops packets only when it fills up.
        //  If control packets temporarily exceeds quota they will
        //  queue up in TBF instead of being dropped.
        //  This is not correct.

        //  Make TBF queue length non zero and q_hi_ length non zero
        //  So TBF drops packets only when it fills up.
        //  If control packets temporarily exceeds quota they will
        //  queue up in TBF instead of being dropped.
        //  Since q_hi_ is served w prio at line rate
        //  and tokens are generated at 1 G/bs by TBF
        //  q_hi_ fills at rate no more than 1 Gb/s.
        //  So control packets are served at rate 1 Gb/s?
        //  q_hi_ would fill up when a data packet is being
        //  served 1500 bytes takes 1.2us. At 1 Gb/s at most 3.75
        //  control packets.
        // This looks correct.

        // Make this Queue a drop target?
  MyTBF *tbf_hi_;  
  PacketQueue *q_hi_; // hi prio
  PacketQueue *q_;  // lo prio

  int summarystats; 
  int first_summarystats;
  void print_summarystats();

  int qib_; /* bool: queue measured in bytes? */
  int never_drop_hi_; /* bool: never drop (always enqueue) hi prio packets? */

  int tbf_limBytes_; /* max bytes available for ctrl tbf */
  int qhi_limBytes_; /* max bytes available for ctrl q */
  int qlimBytes_; /* max bytes available for data packets */
  /* all three must be set if qib_ = true */

  double sperc_tbf_rate_; // bits / s
  int sperc_tbf_qlen_; // packets
  int sperc_tbf_bucket_; // bits

  // for monitoring
  int qhi_enq_bytes_; // enqueued from tbf
  int qhi_deq_bytes_;
  int qhi_drop_bytes_;

  int qlo_enq_bytes_;
  int qlo_deq_bytes_;
  int qlo_drop_bytes_;

  int tbf_drop_bytes_; // dropped from enque() if tbf would overflow
  int tbf_enq_bytes_; // enqueued from enque() if tbf won't overflow
  int tbf_deq_bytes_; // dequeue by timeout
private:
        int Prefer_Routing_Protocols;
        bool isHighPriority(Packet *p);
};

#endif /* !_sperc_priqueue_h */
