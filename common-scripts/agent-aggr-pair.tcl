
Class Agent_Aggr_pair

Agent_Aggr_pair instproc init {args} {
    eval $self next $args
}

Agent_Aggr_pair instproc attach-logfile { logf } {
#Public 
    $self instvar logfile
    $self set logfile $logf
}

Agent_Aggr_pair instproc attach-tracefile {tracef} {
    $self instvar nr_pairs  ;# nr of pairs in this group (given)
    $self instvar apair
    $self instvar tracefile
    $self instvar apair_type
    $self set tracefile $tracef
    for {set i 0} {$i < $nr_pairs} {incr i} {
	if {$tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {
		[$apair($i) set spercs] attach $tracef
	    }
	    if { $apair_type == "RCP_pair"} {
		[$apair($i) set rcps] attach $tracef
	    }
	}
    }
}

Agent_Aggr_pair instproc attach-common-tracefile {common_tracef} {
    $self instvar nr_pairs  ;# nr of pairs in this group (given)
    $self instvar apair
    $self instvar common_tracefile
    $self instvar apair_type
    $self set common_tracefile $common_tracef
    for {set i 0} {$i < $nr_pairs} {incr i} {
	if {$common_tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {
		[$apair($i) set spercs] attach-common $common_tracef
	    }
	    #if { $apair_type == "RCP_pair"} {
	    #		[$apair($i) set rcps] attach $tracef
	    #	    }
	}
    }
}

Agent_Aggr_pair instproc setup {snode dnode tbflist tbfindex gid nr init_fid agent_pair_type tracef common_tracef exp} {
    global alllog
    #Public
    #Note:
    #Create nr pairs of Agent_pair and connect them to snode-dnode bottleneck.
    #We may refer this pair by group_id gid. All Agent_pairs have the same gid,
    #and each of them has its own flow id: init_fid + [0 .. nr-1] global next_fid
    $self instvar apair     ;# array of Agent_pair
    $self instvar group_id  ;# group id of this group (given)
    $self instvar nr_pairs  ;# nr of pairs in this group (given)
    $self instvar s_node d_node apair_type ;
    $self instvar tracefile
    $self set tracefile $tracef
    $self instvar common_tracefile
    $self set common_tracefile $common_tracef
    $self instvar experiment
    $self set experiment $exp

    $self set group_id $gid 
    $self set nr_pairs $nr
    $self set s_node $snode
    $self set d_node $dnode
    $self set apair_type $agent_pair_type

    #puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) setup"
    puts $alllog "set up $nr pairs in this group $gid for experiment $experiment (Agent_Aggr_pair)"

    array set tbf $tbflist
    set arrsize [array size tbf]
    
    for {set i 0} {$i < $nr_pairs} {incr i} {
 	$self set apair($i) [new $apair_type]
	$apair($i) setup $snode $dnode
	$apair($i) setgid $group_id  ;# let each pair know our group id
	$apair($i) setpairid $i      ;# let each pair know his pair id

	# lavanya: fid_ actually reset on start

	$apair($i) setfid $init_fid  ;# Mohammad: assign next fid

	if {$tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {
		[$apair($i) set spercs] attach $tracef
	    }
	    if { $apair_type == "RCP_pair"} {
		[$apair($i) set rcps] attach $tracef
	    }
	}

	if {$common_tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {
		[$apair($i) set spercs] attach-common $common_tracef
	    }
	    # if { $apair_type == "RCP_pair"} {
	    # 	[$apair($i) set rcps] attach $common_tracef
	    # }
	}
	
	if {$arrsize != 0} {
	    # Mohammad: install TBF for this pair
	    puts $alllog "installing tbf $tbfindex for gid $group_id fid $init_fid"
	    $apair($i) settbf $tbf($snode,$tbfindex)
	    #FIXME: needs to assign proper tbf
	}
	incr init_fid
    }
    $self resetvars                  ;# other initialization
}

Agent_Aggr_pair instproc resetvars {} {
    global alllog
    #   $self instvar fid             ;# current flow id of this group
    $self instvar tnext ;# last flow arrival time
    $self instvar actfl             ;# nr of current active flow
   
    $self instvar group_id
    $self instvar nr_pairs
    #puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) reset_vars"

    $self set tnext 0.0
    #    $self set fid 0 ;#  flow id starts from 0
    $self set actfl 0
    # puts "reset active flows to $actfl"
}

set warmupRNG [new RNG]
$warmupRNG seed 5251

Agent_Aggr_pair instproc warmup {jitter_period npkts} {
    global alllog
    global ns warmupRNG
    $self instvar nr_pairs apair group_id
   
    puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) warmup"
    for {set i 0} {$i < $nr_pairs} {incr i} {
	$ns at [expr [$ns now] + [$warmupRNG uniform 0.0 $jitter_period]] "$apair($i) warmup $npkts"
    }
}


Agent_Aggr_pair instproc ct_stop {fid} {
    global alllog
    $self instvar nr_pairs apair
    $self instvar apair_type s_node d_node group_id
    $self instvar tnext
    $self instvar tracefile
    $self instvar common_tracefile
    $self instvar fid_to_pid

    if { [info exists fid_to_pid($fid)] } {
	set pid_ $fid_to_pid($fid)
	if { [$apair($pid_) set is_active] > 0 } {
	    if { [$apair($pid_) set id] == $fid } {
		puts $alllog "stopping pair $pid_ fid $fid"
		$apair($pid_) stop
	    } else {
		puts $alllog "error! can't stop pair $pid_, it doesn't have fid $fid"		
	    }
	} else {
		puts $alllog "error! can't stop pair $pid_, it is not active"		
	}
    } else {
		puts $alllog "error! can't stop fid $fid, no mapping from fid to pair id"		
    }
}

Agent_Aggr_pair instproc ct_start {fid num_bytes} {
    global alllog ns
    $self instvar nr_pairs apair
    $self instvar apair_type s_node d_node group_id
    $self instvar tnext
    $self instvar tracefile
    $self instvar common_tracefile
    $self instvar fid_to_pid

    puts $alllog "[$ns now]: ct_start $s_node -> $d_node"

    set next_inactive_pair -1
    for {set i 0} {$i < $nr_pairs} {incr i} {	
	if { [$apair($i) set is_active] == 0 && $next_inactive_pair == -1} {
	    set next_inactive_pair $i
	}
    }

    if { $next_inactive_pair < [expr 0] } { # create new pair
	puts $alllog "[$ns now]: creating new connection $nr_pairs $s_node -> $d_node"
	# lavanya: things that SPERC_pair should support
	$self set apair($nr_pairs) [new $apair_type]
	$apair($nr_pairs) setup $s_node $d_node
	$apair($nr_pairs) setgid $group_id ;
	$apair($nr_pairs) setpairid $nr_pairs ;
	$apair($nr_pairs) setfid 0  ; # we reset this later when we call start

	if {$tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {		
		[$apair($nr_pairs) set spercs] attach $tracefile
	    }
	    if { $apair_type == "RCP_pair"} {
		puts "attach tracefile to RCP source for flow $fid ([[$apair($nr_pairs) set rcps] set fid_])"
		[$apair($nr_pairs) set rcps] attach $tracefile
	    }
	}

	if {$common_tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {
		[$apair($nr_pairs) set spercs] attach-common $common_tracefile
	    }
	    # if { $apair_type == "RCP_pair"} {
	    # 	[$apair($nr_pairs) set rcps] attach $common_tracefile
	    # }
	}

	set next_inactive_pair $nr_pairs

	#### Callback Setting #################
	$apair($nr_pairs) set_fincallback $self fin_notify
	$apair($nr_pairs) set_startcallback $self start_notify
	#######################################

	incr nr_pairs
    }


    # at this point, there is an inactive pair, we'll start fid using that pair
    $self set fid_to_pid($fid) $next_inactive_pair
    $apair($next_inactive_pair) start $num_bytes

    puts $alllog "[$ns now]: start $fid using inactive pair $next_inactive_pair, pair's fid now [$apair($next_inactive_pair) set id]"
    
    # start will increment flow_gen, set fid_ to flow_gen, see RCP_pair
}

Agent_Aggr_pair instproc init_schedule {} {
    global alllog
    #Public
    #Note:
    #Initially schedule flows for all pairs according to the arrival process.
    global ns
    $self instvar nr_pairs apair group_id
    
    #puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) init_schedule"
    # Mohammad: initializing last_arrival_time
    #$self instvar last_arrival_time
    #$self set last_arrival_time [$ns now]
    $self instvar tnext rv_flow_intval

    set dt [$rv_flow_intval value]

    $self set tnext [expr [$ns now] + $dt]
    
    for {set i 0} {$i < $nr_pairs} {incr i} {
	#### Callback Setting ########################
	$apair($i) set_fincallback $self   fin_notify
	$apair($i) set_startcallback $self start_notify
	###############################################	
	#puts $alllog "Agent_Aggr_Pair schedule group [$apair($i) set group_id] at $tnext"
	flush stdout
	$self schedule $i
    }
}

Agent_Aggr_pair instproc schedule { pid } {
    global alllog
    #Private
    #Schedule  pair (having pid) next flow time according to the flow arrival process.
    global ns flow_gen SIM_END
    $self instvar apair group_id nr_pairs
 #   $self instvar fid
    $self instvar tnext
    $self instvar rv_flow_intval rv_nbytes

    #puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) schedule pid=$pid, tnext $tnext"

    if {$flow_gen >= $SIM_END} {
	puts $alllog "Agent_Aggr_pair schedule: return since flow_gen $flow_gen >= SIM_END $SIM_END"
	return
    }  
 
    set t [$ns now]
    
    if { $t > $tnext } {
	# why does this mean not enough flows?
	puts $alllog "Error, Not enough flows ! Aborting! pair id $pid"
	flush stdout
	exit 
    }

    # Mohammad: persistent connection.. don't 
    # need to set fid each time
    #$apair($pid) setfid $fid
    # incr fid
    # lavanya: just noting this is where random file size is generated when schedule's called
    # rv_nbytes is the number of packets from the trace, if those captured packets
    # where 1500B comprising IP / TCP / Data with min size IP and TCP headers (20B each),
    # each packet had 1460 bytes of data. When we send on the wire, we add
    # for RCP, in addition to Data we have Common / IP / RCP
    # for SPERC, in addition to Data, we have Common / IP / SPERC_CMN
    set tmp_ [expr ceil ([$rv_nbytes value])]
    set tmp_ [expr $tmp_ * 1460]
    # lavanya for SPERC pair, start then calls sendfile
    $ns at $tnext "$apair($pid) start $tmp_"

    set dt [$rv_flow_intval value]
    $self set tnext [expr $tnext + $dt]
    $ns at [expr $tnext - 0.0000001] "$self check_if_behind"
}

Agent_Aggr_pair instproc check_if_behind {} {
    global alllog
    global ns
    global flow_gen SIM_END
    $self instvar apair
    $self instvar nr_pairs
    $self instvar apair_type s_node d_node group_id
    $self instvar tnext
    $self instvar tracefile
    $self instvar common_tracefile


    # create a new SPERC pair in group Agent_Aggr_pair for each new flow
    set t [$ns now]
    #puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) check_if_behind now $t"

    if { $flow_gen < $SIM_END && $tnext < [expr $t + 0.0000002] } { #create new flow
	puts $alllog "[$ns now]: creating new connection $nr_pairs $s_node -> $d_node"
	flush stdout
	# lavanya: things that SPERC_pair should support
	$self set apair($nr_pairs) [new $apair_type]
	$apair($nr_pairs) setup $s_node $d_node
	$apair($nr_pairs) setgid $group_id ;
	$apair($nr_pairs) setpairid $nr_pairs ;
	# lavanya: setfid $init_fid? this might overlap with
	# another pair's fid but that's ok? there is no global init_fid
	# guess it will just be 0 then? TO CHECK
	$apair($nr_pairs) setfid 0  ;
	# $apair($nr_pairs) setfid $init_fid  ;

	if {$tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {
		[$apair($nr_pairs) set spercs] attach $tracefile
	    }
	    if { $apair_type == "RCP_pair"} {
		[$apair($nr_pairs) set rcps] attach $tracefile
	    }
	}

	if {$common_tracefile != 0} {
	    if {$apair_type == "SPERC_pair"} {
		[$apair($nr_pairs) set spercs] attach-common $common_tracefile
	    }
	    # if { $apair_type == "RCP_pair"} {
	    # 	[$apair($nr_pairs) set rcps] attach $common_tracefile
	    # }
	}

	#### Callback Setting #################
	$apair($nr_pairs) set_fincallback $self fin_notify
	$apair($nr_pairs) set_startcallback $self start_notify
	#######################################
	$self schedule $nr_pairs
	incr nr_pairs
	#incr init_fid
    }

}

Agent_Aggr_pair instproc set_PCarrival_process {lambda cdffile rands1 rands2} {
    #public
    ##setup random variable rv_flow_intval and rv_npkts.
    #
    #- PCarrival:
    #flow arrival: poisson with rate $lambda
    #flow length: custom defined expirical cdf
    $self instvar rv_flow_intval rv_nbytes

    set rng1 [new RNG]
    $rng1 seed $rands1
    $self set rv_flow_intval [new RandomVariable/Exponential]
    $rv_flow_intval use-rng $rng1
    $rv_flow_intval set avg_ [expr 1.0/$lambda]

    set rng2 [new RNG]
    $rng2 seed $rands2
    $self set rv_nbytes [new RandomVariable/Empirical]
    $rv_nbytes use-rng $rng2
    $rv_nbytes set interpolation_ 2    
    $rv_nbytes loadCDF "common/${cdffile}_CDF.tcl"
}

Agent_Aggr_pair instproc set_PUarrival_process {lambda min_npkts max_npkts rands1 rands2} {
#Public
#setup random variable rv_flow_intval and rv_nbytes.
#To get the r.v.  call "value" function.
#ex)  $rv_flow_intval  value

#- PParrival:
#flow arrival: poissson with rate $lambda
#flow length : pareto with mean $mean_npkts pkts and shape parameter $shape. 

    $self instvar rv_flow_intval rv_nbytes

    set rng1 [new RNG]

    $rng1 seed $rands1
    $self set rv_flow_intval [new RandomVariable/Exponential]
    $rv_flow_intval use-rng $rng1
    $rv_flow_intval set avg_ [expr 1.0/$lambda]

    set rng2 [new RNG]
    $rng2 seed $rands2
    $self set rv_nbytes [new RandomVariable/Uniform]
    $rv_nbytes use-rng $rng2
    $rv_nbytes set min_ $min_npkts
    $rv_nbytes set max_ $max_npkts
}

Agent_Aggr_pair instproc set_PParrival_process {lambda mean_npkts shape rands1 rands2} {
#Public
#setup random variable rv_flow_intval and rv_npkts.
#To get the r.v.  call "value" function.
#ex)  $rv_flow_intval  value

#- PParrival:
#flow arrival: poissson with rate $lambda
#flow length : pareto with mean $mean_npkts pkts and shape parameter $shape. 

    $self instvar rv_flow_intval rv_nbytes

    set pareto_shape $shape
    set rng1 [new RNG]

    $rng1 seed $rands1
    $self set rv_flow_intval [new RandomVariable/Exponential]
    $rv_flow_intval use-rng $rng1
    $rv_flow_intval set avg_ [expr 1.0/$lambda]

    # I say rv_nbytes but it's actually rv_npkts
    set rng2 [new RNG]
    $rng2 seed $rands2
    $self set rv_nbytes [new RandomVariable/Pareto]
    $rv_nbytes use-rng $rng2
    $rv_nbytes set avg_ $mean_npkts
    $rv_nbytes set shape_ $pareto_shape
}


Agent_Aggr_pair instproc set_PBarrival_process {lambda mean_nbytes S1 S2 rands1 rands2} {
    global alllog
    #Public
    #setup random variable rv_flow_intval and rv_nbytes.
    #To get the r.v.  call "value" function. ex)  $rv_flow_intval  value
    #- PParrival:
    #flow arrival: poissson with rate $lambda
    #flow length : pareto with mean $mean_nbytes bytes and shape parameter $shape. 
    $self instvar rv_flow_intval rv_nbytes

    set rng1 [new RNG]
    $rng1 seed $rands1
    $self set rv_flow_intval [new RandomVariable/Exponential]
    $rv_flow_intval use-rng $rng1
    $rv_flow_intval set avg_ [expr 1.0/$lambda]

    set rng2 [new RNG]
    $rng2 seed $rands2
    $self set rv_nbytes [new RandomVariable/Binomial]
    $rv_nbytes use-rng $rng2

    $rv_nbytes set p1_ [expr  (1.0*$mean_nbytes - $S2)/($S1-$S2)]
    $rv_nbytes set s1_ $S1
    $rv_nbytes set s2_ $S2

    set p [expr  (1.0*$mean_nbytes - $S2)/($S1-$S2)]
    if { $p < 0 } {
	puts $alllog "In PBarrival, prob for bimodal p_ is negative %p_ exiting.. "
	flush stdout
	exit 0
    } 
}

Agent_Aggr_pair instproc start_notify {} {
    global alllog flow_gen
    #Callback Function
    #Note:
    #If we registor $self as "setcallback" of 
    #$apair($id), $apair($i) will callback this
    #function with argument id when the flow between the pair starts.
    #i.e. If we set:  "$apair(13) setcallback $self" somewhere,
    #"start_notyf 13" is called when the $apair(13)'s flow is started.
    global ns total_actfl
    $self instvar actfl;
    $self instvar group_id nr_pairs

    #puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) start_notify"

    incr actfl;
    incr total_actfl;
    puts $alllog "start_notify: [$ns now] $actfl active flows for this Agent_Aggr_pair (group_id $group_id, nr_pairs $nr_pairs), flowgen $flow_gen, $total_actfl active flows in total. "
}


Agent_Aggr_pair instproc fin_notify { pid bytes fldur fldur_data bps rttimes start_time bytes_scheduled bytes_sent} {
    global alllog
#    puts $alllog "Agent_Aggr_pair fin_notify $pid $bytes $fldur $bps $rttimes"
#Callback Function
#pid  : pair_id
#bytes : nr of bytes of the flow which has just finished
#fldur: duration of the flow which has just finished
#bps  : avg bits/sec of the flow which has just finished
#Note:
#If we registor $self as "setcallback" of 
#$apair($id), $apair($i) will callback this
#function with argument id when the flow between the pair finishes.
#i.e.
#If we set:  "$apair(13) setcallback $self" somewhere,
#"fin_notify 13 $bytes $fldur $bps" is called when the $apair(13)'s flow is finished.
# 
    global ns flow_gen flow_fin SIM_END total_actfl
    $self instvar logfile
    $self instvar group_id nr_pairs
    $self instvar actfl
    $self instvar apair
    $self instvar experiment

    puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) fin_notify from pid $pid"

    #Here, we re-schedule $apair($pid).
    #according to the arrival process.
    # lavanya: so we make a new pair every exp_rv_time and 
    # also exp_rv_time after a flow ends, all between same src
    # and dst node
    $self set actfl [expr $actfl - 1]
    set total_actfl [expr $total_actfl - 1]
    puts $alllog "fin_notify: [$ns now] pair $pid fldur $fldur bps $bps rttimes $rttimes, $actfl active flows for this Agent_Aggr_pair (group_id $group_id, nr_pairs $nr_pairs), $total_actfl active flows in total. "

    set fin_fid [$apair($pid) set id]
    
    ###### OUPUT STATISTICS #################
    if { [info exists logfile] } {
        set tmp_pkts [expr $bytes / 1460]
        puts $logfile "flow_stats: [$ns now] gid $group_id start_time $start_time pid $pid fid [$apair($pid) set flow_num] bytes $bytes fldur $fldur fldur_data $fldur_data actfl $actfl bps $bps tmp_pkts $tmp_pkts bytes_scheduled $bytes_scheduled bytes_sent $bytes_sent"
	
	#puts $logfile "$tmp_pkts $fldur $rttimes"
	#puts $logfile "pair $pid pkts $tmp_pkts fldur $fldur rttimes $rttimes group_id $group_id"
	flush stdout
    }
    set flow_fin [expr $flow_fin + 1]
    if {$flow_fin >= $SIM_END} {
	finish
    } 

    puts $alllog "Agent_Aggr_pair(group_id $group_id, nr_pairs $nr_pairs) fin_notify from pid $pid: since flow_gen $flow_gen < SIM_END $SIM_END schedule pid $pid"

    if {$flow_gen < $SIM_END &&
        $experiment == "fct"} {
	$self schedule $pid ;
	# re-schedule a pair having pair_id $pid. 
    } 
}

proc finish {} {
    global ns flowlog alllog
    global sim_start

    $ns flush-trace
    close $flowlog


    set t [clock seconds]
    puts $alllog "Simulation Finished!"
    puts $alllog "Time [expr $t - $sim_start] sec"
    puts $alllog "Date [clock format [clock seconds]]"
    exit 0
}


