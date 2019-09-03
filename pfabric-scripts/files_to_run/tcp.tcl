#remove-all-packet-headers;
#add-packet-header Flags IP TCP

# common to dctcp, pfabric, they'll set
# tcpAgent and switchAlg


set alllog [open all.tr w]
source "common/parse-common.tcl"
source "parse-args.tcl"
source "common/agent-aggr-pair.tcl"
source "tcp-pair.tcl"
source "common/common.tcl"

parseCommonConfiguration
parsePfabricConfiguration

if {$MEAN_FLOW_SIZE > 0} {    
    set meanNPackets [expr ($MEAN_FLOW_SIZE*8.0/($PKTSIZE-40)*$PKTSIZE)]
    set lambda [expr (($LINK_RATE*$LOAD*1000000000)/($MEAN_FLOW_SIZE*8.0/($PKTSIZE-40)*$PKTSIZE))/$TOPOLOGY_X]
    puts "Arrival: Poisson with inter-arrival [expr 1/$lambda * 1000] ms"
    puts "FlowSize: mean = $MEAN_FLOW_SIZE bytes, $meanNPackets packets."
    
}

Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
Agent/TCP set packetSize_ $PKTSIZE
Agent/TCP/FullTcp set segsize_ $PKTSIZE
Agent/TCP/FullTcp set spa_thresh_ 0
Agent/TCP set window_ 64
Agent/TCP set windowInit_ 2 ; # set again later
Agent/TCP set slow_start_restart_ $slowstartrestart
Agent/TCP set windowOption_ 0
Agent/TCP set tcpTick_ 0.000001
Agent/TCP set minrto_ $min_rto
Agent/TCP set maxrto_ 2

Agent/TCP/FullTcp set nodelay_ true; # disable Nagle
Agent/TCP/FullTcp set segsperack_ $ackRatio; 
Agent/TCP/FullTcp set interval_ 0.000006
if {$perflowMP == 0} {  
    Agent/TCP/FullTcp set dynamic_dupack_ 0.75
}
if {$ackRatio > 2} {
    Agent/TCP/FullTcp set spa_thresh_ [expr ($ackRatio - 1) * $PKTSIZE]
}
if {$enableHRTimer != 0} {
    Agent/TCP set minrto_ 0.00100 ; # 1ms
    Agent/TCP set tcpTick_ 0.000001
}
if {[string compare $sourceAlg "DCTCP-Sack"] == 0} {
    Agent/TCP set ecnhat_ true
    Agent/TCPSink set ecnhat_ true
    Agent/TCP set ecnhat_g_ $DCTCP_g;
}
#Shuang
Agent/TCP/FullTcp set prio_scheme_ $prio_scheme_;
Agent/TCP/FullTcp set dynamic_dupack_ 1000000; #disable dupack
Agent/TCP set window_ 1000000
Agent/TCP set windowInit_ $init_cwnd;
Agent/TCP set rtxcur_init_ $min_rto;
Agent/TCP/FullTcp/Sack set clear_on_timeout_ false;
#Agent/TCP/FullTcp set pipectrl_ true;
Agent/TCP/FullTcp/Sack set sack_rtx_threshmode_ 2;
if {$QUEUE_SIZE > 12} {
   Agent/TCP set maxcwnd_ [expr $QUEUE_SIZE - 1];
} else {
   Agent/TCP set maxcwnd_ 12;
}
Agent/TCP/FullTcp set prob_cap_ $prob_cap_;

################# Switch Options ######################

Queue set limit_ $QUEUE_SIZE

# DCTCP doesn't use DropTail, it uses RED
Queue/DropTail set queue_in_bytes_ true
Queue/DropTail set mean_pktsize_ [expr $PKTSIZE+40]
Queue/DropTail set drop_prio_ $drop_prio_
Queue/DropTail set deque_prio_ $deque_prio_
Queue/DropTail set keep_order_ $keep_order_

Queue/RED set bytes_ false
Queue/RED set queue_in_bytes_ true
Queue/RED set mean_pktsize_ $PKTSIZE
Queue/RED set setbit_ true
Queue/RED set gentle_ false
Queue/RED set q_weight_ 1.0
Queue/RED set mark_p_ 1.0
Queue/RED set thresh_ $DCTCP_K
Queue/RED set maxthresh_ $DCTCP_K
Queue/RED set drop_prio_ $drop_prio_
Queue/RED set deque_prio_ $deque_prio_
			 
#DelayLink set avoidReordering_ true
# DCTCP doesn't use Phantom Queues
if {$enablePQ == 1} {
    Queue/RED set pq_enable_ 1
    Queue/RED set pq_mode_ $PQ_mode ; # 0 = Series, 1 = Parallel
    Queue/RED set pq_drainrate_ [expr $PQ_gamma * $link_rate * 1000000000]
    Queue/RED set pq_thresh_ $PQ_thresh
    #Queue/RED set thresh_ 100000
    #Queue/RED set maxthresh_ 100000
}

################ Pacer Options #######################
# DCTCP doesn't use Pacer
if {$enablePacer == 1} {
    TBF set bucket_ [expr 3100 * 8]
    TBF set qlen_ 10000
    TBF set pacer_enable_ 1
    TBF set assoc_timeout_ $Pacer_assoc_timeout
    TBF set assoc_prob_ $Pacer_assoc_prob
    TBF set maxrate_ [expr $link_rate * 1000000000]
    TBF set minrate_ [expr 0.01 * $link_rate * 1000000000]
    TBF set rate_ [expr $link_rate * 1000000000]   
    TBF set qlength_factor_ $Pacer_qlength_factor
    TBF set rate_ave_factor_ $Pacer_rate_ave_factor
    TBF set rate_update_interval_ $Pacer_rate_update_interval    

    for { set i 0 } { $i < $S } { incr i } {
	for { set j 0 } { $j < $TBFsPerServer} { incr j } {
	    set tbf($s($i),$j) [new TBF]
	} 
    }
} else {
    set tbf 0
}

############ Multipathing ######
if {$enableMultiPath == 1} {
    $ns rtproto DV
    Agent/rtProto/DV set advertInterval [expr 2*$SIM_END]  
    Node set multiPath_ 1 
    if {$perflowMP != 0} {
	Classifier/MultiPath set perflow_ 1
    }
}

############# Topology #########################

if { "$TOPOLOGY_TYPE" == "spine-leaf" } {
    setupSpineLeafTopology
} else {
    puts "invalid TOPOLOGY_TYPE $TOPOLOGY_TYPE"
    exit
}

if { "$LOG_QUEUES" != "0" } {
    monitorQueues $LOG_QUEUES
}

##############  Tocken Buckets for Pacer #########
set tbf 0
#############  Agents  #########################

puts "Setting up connections and scheduling flows..."; flush stdout

set flow_gen 0
set flow_fin 0
set flowlog [open flow.tr w]
set init_fid 0

if { "$EXPERIMENT" == "fct" } {
    puts $alllog "not scheduling from file, set up pairs for poission arrival with flow size from $CDF"
    runPoisson
}

if { "$LOG_AGENTS" != "0" } {
    puts "calling monitorAgents $LOG_AGENTS"
    monitorAgents $LOG_AGENTS
}
# $ns gen-map 

puts $alllog "Initial agent creation done";
flush stdout
puts $alllog "Simulation started!"
puts $alllog "Simulation started! Run till ${MAX_SIM_TIME}s max."

$ns at ${MAX_SIM_TIME} finish
$ns run
