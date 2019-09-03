# SPERC pair's have 
# - group_id = "src->dst"
# - pair_id = index of connection among the group
# - fid = unique flow identifier for this connection (group_id, pair_id)
set next_fid 0

Class SPERC_pair
#Variables:
#tcps tcpr:  Sender SPERC, Receiver SPERC 
#sn   dn  :  source/dest node which SPERC sender/receiver exist
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


SPERC_pair instproc init {args} {
    global ns
    $self instvar pair_id group_id id debug_mode rttimes
    $self instvar spercs spercr;# Sender SPERC,  Receiver SPERC
    $self instvar flow_num;

    eval $self next $args

    $self set spercs [new Agent/SPERC]  ;# Sender SPERC
    $self set spercr [new Agent/SPERC]  ;# Receiver SPERC

    $spercs set_callback $self
    $spercr set_callback $self    
    
    $self set pair_id  0
    $self set group_id 0
    $self set id       0
    $self set debug_mode 1
    $self set rttimes 0 ; # not used
    $self set flow_num 0 ;
}

SPERC_pair instproc setup {snode dnode} {
#   Directly connect agents to snode, dnode.
#   For faster simulation.
#   ignore arg_enable_qjump
    global ns link_rate
    $self instvar spercs spercr;# Sender SPERC,  Receiver SPERC
    $self instvar san dan  ;# memorize dumbell node (to attach)
    $self instvar is_active ;
    $self set is_active 0;
    $self set san $snode
    $self set dan $dnode

    $ns attach-agent $snode $spercs;
    $ns attach-agent $dnode $spercr;

    $spercs set nodeid_ [$snode id]
    $spercr set nodeid_ [$dnode id]

    $ns connect $spercs $spercr
}

SPERC_pair instproc set_fincallback { controller func} {
    $self instvar aggr_ctrl fin_cbfunc
    $self set aggr_ctrl  $controller
    $self set fin_cbfunc  $func
}

SPERC_pair instproc set_startcallback { controller func} {
    $self instvar aggr_ctrl start_cbfunc
    $self set aggr_ctrl $controller
    $self set start_cbfunc $func
}

SPERC_pair instproc setgid { gid } {
    $self instvar group_id
    $self set group_id $gid
}

SPERC_pair instproc setpairid { pid } {
    $self instvar pair_id
    $self set pair_id $pid
}

SPERC_pair instproc setfid { fid } {
    $self instvar spercs spercr
    $self instvar id
    $self set id $fid
    $spercs set fid_ $fid;
    $spercr set fid_ $fid;
}

# settbf never called
SPERC_pair instproc start { nr_bytes } {
    global alllog
    global ns SIM_END flow_gen SPERC_DATA_PKT_HDR_BYTES
    $self instvar spercs spercr id group_id pair_id
    $self instvar start_time bytes numpkts
    $self instvar aggr_ctrl start_cbfunc
    $self instvar debug_mode
    $self instvar syn_sent_t ctrl_syn_sent_t begin_datasend_t finish_datasend_t ctrl_fin_sent_src_t ctrl_fin_sent_rcvr_t
    $self instvar all_datapkts_received_t stop_data_t stop_ctrl_t done_t
    $self instvar bytes_scheduled bytes_sent
    $self instvar is_active;
    $self set is_active 1;

    $self set syn_sent_t 0;
    $self set ctrl_syn_sent_t 0;

    $self set begin_datasend_t 0;
    $self set finish_datasend_t 0;
    $self set all_datapkts_received_t 0;
    $self set stop_data_t 0;
    $self set ctrl_fin_sent_src_t 0;
    $self set ctrl_fin_sent_rcvr_t 0;

    $self set stop_ctrl_t 0;
    $self set done_t 0;
    $self set bytes_scheduled 0;
    $self set bytes_sent 0;

    $self set start_time [$ns now] ;# memorize
    $self set bytes       $nr_bytes  ;# memorize
    # set flow_num after we've incremented flow_gen
    set pktsize [$spercs set PKTSIZE_]
    # keep this in sync with ns-2.34/sperc/sperc-hdrs.h    
    # SPERC_CMN_HEADER is 72B. data packets have this in addtion to IP/Common
    # set sperc_hdr_bytes 40; #228 ; # 128 + 28 + 72
    # SPERC_CTRL_PKT_HDR is 64B. ctrl packet have this in addition to IP/Common/SPERC_CMN
    # so sperc_ctrl_pkt has 292B!!
    # if we optimized, ethernet/IP/SPERC_CTRL = 104B 10 hops
    $self set numpkts     [expr {ceil($nr_bytes/($pktsize - $SPERC_DATA_PKT_HDR_BYTES))}]
    if {$flow_gen >= $SIM_END} {
	puts $alllog "start: [$ns now] start group_id $group_id pair_id $pair_id fid $id $numpkts pkts, but flow_Gen $flow_gen >= SIM_END $SIM_END, ignore and return"
	return
    }
    if {$start_time >= 0.2} {
	puts $alllog "$start_time >= 0.2, set flow_gen to [expr $flow_gen + 1]"
	set flow_gen [expr $flow_gen + 1]
    }
    $self set flow_num $flow_gen ; #memorize    
    $spercr set numpkts_ 0
    $spercs set numpkts_ $numpkts
    # can we set fid_ here to say flowgen_ ??
    # other variables that are bound on RCPAgent()
    # seqno_, packetSize_, numpkts_, nodeid_, init_refintv_fix_ (not used) fid_
    $spercs set fid_ $flow_gen
    $spercr set fid_ $flow_gen
    $self set id $flow_gen

    if { $debug_mode == 1 } {
	set st [$ns now]
	puts $alllog "stats: [$ns now] start $st group_id $group_id pair_id $pair_id fid $id nr_bytes $nr_bytes numpkts $numpkts pktsize $pktsize hdr $SPERC_DATA_PKT_HDR_BYTES"
    }
    if { [info exists aggr_ctrl] } {
	$aggr_ctrl $start_cbfunc
    }

    puts $alllog "SPERC_pair(group_id $group_id pair_id $pair_id) start: nr_bytes $nr_bytes set fid_ at spercs, spercr to flow_gen $flow_gen = [$spercs set fid_], set numpkts_ to $numpkts = [$spercs set numpkts_] and [$spercr set numpkts_] at [$ns now]"

    $spercs sendfile
}

SPERC_pair instproc warmup { nr_pkts } {
    global alllog
    global ns
    $self instvar spercs id group_id pair_id
    $self instvar debug_mode

    set pktsize [$spercs set $PKTSIZE_]
    if { $debug_mode == 1 } {
	    puts $alllog "SPERC_pair(group_id $group_id pair_id $pair_id) warm-up: [$ns now] start grp $group_id fid $id $nr_pkts pkts ignore"	
    }
}

SPERC_pair instproc stop {} {
    global alllog
    global ns
    $self instvar id group_id pair_id debug_mode

    if { $debug_mode == 1 } {
	puts $alllog "[$ns now] SPERC_pair(group_id $group_id pair_id $pair_id) stop calling spercs stop"
    }
    $self instvar spercs
    $spercs stop

#    $self instvar  tracefile
#    close $tracefile
}

SPERC_pair instproc fin_notify {} {
    global alllog
    global ns
    $self instvar sn dn san dan rttimes
    $self instvar spercs spercr
    $self instvar aggr_ctrl fin_cbfunc
    $self instvar pair_id group_id debug_mode id
    $self instvar bytes
    $self instvar dt dt_stop_data
    $self instvar bps
    $self instvar bytes_scheduled
    $self instvar bytes_sent

    $self flow_finished
    $self instvar start_time

    if { $debug_mode == 1 } {
	puts $alllog "[$ns now] SPERC_pair(group_id $group_id pair_id $pair_id) fin_notify()."
    }



    #Shuang
    #set old_rttimes $rttimes
    #$self set rttimes [$spercs set nrexmit_]
    #
    # lavanya: not commenting these
    # since SPERC flows start from scratch for each sendfile
    # fid_ size_ etc. from tcl, all others reset

    if { [info exists aggr_ctrl] } {
	$aggr_ctrl $fin_cbfunc $pair_id $bytes $dt $dt_stop_data $bps $rttimes $start_time $bytes_scheduled $bytes_sent
    } else {
    }

    if { $debug_mode == 1 } {
	puts $alllog "[$ns now] SPERC_pair(group_id $group_id pair_id $pair_id) calling spercs reset, spercr reset, setting is_active to 0."
    }

    $spercs reset
    $spercr reset

    $self instvar is_active;
    $self set is_active 0;

    set id 0
}

SPERC_pair instproc flow_finished {} {
    global alllog
    global ns
    $self instvar start_time bytes id group_id pair_id
    $self instvar dt bps
    $self instvar debug_mode
    $self instvar numpkts
    $self instvar rttimes
    $self instvar syn_sent_t ctrl_syn_sent_t begin_datasend_t finish_datasend_t ctrl_fin_sent_src_t ctrl_fin_sent_rcvr_t
    $self instvar all_datapkts_received_t stop_data_t stop_ctrl_t done_t
    $self instvar dt_all_datapkts_received dt_stop_data
    $self instvar bps_stop_data

    if { $debug_mode == 1 } {
	puts $alllog "[$ns now] SPERC_pair(group_id $group_id pair_id $pair_id) flow_finished"
    }

    set ct [$ns now]
    #Shuang commenting these
#    puts $alllog "Flow times (start, end): ($start_time, $ct)"
    $self set dt  [expr $ct - $start_time]
    if { $dt == 0 } {
	puts $alllog "dt = 0"
	flush stdout
    }

    if { $all_datapkts_received_t == 0 } {
	$self set dt_all_datapkts_received 0
    } else {
	$self set dt_all_datapkts_received [expr $all_datapkts_received_t - $start_time]
    }


    if { $stop_data_t == 0 } {
	$self set dt_stop_data 0
	$self set bps_stop_data 0
    } else {
	$self set dt_stop_data [expr $stop_data_t - $start_time]
	if { $dt_stop_data == 0 } {
	    puts $alllog "dt_stop_data = 0"
	    flush stdout
	} else {
	    $self set bps_stop_data [expr $bytes * 8.0 / $dt_stop_data]
	}
    }

    $self set bps [expr $bytes * 8.0 / $dt ]

    if { $debug_mode == 1 } {
	puts $alllog "stats: $ct fin group_id $group_id start_time $start_time pair_id $pair_id fid $id fldur $dt sec bytes $bytes $bps bps numpkts $numpkts fldur_data_recv $dt_all_datapkts_received fldur_data_ack $dt_stop_data bps_stop_data $bps_stop_data rttimes $rttimes. absolute times: syn_sent_t $syn_sent_t ctrl_syn_sent_t $ctrl_syn_sent_t  begin_datasend_t $begin_datasend_t all_datapkts_received_t $all_datapkts_received_t finish_datasend_t $finish_datasend_t stop_data_t $stop_data_t ctrl_fin_sent_src_t $ctrl_fin_sent_src_t ctrl_fin_sent_rcvr_t $ctrl_fin_sent_rcvr_t stop_ctrl_t $stop_ctrl_t"
    }
}

Agent/SPERC instproc set_callback {sperc_pair} {
    global alllog
    global ns
    $self instvar ctrl   
    $self set ctrl $sperc_pair
    puts $alllog "[$ns now] Agent/SPERC set_callback agent/sperc setting ctrl to SPERCpair $sperc_pair- ctrl is [$self set ctrl]"
}

Agent/SPERC instproc done {bytes_scheduled bytes_sent} {
    global alllog
    global ns sink
    $self instvar ctrl

    if { [info exists ctrl] } {
	puts $alllog "[$ns now] Agent/SPERC done fid_ [$self set fid_] $self nodeid_ [$self set nodeid_] ctrl $ctrl (group_id [$ctrl set group_id] pair_id [$ctrl set pair_id]): calls ctrl fin_notify";
	$ctrl set done_t [$ns now]
	$ctrl set bytes_scheduled $bytes_scheduled
	$ctrl set bytes_sent $bytes_sent

	$ctrl fin_notify
    } else {
	puts $alllog "[$ns now] Agent/SPERC done fid_ [$self set fid_] $self nodeid_ [$self set nodeid_] : no ctrl exists";
    }
}

#SPERC_pair instproc set-tracefile { tracefile_ } {
#    $self instvar spercs
#   tracefile
#    $self set tracefile $tracefile_
#    puts $alllog "attaching $tracefile_ tp sperc source"
#    $spercs attach $tracefile_
#}

# for debugging

Agent/SPERC instproc syn-sent {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	puts $alllog "[$ns now] Agent/SPERC syn-sent fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
	$ctrl set syn_sent_t [$ns now]
    } else {
	puts $alllog "[$ns now] Agent/SPERC syn-sent fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc ctrl-syn-sent {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	$ctrl set ctrl_syn_sent_t [$ns now]
	puts $alllog "[$ns now] Agent/SPERC ctrl-syn-sent fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC ctrl-syn-sent fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc ctrl-fin-sent-at-src {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	$ctrl set ctrl_fin_sent_src_t [$ns now]
	puts $alllog "[$ns now] Agent/SPERC ctrl-fin-sent-at-src fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC ctrl-fin-sent-at-src fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc ctrl-fin-sent-at-rcvr {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	$ctrl set ctrl_fin_sent_rcvr_t [$ns now]
	puts $alllog "[$ns now] Agent/SPERC ctrl-fin-sent-at-rcvr fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC ctrl-fin-sent-at-rcvr fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}



Agent/SPERC instproc begin-datasend {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	$ctrl set begin_datasend_t [$ns now]
	puts $alllog "[$ns now] Agent/SPERC begin-datasend fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC begin-datasend fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc send-ctrl-fin-set {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	puts $alllog "[$ns now] Agent/SPERC send-ctrl-fin-set fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC send-ctrl-fin-set fid_ [$self set fid_] nodeid_ [$self set nodeid_]";
    }
}

Agent/SPERC instproc stop-request-set {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	puts $alllog "[$ns now] Agent/SPERC stop-request-set fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";    
    } else {
	puts $alllog "[$ns now] Agent/SPERC stop-request-set fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc finish-datasend {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	$ctrl set finish_datasend_t [$ns now]
	puts $alllog "[$ns now] Agent/SPERC finish-datasend fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC finish-datasend fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc all-datapkts-received {} {
    global alllog
    global ns
    $self instvar ctrl 
    if { [info exists ctrl] } {
	$ctrl set all_datapkts_received_t [$ns now]
	puts $alllog "[$ns now] Agent/SPERC all-datapkts-received fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC all-datapkts-received fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc reset-called {} {
    global alllog
    global ns
    $self instvar ctrl

    if { [info exists ctrl] } {
	puts $alllog "[$ns now] Agent/SPERC reset-called fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";

    } else {
	puts $alllog "[$ns now] Agent/ SPERC reset-called: [$ns now] $self fid_ [$self set fid_] nodeid_ [$self set nodeid_]: no ctrl exists"
    }
}


Agent/SPERC instproc stop-data {num_enter_retx} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	$ctrl set stop_data_t [$ns now]
	$ctrl set rttimes $num_enter_retx
	puts $alllog "[$ns now] Agent/SPERC stop-data fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_] num_enter_retx $num_enter_retx";
    } else {
	puts $alllog "[$ns now] Agent/SPERC stop-data fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }
}

Agent/SPERC instproc stop-ctrl {} {
    global alllog
    global ns
    $self instvar ctrl
    if { [info exists ctrl] } {
	$ctrl set stop_ctrl_t [$ns now]
	puts $alllog "[$ns now] Agent/SPERC stop-ctrl fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_]";
    } else {
	puts $alllog "[$ns now] Agent/SPERC stop-ctrl fid_ [$self set fid_]  nodeid_ [$self set nodeid_]";    
    }

    #puts $alllog "[$ns now] Agent/ SPERC stop-ctrl: $self fid_ [$self set fid_] group_id [$ctrl set group_id] pair_id [$ctrl set pair_id] nodeid_ [$self set nodeid_] nodeid_ [$self set nodeid_] ctrl $ctrl (group_id [$ctrl set group_id] pair_id [$ctrl set pair_id])";

}
