proc parseCommonConfiguration {} {
    global argc argv 
    global a0

    global alllog

    set a0 22
    if {$argc < $a0} {
	puts $alllog "parse-args.tcl: wrong number of arguments, expected $a0, got $argc"
	exit 0
    }

    global SIM_END
    set SIM_END [lindex $argv 0]
    global LINK_RATE
    set LINK_RATE [lindex $argv 1]
    global MEAN_LINK_DELAY
    set MEAN_LINK_DELAY [lindex $argv 2]
    global HOST_DELAY
    set HOST_DELAY [lindex $argv 3]
    global QUEUE_SIZE
    set QUEUE_SIZE [lindex $argv 4]
    global LOAD
    set LOAD [lindex $argv 5]
    global CONNECTIONS_PER_PAIR
    set CONNECTIONS_PER_PAIR [lindex $argv 6]
    global MEAN_FLOW_SIZE
    set MEAN_FLOW_SIZE [lindex $argv 7]
    global PARETO_SHAPE
    set PARETO_SHAPE [lindex $argv 8]
    global TOPOLOGY_TYPE
    set TOPOLOGY_TYPE [lindex $argv 9]
    global TOPOLOGY_X
    set TOPOLOGY_X [lindex $argv 10]
    global CDF
    set CDF [lindex $argv 11]
    global FLOWS_INPUT
    set FLOWS_INPUT [lindex $argv 12]
    global LOG_QUEUES
    set LOG_QUEUES [lindex $argv 13]
    global LOG_AGENTS
    set LOG_AGENTS [lindex $argv 14]
    global MAX_SIM_TIME
    set MAX_SIM_TIME [lindex $argv 15]
    global NUM_ACTIVE_SERVERS
    set NUM_ACTIVE_SERVERS [lindex $argv 16]
    global TRACE_QUEUES
    set TRACE_QUEUES [lindex $argv 17]
    global EXPERIMENT
    set EXPERIMENT [lindex $argv 18]
    global PKTSIZE
    set PKTSIZE [lindex $argv 19]
    global QUEUE_SAMPLING_INTVAL
    set QUEUE_SAMPLING_INTVAL [lindex $argv 20]
    global FLOW_SIZE_DISTRIBUTION
    set FLOW_SIZE_DISTRIBUTION [lindex $argv 21]

    puts "SIM_END ${SIM_END} LINK_RATE ${LINK_RATE} MEAN_LINK_DELAY ${MEAN_LINK_DELAY} HOST_DELAY ${HOST_DELAY} QUEUE_SIZE ${QUEUE_SIZE} LOAD ${LOAD} CONNECTIONS_PER_PAIR ${CONNECTIONS_PER_PAIR} MEAN_FLOW_SIZE ${MEAN_FLOW_SIZE} PARETO_SHAPE ${PARETO_SHAPE} TOPOLOGY_TYPE ${TOPOLOGY_TYPE} TOPOLOGY_X ${TOPOLOGY_X} CDF ${CDF} FLOWS_INPUT ${FLOWS_INPUT} LOG_QUEUES ${LOG_QUEUES} LOG_AGENTS ${LOG_AGENTS} MAX_SIM_TIME ${MAX_SIM_TIME} NUM_ACTIVE_SERVERS ${NUM_ACTIVE_SERVERS} TRACE_QUEUES ${TRACE_QUEUES} EXPERIMENT ${EXPERIMENT} PKTSIZE ${PKTSIZE} QUEUE_SAMPLING_INTVAL ${QUEUE_SAMPLING_INTVAL} FLOW_SIZE_DISTRIBUTION ${FLOW_SIZE_DISTRIBUTION}"


    if {$MEAN_FLOW_SIZE > 0} {    
	set meanNPackets [expr ($MEAN_FLOW_SIZE*8.0/1460*1500)]
	set lambda [expr (($LINK_RATE*$LOAD*1000000000)/($MEAN_FLOW_SIZE*8.0/1460*1500))/$TOPOLOGY_X]
	puts "Arrival: Poisson with inter-arrival [expr 1/$lambda * 1000] ms"
	puts "FlowSize: mean = $MEAN_FLOW_SIZE bytes, $meanNPackets packets."
	
    }
    set minRTT [expr (($MEAN_LINK_DELAY * 2) + ($HOST_DELAY * 2)) * 2.0]

    if { $TOPOLOGY_TYPE == "spine-leaf" } {
	set maxRTT [expr (($MEAN_LINK_DELAY * 4) + ($HOST_DELAY * 2)) * 2.0]
	puts $alllog "RTT including host delay is $minRTT to $maxRTT sec"

    } else {
	if { $TOPOLOGY_TYPE == "clos" } {
	    set maxRTT [expr (($MEAN_LINK_DELAY * 6) + ($HOST_DELAY * 2)) * 2.0]
	    puts $alllog "RTT including host delay is $minRTT to $maxRTT sec"

	} else {
	    if { $TOPOLOGY_TYPE == "b4" } {
		puts $alllog "RTT including host delay is $minRTT to ?? sec"
		
	}
	}

    }
}
