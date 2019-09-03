set next_fid 0

Class TCP_pair

TCP_pair instproc init {args} {
    $self instvar pair_id group_id id debug_mode rttimes
    $self instvar tcps tcpr;# Sender TCP,  Receiver TCP
    $self instvar flow_gen_when_start;
    global myAgent
    eval $self next $args
    
    $self set tcps [new $myAgent]  ;# Sender TCP
    $self set tcpr [new $myAgent]  ;# Receiver TCP
    $tcps set_callback $self
    #$tcpr set_callback $self
    $self set pair_id  0
    $self set group_id 0
    $self set id       0
    $self set debug_mode 0
    $self set rttimes 0
}

TCP_pair instproc setup {snode dnode} {
    #Directly connect agents to snode, dnode. For faster simulation.
    #puts "TCP_pair setup $snode $dnode"
    global ns link_rate
    $self instvar tcps tcpr;# Sender TCP,  Receiver TCP
    $self instvar san dan  ;# memorize dumbell node (to attach)
    $self instvar tbf;
    $self instvar qjump;

    $self set san $snode
    $self set dan $dnode
    $ns attach-agent $snode $tcps;
    $ns attach-agent $dnode $tcpr;
    $tcpr listen

#    $self set tbf [new TBF]
#    $ns attach-tbf-agent $snode $tcps $tbf

    $ns connect $tcps $tcpr
}

TCP_pair instproc set_fincallback { controller func} {
    $self instvar aggr_ctrl fin_cbfunc
    $self set aggr_ctrl  $controller
    $self set fin_cbfunc  $func
}

TCP_pair instproc set_startcallback { controller func} {
    $self instvar aggr_ctrl start_cbfunc
    $self set aggr_ctrl $controller
    $self set start_cbfunc $func
}

TCP_pair instproc setgid { gid } {
    $self instvar group_id
    $self set group_id $gid
}

TCP_pair instproc setpairid { pid } {
    $self instvar pair_id
    $self set pair_id $pid
}

TCP_pair instproc setfid { fid } {
    $self instvar tcps tcpr
    $self instvar id
    $self set id $fid
    $tcps set fid_ $fid;
    $tcpr set fid_ $fid;
}

TCP_pair instproc settbf { tbf } {
    global ns
    $self instvar tcps tcpr
    $self instvar san 
    $self instvar tbfs
    $self set tbfs $tbf
    $ns attach-tbf-agent $san $tcps $tbf
}

TCP_pair instproc start { nr_bytes } {
    global ns SIM_END flow_gen
    $self instvar tcps tcpr id group_id
    $self instvar start_time bytes
    $self instvar aggr_ctrl start_cbfunc
    $self instvar debug_mode
    $self instvar tbf;
    $self instvar qjump;
    $self instvar id;

    $self set start_time [$ns now] ;# memorize
    $self set bytes       $nr_bytes  ;# memorize
    $self set flow_num $flow_gen ; # memorize

    if {$flow_gen >= $SIM_END} {
	return
    }
    if {$start_time >= 0.2} {
	set flow_gen [expr $flow_gen + 1]
    }
    if { $debug_mode == 1 } {
	puts "stats: [$ns now] start grp $group_id fid $id $nr_bytes bytes"
    }
    if { [info exists aggr_ctrl] } {
	$aggr_ctrl $start_cbfunc
    }
    $tcpr set flow_remaining_ [expr $nr_bytes]
    $tcps set signal_on_empty_ TRUE
    puts "Starting flow: $flow_gen of size $nr_bytes $id"
    $tcps advance-bytes $nr_bytes
#    set test_cbr [new Application/Traffic/CBR]
#    $test_cbr attach-agent $tcps
#    $test_cbr set interval_ 0.0000015
#    $test_cbr set packetSize_ 1460
#    $test_cbr set maxpkts_ [expr $nr_bytes / 1460]
#    $ns at [expr [$ns now]] "$test_cbr start"
}

TCP_pair instproc warmup { nr_pkts } {
    global ns
    $self instvar tcps id group_id
    $self instvar debug_mode

    set pktsize [$tcps set packetSize_]
    if { $debug_mode == 1 } {
	puts "warm-up: [$ns now] start grp $group_id fid $id $nr_pkts pkts ($pktsize +40)"
    }
    $tcps advanceby $nr_pkts
}

TCP_pair instproc stop {} {
    $self instvar tcps tcpr

    $tcps reset
    $tcpr reset
}


TCP_pair instproc fin_notify {} {
    global ns persistent_connections
    $self instvar sn dn san dan rttimes
    $self instvar tcps tcpr
    $self instvar aggr_ctrl fin_cbfunc
    $self instvar pair_id
    $self instvar bytes
    $self instvar dt ct
    $self instvar bps
    $self flow_finished
    $self instvar flow_num start_time

    #Shuang
    set old_rttimes $rttimes
    $self set rttimes [$tcps set nrexmit_]
    #
    # Mohammad commenting these
    # for persistent connections
    if { $persistent_connections == 0 } {
	$tcps reset
	$tcpr reset
    }

    # changed Agent_Aggr_pair fin_notify
    # added fldur_data, removed ct flow_num
    # Agent_Aggr_pair instproc fin_notify { pid bytes fldur fldur_data bps rttimes start_time} {}

    if { [info exists aggr_ctrl] } {
	#$aggr_ctrl $fin_cbfunc $pair_id $bytes $dt $bps [expr $rttimes - $old_rttimes] $start_time $ct $flow_num
	$aggr_ctrl $fin_cbfunc $pair_id $bytes $dt $dt $bps [expr $rttimes - $old_rttimes] $start_time 0 0
    }
}

TCP_pair instproc flow_finished {} {
    global ns
    $self instvar start_time bytes id group_id
    $self instvar dt bps ct
    $self instvar debug_mode

    set ct [$ns now]
    #Shuang commenting these
#    puts "Flow times (start, end): ($start_time, $ct)"
    $self set dt  [expr $ct - $start_time]
    if { $dt == 0 } {
	puts "dt = 0"
	flush stdout
    }
    $self set bps [expr $bytes * 8.0 / $dt ]
    if { $debug_mode == 1 } {
	puts "stats: $ct fin grp $group_id fid $id fldur $dt sec $bps bps"
    }
}


Agent/TCP/FullTcp instproc set_callback {tcp_pair} {
    $self instvar ctrl
    $self set ctrl $tcp_pair
}

Agent/TCP/FullTcp instproc done_data {} {
    global ns sink
    $self instvar ctrl
    #puts "[$ns now] $self fin-ack received";
    if { [info exists ctrl] } {
	$ctrl fin_notify
    }
}
