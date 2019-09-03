# RCP pair's have 
# - group_id = "src->dst"
# - pair_id = index of connection among the group
# - fid = unique flow identifier for this connection (group_id, pair_id)
set next_fid 0

Class RCP_pair
#Variables:
#tcps tcpr:  Sender RCP, Receiver RCP 
#sn   dn  :  source/dest node which RCP sender/receiver exist
#:  (only for setup_wnode)
#delay    :  delay between sn and san (dn and dan)
#:  (only for setup_wnode)
#san  dan :  nodes to which sn/dn are attached   
#aggr_ctrl:  Agent_Aggr_pair for callback
#start_cbfunc:  callback at start
#fin_cbfunc:  callback at start
#group_id :  group id
#pair_id  :  group id
#id       :  flow id
#Public Functions:
#setup{snode dnode enable_qjump}       <- either of them
#setgid {gid}             <- if applicable (default 0)
#setpairid {pid}          <- if applicable (default 0)
#setfid {fid}             <- if applicable (default 0)
#start { nr_bytes } ;# let start sending nr_bytes 
#set_fincallback { controller func} #;
#set_startcallback { controller func} #;
#fin_notify {}  #; Callback- called by agent when it finished
# warmup { nr_bytes } ;
#Private Function
#flow_finished {} {


RCP_pair instproc init {args} {
    global ns alllog
    $self instvar pair_id group_id id debug_mode rttimes
    $self instvar rcps rcpr;# Sender RCP,  Receiver RCP
    $self instvar flow_num
    eval $self next $args

    $self set rcps [new Agent/RCP]  ;# Sender RCP
    $self set rcpr [new Agent/RCP]  ;# Receiver RCP

    $rcps set_callback $self
    $rcpr set_callback $self    
    
    $self set pair_id  0
    $self set group_id 0
    $self set id       -1
    $self set debug_mode 1
    $self set rttimes 0 ; # not used
    $self set flow_num -1;
}

RCP_pair instproc setup {snode dnode} {
#   Directly connect agents to snode, dnode.
#   For faster simulation.
#   ignore arg_enable_qjump
    global ns link_rate alllog
    $self instvar rcps rcpr;# Sender RCP,  Receiver RCP
    $self instvar san dan  ;# memorize dumbell node (to attach)
    $self instvar is_active ;
    $self set is_active 0;
    $self set san $snode
    $self set dan $dnode

    $ns attach-agent $snode $rcps;
    $ns attach-agent $dnode $rcpr;

    # lavanya: added nodeid_ to Agent/RCP
    $rcps set nodeid_ [$snode id]
    $rcps set tonodeid_ [$snode id]
    $rcps set fromnodeid_ [$snode id]

    $rcpr set nodeid_ [$dnode id]
    $rcpr set tonodeid_ [$dnode id]
    $rcpr set fromnodeid_ [$dnode id]

    $ns connect $rcps $rcpr
}

RCP_pair instproc set_fincallback { controller func} {
    $self instvar aggr_ctrl fin_cbfunc
    $self set aggr_ctrl  $controller
    $self set fin_cbfunc  $func
}

RCP_pair instproc set_startcallback { controller func} {
    $self instvar aggr_ctrl start_cbfunc
    $self set aggr_ctrl $controller
    $self set start_cbfunc $func
}

RCP_pair instproc setgid { gid } {
    $self instvar group_id alllog
    $self set group_id $gid
}

RCP_pair instproc setpairid { pid } {
    $self instvar pair_id 
    $self set pair_id $pid
}

RCP_pair instproc setfid { fid } {
    $self instvar rcps rcpr
    $self instvar id
    $self set id $fid
    $rcps set fid_ 0; #$fid;
    $rcpr set fid_ 0; #$fid;
}

# settbf never called
RCP_pair instproc start { nr_bytes } {
    global ns SIM_END flow_gen alllog
    $self instvar rcps rcpr id group_id pair_id
    $self instvar start_time bytes numpkts
    $self instvar aggr_ctrl start_cbfunc
    $self instvar debug_mode
    $self instvar flow_num
    $self instvar is_active;
    $self set is_active 1;
    $self set start_time [$ns now] ;# memorize
    $self set bytes       $nr_bytes  ;# memorize
    # set flow_num and id after we've incremented flow_gen

    set pktsize [$rcps set packetSize_]
    
    # keep this in sync with ns-2.34/rcp/rcp-hdrs.h    
    # RCP_CMN_HEADER is 72B. data packets have this in addtion to IP/Common
    #set rcp_hdr_bytes 40; #228 ; # 128 + 28 + 72
    # RCP_CTRL_PKT_HDR is 64B. ctrl packet have this in addition to IP/Common/RCP_CMN
    # so rcp_ctrl_pkt has 292B!!
    # if we optimized, ethernet/IP/RCP_CTRL = 104B 10 hops
    set rcp_hdr_bytes [$rcps set rcp_hdr_bytes_]
    $self set numpkts     [expr {ceil($nr_bytes/($pktsize - $rcp_hdr_bytes))}]
    if {$flow_gen >= $SIM_END} {
	puts $alllog "start: [$ns now] start group_id $group_id pair_id $pair_id fid $id $numpkts pkts, but flow_Gen $flow_gen >= SIM_END $SIM_END, ignore and return"
	return
    }
    if {$start_time >= 0.2} {
	puts $alllog "$start_time >= 0.2, set flow_gen to [expr $flow_gen + 1]"
	set flow_gen [expr $flow_gen + 1]
    }
    $self set flow_num $flow_gen ;#memorize
    puts $alllog "Starting flow: $flow_gen of size $nr_bytes bytes or $numpkts pkts $id"
    $rcps set numpkts_ $numpkts    
    $rcpr set numpkts_ 0
    # can we set fid_ here to say flowgen_ ??
    # other variables that are bound on RCPAgent()
    # seqno_, packetSize_, numpkts_, nodeid_, init_refintv_fix_ (not used) fid_
    $rcps set fid_ $flow_gen
    $rcpr set fid_ $flow_gen
    $self set id $flow_gen

    if { $debug_mode == 1 } {
	set st [$ns now]
	puts $alllog "stats: [$ns now] start $st group_id $group_id pair_id $pair_id fid $id nr_bytes $nr_bytes numpkts $numpkts pktsize $pktsize hdr $rcp_hdr_bytes"
    }
    if { [info exists aggr_ctrl] } {
	$aggr_ctrl $start_cbfunc
    }

    puts $alllog "RCP_pair(group_id $group_id pair_id $pair_id) start nr_bytes $nr_bytes set fid_ at rcps, rcpr to flow_gen $flow_gen = [$rcps set fid_], set numpkts_ to $numpkts = [$rcps set numpkts_] and [$rcpr set numpkts_] at [$ns now]"


    $rcps sendfile
}

RCP_pair instproc warmup { nr_pkts } {
    global ns alllog
    $self instvar rcps id group_id
    $self instvar debug_mode

    set pktsize [$rcps set packetSize_]
    if { $debug_mode == 1 } {
	    puts $alllog "warm-up: [$ns now] start grp $group_id fid $id $nr_pkts pkts ignore"	
    }
}

RCP_pair instproc stop {} {
    # how to stop cleanly?

    $self instvar rcps
    $rcps stop

    #$self fin_notify
}


RCP_pair instproc fin_notify {} {
    global ns alllog
    $self instvar sn dn san dan rttimes
    $self instvar rcps rcpr
    $self instvar aggr_ctrl fin_cbfunc
    $self instvar pair_id
    $self instvar bytes
    $self instvar dt
    $self instvar bps
    $self flow_finished
    $self instvar start_time
    $self instvar id

    puts $alllog "[$ns now] RCP_pair $pair_id, id $id, src flow id [$rcps set fid_], dst flow id [$rcpr set fid_]  fin_notify. resetting rcps and rcpr. then call agent_aggr_pair fin_notify callback.\n"
    #Shuang
    #set old_rttimes $rttimes
    #$self set rttimes [$rcps set nrexmit_]
    #
    # not commenting these
    # since RCP flows start from scratch for each new sendfile
    # 
    if { [info exists aggr_ctrl] } {
	$aggr_ctrl $fin_cbfunc $pair_id $bytes $dt $dt $bps 0 $start_time 0 0
    } else {
	puts $alllog " [$ns now]  rcp_pair fin_notify was called, but no agg_ctrl or fin_cbfunc exists\n";
    }
    $rcps reset
    $rcpr reset

    $self instvar is_active;
    $self set is_active 0;
}

RCP_pair instproc flow_finished {} {
    global ns alllog
    $self instvar start_time bytes id group_id pair_id
    $self instvar dt bps
    $self instvar debug_mode
    $self instvar numpkts

    set ct [$ns now]
    #Shuang commenting these
#    puts "Flow times (start, end): ($start_time, $ct)"
    $self set dt  [expr $ct - $start_time]
    if { $dt == 0 } {
	puts $alllog "dt = 0"
	flush stdout
    }
    $self set bps [expr $bytes * 8.0 / $dt ]
    if { $debug_mode == 1 } {
	puts $alllog "stats: $ct fin group_id $group_id pair_id $pair_id fid $id fldur $dt sec bytes $bytes $bps bps numpkts $numpkts"
    }
}

Agent/RCP instproc set_callback {rcp_pair} {
    global ns
    $self instvar ctrl   
    $self set ctrl $rcp_pair
    #puts "[$ns now] set_callback agent/rcp setting ctrl to RCPpair $rcp_pair- ctrl is [$self set ctrl]"
}

Agent/RCP instproc done_stats {numPkts num_dataPkts_sent num_refPkts_sent num_enter_retx num_dataPkts_acked_by_receiver_} {
    global ns alllog ctlog
    $self instvar ctrl
    if { [info exists ctrl] } {
	puts $ctlog "[$ns now] $self fid_ [$self set fid_] agent/rcp done_stats [$ctrl set start_time] [$ctrl set numpkts] $numPkts $num_dataPkts_sent $num_refPkts_sent $num_enter_retx $num_dataPkts_acked_by_receiver_";

	puts $alllog "[$ns now] $self fid_ [$self set fid_] agent/rcp done_stats [$ctrl set start_time] [$ctrl set numpkts] $numPkts $num_dataPkts_sent $num_refPkts_sent $num_enter_retx $num_dataPkts_acked_by_receiver_";
    }  else {
	puts $alllog " [$ns now] $self fid_ [$self set fid_] agent/rcp is done_stats, no ctrl exists\n";
    }
}


Agent/RCP instproc done {} {
    global ns sink alllog
    $self instvar ctrl

    puts $alllog "[$ns now] $self fid_ [$self set fid_] agent/rcp done";

    if { [info exists ctrl] } {
	puts $alllog " [$ns now] $self fid_ [$self set fid_] agent/rcp calling ctrl fin_notify\n";
	$ctrl fin_notify
    } else {
	puts $alllog " [$ns now] $self fid_ [$self set fid_] agent/rcp is done, no ctrl exists\n";

    }
}

#RCP_pair instproc set-tracefile { tracefile_ } {
#    $self instvar rcps
#   tracefile
#    $self set tracefile $tracefile_
#    puts "attaching $tracefile_ tp rcp source"
#    $rcps attach $tracefile_
#}

# for debugging

Agent/RCP instproc begin-datasend {} {
    global ns alllog
#    $self instvar sstart
#    $self set sstart [$ns now]
    puts $alllog "[$ns now] $self fid_ [$self set fid_] begin-datasend";
}
Agent/RCP instproc finish-datasend {} {
    global ns alllog
    puts $alllog "[$ns now] $self fid_ [$self set fid_] finish-datasend";
}

Agent/RCP instproc syn-sent {} {
    global ns alllog
    puts $alllog "[$ns now] $self fid_ [$self set fid_] syn-sent";
}

Agent/RCP instproc fin-received {} {
    global ns alllog
    $self instvar ctrl
    puts $alllog "[$ns now] $self fid_ [$self set fid_] fin-received";
}

Agent/RCP instproc reset-tcl {} {
    global ns alllog
    $self instvar ctrl
    if { [info exists ctrl] } {
	puts $alllog "Agent/ RCP reset-tcl: [$ns now] $self fid_ [$self set fid_] $self nodeid_ [$self set nodeid_] ctrl group_id [$ctrl set group_id]";
    } else {
	puts "Agent/ RCP reset-tcl but no ctrl: [$ns now] $self fid_ [$self set fid_] $self nodeid_ [$self set nodeid_]"
    }
}
