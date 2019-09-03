proc parsePfabricConfiguration {} {
    global argc argv a0 alllog
    # include everything from Original qjump.tcl
    global enableMultiPath perflowMP
    global ackRatio slowstartrestart
    global DCTCP_g DCTCP_K init_cwnd min_rto
    global persistent_connections
    # set in tcp.tcl
    global sourceAlg switchAlg 
    set total_args [expr $a0 + 3]
    if {$argc < $total_args} {
	puts $alllog "parse-args.tcl: wrong number of arguments, expected $total_args, got $argc"
	exit 0
    }

    set init_cwnd [lindex $argv [expr $a0 + 0]] 
    set min_rto [lindex $argv [expr $a0 + 1]] 
    set persistent_connections [lindex $argv [expr $a0 + 2]] 

    # for pfabric
    global enableHRTimer
    global drop_prio_ deque_prio_
    global prob_cap_ keep_order_
    global enablePacer enablePQ TBFsPerServer
    global prio_scheme_ enable_dctcp
    global agentPairType sourcealg switchAlg myAgent

    set agentPairType "TCP_pair"
    set myAgent "Agent/TCP/FullTcp/Sack/MinTCP"
    
    # different from DCTCP
    set enableMultiPath 1
    set perflowMP 0
    set sourceAlg "DCTCP-Sack"
    # set min_rto
    set deque_prio_ true
    set drop_prio_ true
    set DCTCP_K 10000
    # set queue_size (common args)
    # set link_rate (common args)
    # set mean_link_delay (common)
    # set host_delay (common)
    # set pareto_shape (common)
    set ackRatio 1
    set slowstartrestart true
    set DCTCP_g 0.0625
    set switchAlg "DropTail"
    set enablePQ 0
    set enablePacer 0
    set TBFsPerServer 1
    set enableHRTimer 0
    # what are this?
    set prob_cap_ 5
    set prio_scheme_ 2 
    set keep_order_ true

    # Phantom Queue settings 
    set PQ_mode 0
    set PQ_gamma 0
    set PQ_thresh 0

    ### Pacer settings
    set TBFsPerServer 1
    set Pacer_qlength_factor 3000
    set Pacer_rate_ave_factor 0.125
    set Pacer_rate_update_interval 0.0000072
    set Pacer_assoc_prob 0.125
    set Pacer_assoc_timeout 0.001



    #set queueSamplingInterval 1
    puts "enableMultiPath=$enableMultiPath, perflowMP=$perflowMP"
    puts "source algorithm: $sourceAlg"
    puts "ackRatio $ackRatio"
    puts "DCTCP_g $DCTCP_g"
    puts "HR-Timer $enableHRTimer"
    puts "slow-start Restart $slowstartrestart"
    puts "switch algorithm $switchAlg"
    puts "DCTCP_K_ $DCTCP_K"
    puts "enablePQ $enablePQ"
    puts "PQ_mode $PQ_mode"
    puts "PQ_gamma $PQ_gamma"
    puts "PQ_thresh $PQ_thresh"
    puts "enablePacer $enablePacer"
    puts "TBFsPerServer $TBFsPerServer"
    puts "Pacer_qlength_factor $Pacer_qlength_factor"
    puts "Pacer_rate_ave_factor $Pacer_rate_ave_factor"
    puts "Pacer_rate_update_interval $Pacer_rate_update_interval"
    puts "Pacer_assoc_prob $Pacer_assoc_prob"
    puts "Pacer_assoc_timeout $Pacer_assoc_timeout"
    puts "init_cwnd ${init_cwnd}"
    puts " "
}
