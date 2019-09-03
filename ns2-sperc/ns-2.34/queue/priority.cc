/*
 * Strict Priority Queueing (SP)
 *
 * Variables:
 * queue_num_: number of CoS queues
 * thresh_: ECN marking threshold
 * mean_pktsize_: configured mean packet size in bytes
 * marking_scheme_: Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
 */

#include "priority.h"
#include "flags.h"
#include "math.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

static class PriorityClass : public TclClass {
 public:
	PriorityClass() : TclClass("Queue/Priority") {}
	TclObject* create(int, const char*const*) {
		return (new Priority);
	}
} class_priority;

// reserve 4% of queue for control packets only (pri 0)
void Priority::enque(Packet* p)
{
	hdr_ip *iph = hdr_ip::access(p);
	int prio = iph->prio();
	hdr_cmn *ch = hdr_cmn::access(p);
	hdr_flags* hf = hdr_flags::access(p);
	int qlimBytes = qlim_ * mean_pktsize_;
	int qcommonBytes = qlim_ * 0.96 * mean_pktsize_;

	// 1<=queue_num_<=MAX_QUEUE_NUM
	if (queue_num_ != max(min(queue_num_,MAX_QUEUE_NUM),1)) {
	  fprintf(stderr, "Error!! queue_num_ must be between 1 and %d\n", MAX_QUEUE_NUM);
	  exit(1);
	}
	//queue length exceeds the queue limit
	if (prio == 0) {
	  // pri 0 can use common buffer if needed
	  if(TotalByteLength()+hdr_cmn::access(p)->size()>qlimBytes)
	    {
	      drop(p);
	      return;
	    }
	} else {
	  // other pri cannot use any of pri 0's buffer
	  if((TotalByteLength()-QueueByteLength(0))
	     +hdr_cmn::access(p)->size()
	     >qcommonBytes)
	    {
	      drop(p);
	      return;
	    }
	}

	if(prio>=queue_num_) {
	  fprintf(stderr, "Error!! prio %d exceeds queue_num_ %d\n", prio, queue_num_);
	  //prio=queue_num_-1;
	  exit(1);
	}

	//Enqueue packet
	q_[prio]->enque(p);

    //Enqueue ECN marking: Per-queue or Per-port
    if((marking_scheme_==PER_QUEUE_ECN && q_[prio]->byteLength()>thresh_*mean_pktsize_)||
    (marking_scheme_==PER_PORT_ECN && TotalByteLength()>thresh_*mean_pktsize_))
    {
      fprintf(stderr, "Error!! Marking scheme enabled, should not be used for s-PERC..\n");
      // if (hf->ect()) //If this packet is ECN-capable
      // 	hf->ce()=1;
      exit(1);
    }
}

int Priority::QueueByteLength(int i)
{
  int bytelength=0;
  if (i>=0 and i < queue_num_) {
    bytelength+=q_[i]->byteLength();
  } else{			  
    fprintf(stderr, "Error!! requesting length of invalid queue %d\n", i);
    exit(1);
  }
  return bytelength;
}

Packet* Priority::deque()
{
    if(TotalByteLength()>0)
	{
        //high->low: 7->0
	    for(int i=queue_num_-1;i>=0;i--)
	    {
		    if(q_[i]->length()>0)
            {
			    Packet* p=q_[i]->deque();
		        return (p);
		    }
        }
    }

	return NULL;
}
