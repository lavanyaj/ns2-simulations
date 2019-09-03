#include <string>
#include <sstream>

#include "connector.h" 
#include "packet.h"
#include "queue.h"
#include "qjump.h"

QJUMP_Value::QJUMP_Value(QJUMP *qjump): qjump_(qjump), bucket_size_(0), qjump_timer_(this) {
  packet_queue_ = new PacketQueue();
}

QJUMP_Value::~QJUMP_Value() {
  qjump_timer_.cancel();
  if (packet_queue_->length() != 0) {
    for (Packet *p = packet_queue_->head(); p != 0; p = p->next_) {
      Packet::free(p);
    }
  }
  delete packet_queue_;
}

void QJUMP::recv(Packet *p, Handler *) {
  hdr_ip* iph = hdr_ip::access(p);
  fid_qjump_itr it = qjumps_.find(iph->flowid());
  if (it == qjumps_.end()) {
    // Just send packets because the rate limiting is not active for this flow.
    target_->recv(p);
    return;
  }
  QJUMP_Value* qjump = it->second;
  iph->prio() = qjump->prio_;
  hdr_cmn *ch = hdr_cmn::access(p);
  int pktsize = ch->size() << 3;
  if (qjump->tokens_ >= pktsize) {
    target_->recv(p);
    qjump->tokens_ -= pktsize;
  } else {
    drop(p);
  }
}

void QJUMP_Value::timeout(int) {
  // Schedule a new tick.
  tokens_ = bucket_size_;
  qjump_timer_.resched(time_interval_);
}

int QJUMP::command(int argc, const char* const* argv) {
	if (argc == 7) {
		if (strcmp("activate-fid", argv[1]) == 0) {
			int fid = atoi(argv[2]);
			long long bucket;
			long long time_interval_ns;
			string text = argv[3];
			stringstream buffer_bucket(text);
			buffer_bucket >> bucket;
			text = argv[4];
			stringstream buffer_time_interval(text);
			buffer_time_interval >> time_interval_ns;
			int qlen = atoi(argv[5]);
			int prio = atoi(argv[6]);
			activate_fid(fid, bucket, time_interval_ns, qlen, prio);
			return TCL_OK;
		}
	}
	return Connector::command(argc, argv);
}

void QJUMP::activate_fid(int fid, long long bucket, long long time_interval_ns, int qlen, int prio) {
	fid_qjump_itr it = qjumps_.find(fid);
	if (it != qjumps_.end()) {
		QJUMP_Value* qjump_val = it->second;
		qjumps_.erase(it);
		delete qjump_val;
	}
	QJUMP_Value* qjump = new QJUMP_Value(this);
	qjump->flow_id_ = fid;
	qjump->bucket_size_ = bucket;
	qjump->tokens_ = bucket;
	qjump->time_interval_ = time_interval_ns / 1000000000.0;
	qjump->queue_len_ = qlen;
	qjump->prio_ = prio;
	qjumps_.insert(fid_qjump_val(fid, qjump));
	qjump->qjump_timer_.resched(qjump->time_interval_);
}

void QJUMP_Timer::expire(Event* /*e*/) {
	qjump_value_->timeout(0);
}

static class QJUMPClass : public TclClass {

public:
  QJUMPClass() : TclClass ("QJUMP") {
  }

  TclObject* create(int, const char*const*) {
    return (new QJUMP());
  }
}class_qjump;
