remove-all-packet-headers;
add-packet-header Flags IP RCP rtProtoDV

set agentPairType "RCP_pair"
set switchAlg "DropTail/RCP"
RtModule/Base set use_sperc_classifier_ 0
set alllog [open all.tr w]
set ctlog [open ct.tr w]
source "common/parse-common.tcl"
source "parse-args.tcl"

parseCommonConfiguration
parseRcpConfiguration

if {$MEAN_FLOW_SIZE > 0} {    
    set meanNPackets [expr ($MEAN_FLOW_SIZE*8.0/($PKTSIZE-$rcp_hdr_bytes)*$PKTSIZE)]
    set lambda [expr (($LINK_RATE*$LOAD*1000000000)/($MEAN_FLOW_SIZE*8.0/($PKTSIZE-$rcp_hdr_bytes)*$PKTSIZE))/$TOPOLOGY_X]
    puts "Arrival: Poisson with inter-arrival [expr 1/$lambda * 1000] ms"
    puts "FlowSize: mean = $MEAN_FLOW_SIZE bytes, $meanNPackets packets."
    
}


################# Transport Options ####################
Agent/RCP set rto_abs_ $rcp_rto_abs_
Agent/RCP set rto_fact_ $rcp_rto_fact_
Agent/RCP set rate_probe_interval_ $rcp_rate_probe_interval_
Agent/RCP set seqno_ 0
Agent/RCP set packetSize_ $PKTSIZE
Agent/RCP set numpkts_ 1 ; # to customize later
Agent/RCP set nodeid_ -1
Agent/RCP set tonodeid_ -1 ; # to avoid warning, don't really use
Agent/RCP set fromnodeid_ -1 ; # to avoid warning, don't really use
Agent/RCP set fid_ 0 ; # to customize later
Agent/RCP set syn_delay_ $rcp_syn_delay ;
Agent/RCP set rcp_hdr_bytes_ $rcp_hdr_bytes ;
Agent/RCP set probe_pkt_bytes_ $rcp_probe_pkt_bytes_;
# Agent/RCP functions in rcp-pair.tcl
Queue/DropTail/RCP set fromnodeid_ -1 ; # to customize later
Queue/DropTail/RCP set tonodeid_ -1 ; # to customize later
Queue/DropTail/RCP set min_rate_ $rcp_min_rate_
Queue/DropTail/RCP set alpha_ $rcp_alpha
Queue/DropTail/RCP set beta_ $rcp_beta
Queue/DropTail/RCP set init_rate_fact_ $rcp_init_rate_fact
Queue/DropTail/RCP set upd_timeslot_ $rcp_init_rtt ; # to check later
Queue/DropTail/RCP set init_rtt_ $rcp_init_rtt  ; # to check later
Queue/DropTail/RCP set packet_size_ $PKTSIZE
Queue/DropTail/RCP set simple_rtt_update_ $rcp_simple_rtt_update_

################# Switch Options ######################
Queue set limit_ $QUEUE_SIZE
Queue/DropTail set queue_in_bytes_ true
Queue/DropTail set drop_front_ false
Queue/DropTail set summarystats_ false
Queue/DropTail set mean_pktsize_ $PKTSIZE

############## Multipathing and SYMMETRIC routing ###########################
source "common/agent-aggr-pair.tcl"
source "rcp-pair.tcl"
source "common/common.tcl"


$ns rtproto DV
# TODO(lavanya): for FCT, this is fine, routing advt go out only once
# for schedule form file, make sure to set this high enough so ^
Agent/rtProto/DV set advertInterval	[expr 2*$SIM_END]
# for WAN, we set link costs to equal link delay in milliseconds, to be safe, set max to 1000000
Agent/rtProto/DV set INFINITY 1000000

Node set multiPath_ 1 
# lavanya for symmetric paths (don't need for RCP, but would help with debugging)
Classifier/MultiPath set nodeid_ -1
Classifier/MultiPath set perflow_ 1
Classifier/MultiPath set symmetric_ 1

############# Topology #########################

if { "$TOPOLOGY_TYPE" == "spine-leaf" } {
    setupSpineLeafTopology
} else {
    if { "$TOPOLOGY_TYPE" == "b4" } {
	setupB4TopologyFromFile "common/b4.tcl"
    } else {
	if { "$TOPOLOGY_TYPE" == "clos" } {
	    setupClosTopology
	} else {
	    puts "invalid TOPOLOGY_TYPE $TOPOLOGY_TYPE"
	    exit
	}
    }
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

if { "$FLOWS_INPUT" != "0" } { 
    puts $alllog "calling runScheduleFromFile $FLOWS_INPUT"
    runFromFile $FLOWS_INPUT
} else {
    if { "${EXPERIMENT}" == "fct" } {
	puts $alllog "not scheduling from file, set up pairs for poission arrival with flow size from ${CDF}"
	runPoisson       
    } else {
	puts "FLOWS_INPUT is 0 and EXPERIMENT $EXPERIMENT: exiting."
	exit
    }
}

if { "$LOG_AGENTS" != "0" } {
    puts "calling monitorAgents $LOG_AGENTS"
    monitorAgents $LOG_AGENTS
}

puts $alllog "Initial agent creation done";flush stdout
puts $alllog "Simulation started! Run till ${MAX_SIM_TIME}s max."


$ns at ${MAX_SIM_TIME} finish
$ns run
