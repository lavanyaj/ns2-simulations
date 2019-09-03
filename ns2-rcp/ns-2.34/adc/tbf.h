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

#ifndef ns_tbf_h
#define ns_tbf_h

#include "connector.h"
#include "timer-handler.h"

#include <map>

using namespace std;

class TBF;
class TBF_Value;

class TBF_Timer : public TimerHandler {
public:
	TBF_Timer(TBF_Value *t) : TimerHandler() {
		tbf_value_ = t;
	}
	
protected:
	virtual void expire(Event *e);
	TBF_Value *tbf_value_;
};

class TBF_Value {
public:
	TBF_Value(TBF *tbf);
	~TBF_Value();
	void timeout(int);

	double getupdatedtokens();
	int flow_id_;
	long long tokens_;
	long long rate_;
	long long bucket_;
	int qlen_;
	double lastupdatetime_;
	PacketQueue *q_;
	TBF_Timer tbf_timer_;
	int init_;
	int prio_;
	TBF *tbf_;
};

typedef map<int, TBF_Value*> fid_tbf;
typedef fid_tbf::iterator fid_tbf_itr;
typedef fid_tbf::value_type fid_tbf_val;

class TBF : public Connector {
public:
	int command(int argc, const char*const* argv);
protected:
	void recv(Packet *, Handler *);
	void activate_fid(int fid, long long rate, long long bucket, int qlen, int prio);

	fid_tbf tbfs_;
};

#endif
