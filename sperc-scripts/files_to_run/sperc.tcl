# we'll use the SPERC ns-2 binary where classifier, link, agents etc.
# are all modified for running SPERC, here we only set up these objects
# in the a DC topology and configure the topology and objects based
# on command-line arguments

remove-all-packet-headers;
add-packet-header Flags IP SPERC SPERC_CTRL rtProtoDV


set agentPairType "SPERC_pair"
set switchAlg "Priority/SPERC"
#set switchAlg "DRR/SPERC"
RtModule/Base set use_sperc_classifier_ 1
set alllog [open all.tr w]
source "common/parse-common.tcl"
source "parse-args.tcl"

parseCommonConfiguration
parseSpercConfiguration

if {$MEAN_FLOW_SIZE > 0} {    
    set meanNPackets [expr ($MEAN_FLOW_SIZE*8.0/($PKTSIZE-$SPERC_DATA_PKT_HDR_BYTES)*$PKTSIZE)]
    set lambda [expr (($LINK_RATE*$LOAD*1000000000)/($MEAN_FLOW_SIZE*8.0/($PKTSIZE-$SPERC_DATA_PKT_HDR_BYTES)*$PKTSIZE))/$TOPOLOGY_X]
    puts "Arrival: Poisson with inter-arrival [expr 1/$lambda * 1000] ms"
    puts "FlowSize: mean = $MEAN_FLOW_SIZE bytes, $meanNPackets packets."
    
}

#set trace_file [open qjump.tr w]
#$ns trace-all $trace_file

################# Transport Options ####################
# moved from ns-default.tcl to here
Agent/SPERC set nodeid_ -1
Agent/SPERC set numpkts_ 1
Agent/SPERC set seqno_ 0
puts "setting Agent/SPERC PKTSIZE_ to $PKTSIZE"
flush stdout

Agent/SPERC set PKTSIZE_ $PKTSIZE
Agent/SPERC set fid_ 0

Agent/SPERC set SPERC_SYN_RETX_PERIOD_SECONDS $SPERC_SYN_RETX_PERIOD_SECONDS
Agent/SPERC set SPERC_FIXED_RTT_ESTIMATE_AT_HOST $SPERC_FIXED_RTT_ESTIMATE_AT_HOST
Agent/SPERC set SPERC_CONTROL_PKT_HDR_BYTES $SPERC_CONTROL_PKT_HDR_BYTES
Agent/SPERC set SPERC_DATA_PKT_HDR_BYTES $SPERC_DATA_PKT_HDR_BYTES
Agent/SPERC set SPERC_INIT_SPERC_DATA_INTERVAL $SPERC_INIT_SPERC_DATA_INTERVAL
Agent/SPERC set MATCH_DEMAND_TO_FLOW_SIZE $MATCH_DEMAND_TO_FLOW_SIZE
Agent/SPERC set SPERC_PRIO_WORKLOAD $SPERC_PRIO_WORKLOAD
Agent/SPERC set SPERC_PRIO_THRESH_INDEX $SPERC_PRIO_THRESH_INDEX
Agent/SPERC set SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES $SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES
Agent/SPERC set SPERC_JITTER $SPERC_JITTER
Agent/SPERC set SPERC_LINE_RATE $LINK_RATE
# Agent/SPERC instproc done {} { }
Agent/SPERC instproc fin-received {} { }
# Agent/SPERC instproc all-datapkts-received {} { }
# Agent/SPERC instproc syn-sent {} { }
# Agent/SPERC instproc ctrl-syn-sent {} { }
# Agent/SPERC instproc begin-datasend {} { }
# Agent/SPERC instproc finish-datasend {} { }
# Agent/SPERC instproc stop-data {rttimes} { }
# Agent/SPERC instproc stop-ctrl {} { }

Queue/Priority/SPERC set fromnodeid_ -1
Queue/Priority/SPERC set tonodeid_ -1
Queue/Priority/SPERC set SPERC_CONTROL_TRAFFIC_PC $SPERC_CONTROL_TRAFFIC_PC;
Queue/Priority/SPERC set alpha_ 0.5
Queue/Priority/SPERC set beta_ 0.5
Queue/Priority/SPERC set Tq_ 0.000012;
Queue/Priority/SPERC set queue_threshold_ 15000;
Queue/Priority/SPERC set stretch_factor_ 0.1;

# Parameters for SPERC processing at switches
############## SPERC link settings  ###########################
# Parameters for SPERC processing at switches, we implement this as a classifier
# so ingress and egress links can share state
# lavanya: used when finding ingress/ egress link at node entry_ classifier

Classifier/Hash/Dest/SPERC set SPERC_CONTROL_TRAFFIC_PC $SPERC_CONTROL_TRAFFIC_PC
Classifier/Hash/Dest/SPERC set SPERC_HEADROOM_PC $SPERC_HEADROOM_PC
Classifier/Hash/Dest/SPERC set SPERC_MAXSAT_TIMEOUT $SPERC_MAXSAT_TIMEOUT
Classifier/Hash/Dest/SPERC set SPERC_MAXRTT_TIMEOUT $SPERC_MAXRTT_TIMEOUT
Classifier/Hash/Dest/SPERC set SPERC_UPDATE_IN_REV $SPERC_UPDATE_IN_REV
Classifier/Hash/Dest/SPERC set nodeid_ -1;

Agent/SPERC set tonodeid_ -1
Agent/SPERC set fromnodeid_ -1

################# Switch Options ######################
Queue set limit_ $QUEUE_SIZE
Queue/Priority set queue_num_ 3
Queue/Priority set thresh_ 65 
Queue/Priority set mean_pktsize_ $PKTSIZE
Queue/Priority set marking_scheme_ 0
Queue/Priority/SPERC set fromnodeid_ -1
Queue/Priority/SPERC set tonodeid_ -1

############## Multipathing and SYMMETRIC routing ###########################
# TODO(lavanya): for FCT, this is fine, routing advt go out only once
# for schedule from file, make sure to set this high enough 
Agent/rtProto/DV set advertInterval	[expr 2*$SIM_END]
# for WAN, we set link costs to equal link delay in milliseconds, to be safe, set max to 1000000
Agent/rtProto/DV set INFINITY 1000000
Node set multiPath_ 1 
# lavanya for SPERC
Classifier/MultiPath set nodeid_ -1
Classifier/MultiPath set perflow_ 1
Classifier/MultiPath set symmetric_ 1 
############# Topology #########################
source "common/agent-aggr-pair.tcl"
source "sperc-pair.tcl"
source "common/common.tcl"
############## Multipathing and SYMMETRIC routing ###########################
$ns rtproto DV

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
    if { "$EXPERIMENT" == "fct" } {
	puts $alllog "not scheduling from file, set up pairs for poission arrival with flow size from $CDF"
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
# $ns gen-map 

puts $alllog "Initial agent creation done";
flush stdout
puts $alllog "Simulation started!"
puts $alllog "Simulation started! Run till ${MAX_SIM_TIME} s max."

$ns at ${MAX_SIM_TIME} finish
$ns run
