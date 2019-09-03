proc parseSpercConfiguration {} {
    global argv argc a0
    global alllog

    set total_args [expr $a0 + 15]
    if {$argc < $total_args} {
	puts $alllog "parse-args.tcl: wrong number of arguments, expected $total_args, got $argc"
	exit 0
    }

    global SPERC_CONTROL_TRAFFIC_PC
    set SPERC_CONTROL_TRAFFIC_PC [lindex $argv [expr $a0 + 0]]
    global SPERC_HEADROOM_PC
    set SPERC_HEADROOM_PC [lindex $argv [expr $a0 + 1]]
    global SPERC_MAXSAT_TIMEOUT
    set SPERC_MAXSAT_TIMEOUT [lindex $argv [expr $a0 + 2]]
    global SPERC_MAXRTT_TIMEOUT
    set SPERC_MAXRTT_TIMEOUT [lindex $argv [expr $a0 + 3]]
    global SPERC_SYN_RETX_PERIOD_SECONDS
    set SPERC_SYN_RETX_PERIOD_SECONDS [lindex $argv [expr $a0 + 4]]
    global SPERC_FIXED_RTT_ESTIMATE_AT_HOST
    set SPERC_FIXED_RTT_ESTIMATE_AT_HOST [lindex $argv [expr $a0 + 5]]
    global SPERC_CONTROL_PKT_HDR_BYTES
    set SPERC_CONTROL_PKT_HDR_BYTES [lindex $argv [expr $a0 + 6]]
    global SPERC_DATA_PKT_HDR_BYTES
    set SPERC_DATA_PKT_HDR_BYTES [lindex $argv [expr $a0 + 7]]
    global SPERC_INIT_SPERC_DATA_INTERVAL
    set SPERC_INIT_SPERC_DATA_INTERVAL [lindex $argv [expr $a0 + 8]]
    global MATCH_DEMAND_TO_FLOW_SIZE
    set MATCH_DEMAND_TO_FLOW_SIZE [lindex $argv [expr $a0 + 9]]
    global SPERC_PRIO_WORKLOAD
    set SPERC_PRIO_WORKLOAD [lindex $argv [expr $a0 + 10]]
    global SPERC_PRIO_THRESH_INDEX
    set SPERC_PRIO_THRESH_INDEX [lindex $argv [expr $a0 + 11]]
    global SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES
    set SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES [lindex $argv [expr $a0 + 12]]
    global SPERC_JITTER
    set SPERC_JITTER [lindex $argv [expr $a0 + 13]]
    global SPERC_UPDATE_IN_REV
    set SPERC_UPDATE_IN_REV [lindex $argv [expr $a0 + 14]]

    puts "SPERC_CONTROL_TRAFFIC_PC ${SPERC_CONTROL_TRAFFIC_PC} SPERC_HEADROOM_PC ${SPERC_HEADROOM_PC} SPERC_MAXSAT_TIMEOUT ${SPERC_MAXSAT_TIMEOUT} SPERC_MAXRTT_TIMEOUT ${SPERC_MAXRTT_TIMEOUT} SPERC_SYN_RETX_PERIOD_SECONDS ${SPERC_SYN_RETX_PERIOD_SECONDS} SPERC_FIXED_RTT_ESTIMATE_AT_HOST ${SPERC_FIXED_RTT_ESTIMATE_AT_HOST} SPERC_CONTROL_PKT_HDR_BYTES ${SPERC_CONTROL_PKT_HDR_BYTES} SPERC_DATA_PKT_HDR_BYTES ${SPERC_DATA_PKT_HDR_BYTES} SPERC_INIT_SPERC_DATA_INTERVAL ${SPERC_INIT_SPERC_DATA_INTERVAL} MATCH_DEMAND_TO_FLOW_SIZE ${MATCH_DEMAND_TO_FLOW_SIZE} SPERC_PRIO_WORKLOAD ${SPERC_PRIO_WORKLOAD} SPERC_PRIO_THRESH_INDEX ${SPERC_PRIO_THRESH_INDEX} SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES ${SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES} SPERC_JITTER ${SPERC_JITTER} SPERC_UPDATE_IN_REV ${SPERC_UPDATE_IN_REV}"
}



# # lavanya
# Agent/SPERC set nodeid_ -1
# Agent/SPERC set numpkts_ 1
# Agent/SPERC set seqno_ 0
# set in sperc.tcl from $pktSize parsed in parse-common.tcl
# Agent/SPERC set packetSize_ 1000 
# Agent/SPERC set fid_ 0

# the following parameters are
# 1. initialized in drive_sperc_ct.sh
# 2. parsed in sperc/parse-common.tcl
# 3. set in sperc.tcl
# 4. to override initial values from ns-default.tcl
# Agent/SPERC set line_rate_ [expr $link_rate]Gb ;
# Queue set limit_ 50

# the following parameters are
# 1. initialized in drive_sperc_ct.sh
# 2. parsed in sperc/parse-common.tcl
# 3. used in common.tcl
# set sim_end [lindex $argv 0]
# set link_rate [lindex $argv 1]
# set paretoShape [lindex $argv 8]
# set topology_type [lindex $argv 9]
# set topology_x [lindex $argv 10]
# set FLOW_CDF [lindex $argv 11]
# set max_sim_time [lindex $argv 15 ] ;
# set num_active_servers [lindex $argv 16 ];
# set flowSizeDistribution [lindex $argv 21];

# the following parameters are 
# 1. initialized in run_sperc.sh
# 2. parsed in sperc/parse-common.tcl
# MEAN_LINK_DELAY=0.0000002 # 0.2 us !
# HOST_DELAY=0.0000025 # 2.5 us
# PKTSIZE=1500
# QUEUE_SAMPLING_INTVAL=0.0001
# USING_SPERC_DATA_RATE=1

# the following are calculated in parse-common.tcl
# meanNPackets - f(meanFlowSize)
# lambda = f(link_rate, load, meanFlowSize, topology_x)
# minRTT = f(mean_link_delay)
# maxRTT = f(mean_link_delay, host_delay, topology_x)


# the following parameters are
# 1. initialized in drive_sperc_ct.sh
# 2. parsed in sperc/parse-args.tcl
# 3. set in sperc.tcl
# 4. to override initial values from ns-default.tcl
# maybe better to not have default
# Agent/SPERC set line_rate_limiting_thresh_ 0 ; 
# Agent/SPERC set use_min_alloc_as_rate_ 1;
# Agent/SPERC set min_pkts_for_priority_ 0;
# Agent/SPERC set weight_for_small_flows_ 1;
# Agent/SPERC set use_stretch_ 0;
# Agent/SPERC set rate_change_interval_ 0
# Agent/SPERC set init_sperc_data_interval_ -1 ; 
# Agent/SPERC set match_demand_to_flow_size_ 0 ;
# Classifier/Hash/Dest/SPERC set controlTrafficPc_ 10;
# Classifier/Hash/Dest/SPERC set headroomPc_ 10;
# Classifier/Hash/Dest/SPERC set maxsatTimeout_ 100e-6;
# Classifier/Hash/Dest/SPERC set maxrttTimeout_ ..;
# Queue/Priority/SPERC set control_traffic_pc_ 0
# Queue/Priority/SPERC set alpha_ 0.5
# Queue/Priority/SPERC set beta_ 0.5
# Queue/Priority/SPERC set Tq_ 0.000012;
# Queue/Priority/SPERC set queue_threshold_ 15000;
# Queue/Priority/SPERC set stretch_factor_ 0.1;

# the following parameters are initialized in sperc.tcl
# 4. to override initial values from ns-default.tcl
# Queue/Priority set queue_num_ 3
# Queue/Priority set thresh_ 65 
# Queue/Priority set mean_pktsize_ 1500
# Queue/Priority set marking_scheme_ 0

# the following parameters are 
# 1. initialized in sperc.tcl
# 2. bound to sperc-host.cc
# Agent/SPERC set initial_min_rtt_seconds_ 0.000030
# Agent/SPERC set syn_retx_period_seconds_ 0.100 

# the following parameters are 
# 1. initialized in sperc.tcl
# 2. not bound or used
# Agent/SPERC set line_rate_ 0Gb ;
# Agent/SPERC set ref_start_when_period_rtts_ 1000
# Agent/SPERC set ref_rate_period_rtts_ 4
# # 0.000050 for DC, 100ms for WAN


# Queue/Priority/SPERC set fromnodeid_ -1
# Queue/Priority/SPERC set tonodeid_ -1
# Classifier/Hash/Dest/SPERC set nodeid_ -1;

# from sperc-host.h
# double SYN_RETX_PERIOD_SECONDS_;
# double INITIAL_MIN_RTT_SECONDS_; 
# double SPERC_CTRL_PKT_HDR_BYTES_;
# double SPERC_DATA_PKT_HDR_BYTES_;
# double INIT_SPERC_DATA_INTERVAL_;
# int MATCH_DEMAND_TO_FLOW_SIZE_;

# from sperc-control-host.h
# int USE_STRETCH_ = 0;
# double RATE_CHANGE_INTERVAL_ = 0;
# int USE_MIN_ALLOC_AS_RATE_ = 1;

# tcl bound
# // from sperc-host.cc
#   bind("seqno_", &seqno_);
#   bind("packetSize_", &size_); 
#   bind("line_rate_", &line_rate_);
#   bind("line_rate_limiting_thresh_", &line_rate_limiting_thresh_); // not used
#   bind("numpkts_",&numpkts_);
#   bind("fid_",&fid_);
#   bind("nodeid_", &nodeid_);
#   bind("rate_change_interval_", &RATE_CHANGE_INTERVAL_);
#   bind("syn_retx_period_seconds_", &SYN_RETX_PERIOD_SECONDS_);
#   bind("initial_min_rtt_seconds_", &INITIAL_MIN_RTT_SECONDS_);
#   bind("sperc_ctrl_pkt_hdr_bytes_", &SPERC_CTRL_PKT_HDR_BYTES_);
#   bind("sperc_data_pkt_hdr_bytes_", &SPERC_DATA_PKT_HDR_BYTES_);
#   bind("init_sperc_data_interval_", &INIT_SPERC_DATA_INTERVAL_);
#   bind("match_demand_to_flow_size_", &MATCH_DEMAND_TO_FLOW_SIZE_);
#   bind("use_min_alloc_as_rate_", &USE_MIN_ALLOC_AS_RATE_);
#   bind("use_stretch_", &USE_STRETCH_);
#   bind("min_pkts_for_priority_", &min_pkts_for_priority_);
#   bind("weight_for_small_flows_", &weight_for_small_flows_);

# // from sperc.cc
#   bind("fromnodeid_", &fromnodeid_);
#   bind("tonodeid_", &tonodeid_);
#   bind("alpha_", &alpha_);
#   bind("beta_", &beta_);
#   bind("Tq_", &Tq_);
#   bind("queue_threshold_", &queue_threshold_);
#   bind("stretch_factor_", &stretch_factor_);
