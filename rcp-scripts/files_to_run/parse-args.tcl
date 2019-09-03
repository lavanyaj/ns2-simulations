proc parseRcpConfiguration {} {
    global argc argv a0
    global rcp_alpha rcp_beta rcp_init_rate_fact
    global rcp_init_rtt rcp_syn_delay
    global rcp_hdr_bytes
    global rcp_simple_rtt_update_ rcp_min_rate_ 
    global rcp_rto_abs_ rcp_rto_fact_
    global rcp_probe_pkt_bytes_ 
    global rcp_rate_probe_interval_
    global alllog

    set total_args [expr $a0 + 12 - 1]
    if {$argc < $total_args} {
	puts "parse-args.tcl: wrong number of arguments, expected $total_args, got $argc"
	exit 0
    }

    set rcp_alpha [lindex $argv [expr $a0 + 0]] 
    set rcp_beta [lindex $argv [expr $a0 + 1]] 
    set rcp_init_rate_fact [lindex $argv [expr $a0 + 2]] 
    set rcp_init_rtt [lindex $argv [expr $a0 + 3]] 
    set rcp_syn_delay [lindex $argv [expr $a0 + 4]] 
    set rcp_hdr_bytes [lindex $argv [expr $a0 + 5]] 
    set rcp_simple_rtt_update_ [lindex $argv [expr $a0 + 6]]
    set rcp_min_rate_ [lindex $argv [expr $a0 + 7]]
    set rcp_rto_abs_ [lindex $argv [expr $a0 + 8]]
    set rcp_rto_fact_ [lindex $argv [expr $a0 + 9]]
    set rcp_probe_pkt_bytes_ [lindex $argv [expr $a0 + 10]]
    set rcp_rate_probe_interval_ [lindex $argv [expr $a0 + 11]]

    puts $alllog "rcp: alpha $rcp_alpha beta $rcp_beta  init_rate_fact $rcp_init_rate_fact   init_rtt $rcp_init_rtt syn_delay $rcp_syn_delay rcp_hdr_bytes $rcp_hdr_bytes"
    puts $alllog "rcp: simple rtt update $rcp_simple_rtt_update_ min_rate_ $rcp_min_rate_ rto_abs $rcp_rto_abs_ rto_fact_ $rcp_rto_fact_ rate_probe_interval ${rcp_rate_probe_interval_} probe_pkt_bytes ${rcp_probe_pkt_bytes_}"
}


proc parseConfiguration {} {
    global ns
    global argc argv sim_end link_rate mean_link_delay host_delay queueSize connections_per_pair
    global meanFlowSize paretoShape lambda
    global rcp_alpha rcp_beta rcp_init_rate_fact
    global rcp_init_rtt rcp_syn_delay
    global rcp_hdr_bytes
    global topology_spt topology_tors topology_spines topology_x
    global enableNAM FLOW_CDF pktSize queueSamplingInterval scheduleFromFile monitorQueuesFile
    global monitorAgentsFile
    global rcp_simple_rtt_update_ rcp_min_rate_ rcp_rto_abs_ rcp_rto_fact_
    global max_sim_time
    global rcp_probe_pkt_bytes_ rcp_rate_probe_interval_
    global num_active_servers
    global lambda
    global alllog
    global trace_queues
    global tcl_config

    if {$argc != 34} {
	puts "$tcl_config: wrong number of arguments, expected 34, got $argc"
	exit 0
    }

    set sim_end [lindex $argv 0]
    set link_rate [lindex $argv 1]
    set mean_link_delay [lindex $argv 2]
    set host_delay [lindex $argv 3]
    set queueSize [lindex $argv 4]
    set load [lindex $argv 5]
    set connections_per_pair [lindex $argv 6]; # lavanya: 8 for baseline, DCTCP not really simulataneous (?)
    set meanFlowSize [lindex $argv 7]
    set paretoShape [lindex $argv 8]

    if {$meanFlowSize > 0} {    
	set lambda [expr ($link_rate*$load*1000000000)/($meanFlowSize*8.0/1460*1500)]
	puts "Arrival: Poisson with inter-arrival [expr 1/$lambda * 1000] ms"
	puts "FlowSize: mean = $meanFlowSize"
    }

    #### Multipath
    #### Transport settings options

    #### Switch side options
    # reactive options, same for data and control
    set rcp_alpha [lindex $argv 9]
    set rcp_beta [lindex $argv 10]
    set rcp_init_rate_fact [lindex $argv 11]
    set rcp_init_rtt [lindex $argv 12]
    set rcp_syn_delay [lindex $argv 13]
    set rcp_hdr_bytes [lindex $argv 14]
    #### topology
    set topology_spt [lindex $argv 15]
    set topology_tors [lindex $argv 16]
    set topology_spines [lindex $argv 17]
    set topology_x [lindex $argv 18]
    #### NAM
    set enableNAM [lindex $argv 19]
    set FLOW_CDF [lindex $argv 20]
    set scheduleFromFile [lindex $argv 21] ;
    set monitorQueuesFile [lindex $argv 22] ;
    set monitorAgentsFile [lindex $argv 23] ;
    set rcp_simple_rtt_update_ [lindex $argv 24] ;
    set rcp_min_rate_ [lindex $argv 25] ;
    set rcp_rto_abs_ [lindex $argv 26] ;
    set rcp_rto_fact_ [lindex $argv 27] ;
    set max_sim_time [lindex $argv 28 ] ;
    set rcp_probe_pkt_bytes_ [lindex $argv 29 ];
    set rcp_rate_probe_interval_ [lindex $argv 30 ];
    set num_active_servers [lindex $argv 31 ];
    set trace_queues [lindex $argv 32 ];
    # argv 33 is tcl_config

    #### Packet size is in bytes.
    set pktSize 1500
    #### trace frequency
    set queueSamplingInterval 0.000100

    puts $alllog "Simulation input:"
    puts $alllog "Dynamic Flow - Pareto"
    puts $alllog "topology: $topology_spines spines, server per rack = $topology_spt, $topology_tors racks, x = $topology_x"
    puts $alllog "sim_end $sim_end"
    puts $alllog "link_rate $link_rate Gbps"
    puts $alllog "link_delay $mean_link_delay sec"
    puts $alllog "RTT  [expr $mean_link_delay * 2.0 * 4] sec"
    puts $alllog "RTT including host delay is [expr (($mean_link_delay * 2) + ($host_delay * 2)) * 2.0] to [expr (($mean_link_delay * 4) + ($host_delay * 2)) * 2.0] sec"
    puts $alllog "host_delay $host_delay sec"
    puts $alllog "queue size $queueSize pkts"
    puts $alllog "load $load"
    puts $alllog "connections_per_pair $connections_per_pair"
    puts $alllog "pktSize(payload) (data packet on wire) $pktSize Bytes"
    puts $alllog "we'll need ot adjust flow size based on actual payload that can fit"
    puts $alllog " in pktSize, assuming xx header bytes for SPERC"
    puts $alllog "enableNAM $enableNAM"
    puts $alllog "rcp: alpha $rcp_alpha beta $rcp_beta  init_rate_fact $rcp_init_rate_fact   init_rtt $rcp_init_rtt syn_delay $rcp_syn_delay rcp_hdr_bytes $rcp_hdr_bytes"
    puts $alllog "scheduleFromFile (flows_input) $scheduleFromFile monitorQueuesFile (queues_config) $monitorQueuesFile"
    puts $alllog "rcp: simple rtt update $rcp_simple_rtt_update_ min_rate_ $rcp_min_rate_ rto_abs $rcp_rto_abs_ rto_fact_ $rcp_rto_fact_ rate_probe_interval ${rcp_rate_probe_interval_} probe_pkt_bytes ${rcp_probe_pkt_bytes_}"
    puts $alllog "max_sim_time ${max_sim_time} num_active_servers ${num_active_servers}"
}
