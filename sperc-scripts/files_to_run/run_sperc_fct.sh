#! /bin/bash 

echo "in run_sperc_fct.sh"
declare -a ENV_VARS_STR=("PERC_RUN_INSTANCES" "PERC_RUN_SCRIPTS" "PERC_NS2_BINARY" )

for ENV_VAR_STR in "${ENV_VARS_STR[@]}" ; do
    if [[ -z "${!ENV_VAR_STR}" ]]; then
	echo "env var ${ENV_VAR_STR} not defined."
	exit -1;
    else
	echo "env var ${ENV_VAR_STR} : ${!ENV_VAR_STR}."
    fi
done;

if [ $# -lt 7 ] ; then
    echo "$0: Not enough parameters. Expected 7 (unique_id topology_type num_servers load workload=cf search|learning kw ..)  got $#." 1>&2;
    exit 1;
fi;


UNIQUE_ID=$1;
SIM_END=100000;LINK_RATE=100;LOAD=$4;CONNECTIONS_PER_PAIR=1;MEAN_FLOW_SIZE=0;
PARETO_SHAPE=0; TOPOLOGY_TYPE=${2};TOPOLOGY_X=1;CDF=0;FLOWS_INPUT=0;
LOG_QUEUES=0; LOG_AGENTS=0; MAX_SIM_TIME=100; NUM_ACTIVE_SERVERS=${3};
TRACE_QUEUES=0;EXPERIMENT="fct";PKTSIZE=1500;QUEUE_SAMPLING_INTVAL=0.001;
FLOW_SIZE_DISTRIBUTION=$5;
REQUESTED_KW=""
if [ "${FLOW_SIZE_DISTRIBUTION}" == "cdf" ]; then 
    CDF=$6;
    if [ "$CDF" == "search" ]; then MEAN_FLOW_SIZE=1661480; SPERC_PRIO_WORKLOAD=2; fi;
    if [ "$CDF" == "learning" ]; then MEAN_FLOW_SIZE=7470820; SPERC_PRIO_WORKLOAD=1; fi;
fi;
if [ $# -ge 7 ] ; then
    REQUESTED_KW=$7
fi

# queue stuff that we don't use or change
# QUEUE_THRESHOLD=${29}
# USE_STRETCH=${30}
# RCP_ALPHA=${31}
# RCP_BETA=${32}
# RCP_INIT_RTT=${33} replace with Queue log interval

echo "UNIQUE_ID=${UNIQUE_ID}; NUM_ACTIVE_SERVERS=${NUM_ACTIVE_SERVERS}; LOAD=${LOAD}; FLOW_SIZE_DISTRIBUTION=${FLOW_SIZE_DISTRIBUTION}; CDF=${CDF} MEAN_FLOW_SIZE=${MEAN_FLOW_SIZE}; PARETO_SHAPE=${PARETO_SHAPE}"

if [ "${TOPOLOGY_TYPE}" == "b4" ]; then
    LOG_AGENTS="common/mon3-a.tcl"
    LOG_QUEUES="common/mon3-q.tcl"
    # This is a version for running in WAN, see that maxsat_timeout is 100ms, max_rtt_timeout is 500ms, no priority
    declare -a versions=(
	# FCT in WAN version, notice does match-demand and control traffic is 2pc and no jitter
	"LINK_RATE=1; PKTSIZE=1500; QUEUE_SIZE=83; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; SPERC_CONTROL_TRAFFIC_PC=2; SPERC_HEADROOM_PC=2; SPERC_MAXSAT_TIMEOUT=0.1; SPERC_MAXRTT_TIMEOUT=0.5; SPERC_SYN_RETX_PERIOD_SECONDS=0.2; SPERC_FIXED_RTT_ESTIMATE_AT_HOST=0.1; SPERC_CONTROL_PKT_HDR_BYTES=64; SPERC_DATA_PKT_HDR_BYTES=40; SPERC_INIT_SPERC_DATA_INTERVAL=-1; MATCH_DEMAND_TO_FLOW_SIZE=1; SPERC_PRIO_WORKLOAD=0; SPERC_PRIO_THRESH_INDEX=0; SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES=1; SPERC_JITTER=0; SPERC_UPDATE_IN_REV=0; KW=\"b4-basic-1G-1500B\""    
    ) ;
elif [ "${TOPOLOGY_TYPE}" == "spine-leaf" ]; then
    LOG_AGENTS="common/mon2-a.tcl"
    LOG_QUEUES="common/mon2-q.tcl"
    # This is a version for running in a data center, see that maxsat_timeout is 20us, max_rtt_timeout is 100us, priority
    declare -a versions=(
    # FCT in data center version, sperc basic, note we don't prioritize short flows (SPERC_PRIO_THRESH_INDEX=0+SPERC_PRIO_WORKLOAD) and wait to get rate before start (INIT_SPERC_DATA_INTERVAL=-1)
	"QUEUE_SIZE=83; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; SPERC_CONTROL_TRAFFIC_PC=2; SPERC_HEADROOM_PC=2; SPERC_MAXSAT_TIMEOUT=0.000020; SPERC_MAXRTT_TIMEOUT=0.000100; SPERC_SYN_RETX_PERIOD_SECONDS=0.000040; SPERC_FIXED_RTT_ESTIMATE_AT_HOST=0.000012; SPERC_CONTROL_PKT_HDR_BYTES=64; SPERC_DATA_PKT_HDR_BYTES=40; SPERC_INIT_SPERC_DATA_INTERVAL=-1; MATCH_DEMAND_TO_FLOW_SIZE=1;  SPERC_PRIO_THRESH_INDEX=0; SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES=1; SPERC_JITTER=0; SPERC_UPDATE_IN_REV=1; KW=\"wt1-pri2-match-ign\""    
    # FCT in data center version, sperc short, note we prioritize short flows (SPERC_PRIO_THRESH_INDEX=1+SPERC_PRIO_WORKLOAD) and start them at line-rate (INIT_SPERC_DATA_INTERVAL=0.000012)
    	"QUEUE_SIZE=83; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; SPERC_CONTROL_TRAFFIC_PC=2; SPERC_HEADROOM_PC=2; SPERC_MAXSAT_TIMEOUT=0.000020; SPERC_MAXRTT_TIMEOUT=0.000100; SPERC_SYN_RETX_PERIOD_SECONDS=0.000040; SPERC_FIXED_RTT_ESTIMATE_AT_HOST=0.000012; SPERC_CONTROL_PKT_HDR_BYTES=64; SPERC_DATA_PKT_HDR_BYTES=40; SPERC_INIT_SPERC_DATA_INTERVAL=0.000012; MATCH_DEMAND_TO_FLOW_SIZE=1; SPERC_PRIO_THRESH_INDEX=1; SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES=1; SPERC_JITTER=0; SPERC_UPDATE_IN_REV=1; KW=\"wt1-pri2-match-start100-short-ign\""    
    ) ;
else
    echo "No configurations to run s-PERC in ${TOPOLOGY_TYPE} topology, must be one of b4| spine-leaf."
    exit 1;
fi;


DS=$PWD
echo "Current directory is ${DS}, PERC_RUN_INSTANCES is ${PERC_RUN_INSTANCES}, PERC_RUN_SCRIPTS is ${PERC_RUN_SCRIPTS}"

for ver in "${versions[@]}" ; do
    eval "$ver"	    ;
    if [ "${SPERC_CONTROL_TRAFFIC_PC}" == "0" ]; then echo "couldn't eval and set SPERC_CONTROL_TRAFFIC_PC" ; exit ; fi;
    if ( [ "${REQUESTED_KW}" == "${KW}" ] ); then 
	# FCT-specific
	echo "name is ${UNIQUE_ID}"
	PERC_RUN_INSTANCE="${PERC_RUN_INSTANCES}/${UNIQUE_ID}"
	why="Create a clean directory to run ${KW} version of SPERC. ${PERC_RUN_INSTANCE}."
	cmd="mkdir -p ${PERC_RUN_INSTANCE}"
	eval $cmd
	echo $cmd
	success=$?
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

	why="Copy over scripts from ${PERC_RUN_SCRIPTS} to run ${KW} version of SPERC simulation."
	cmd="cp ${PERC_RUN_SCRIPTS}/*.tcl ${PERC_RUN_SCRIPTS}/*.sh ${PERC_RUN_INSTANCE}"
	eval $cmd
	echo $cmd
	success=$?
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

	cmd="cp -rL ${PERC_RUN_SCRIPTS}/common ${PERC_RUN_INSTANCE}"
	eval $cmd
	echo $cmd
	success=$?
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

	why="Change to clean directory to run simulations for ${KW} version of SPERC."
	cmd="cd ${PERC_RUN_INSTANCE}"
	eval $cmd
	echo $cmd
	success=$?
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

	why="Run ${KW} version of SPERC."
	INSTANCE_RUN_OUT="${UNIQUE_ID}-ns.out"
	INSTANCE_RUN_ERR="${UNIQUE_ID}-ns.err"

	echo "Log (out) start at time: $(date)." >> ${INSTANCE_RUN_OUT}
	echo "Log (err) start at time: $(date)." >> ${INSTANCE_RUN_ERR}

	sperc_cmd="${SPERC_CONTROL_TRAFFIC_PC} ${SPERC_HEADROOM_PC} ${SPERC_MAXSAT_TIMEOUT} ${SPERC_MAXRTT_TIMEOUT} ${SPERC_SYN_RETX_PERIOD_SECONDS} ${SPERC_FIXED_RTT_ESTIMATE_AT_HOST} ${SPERC_CONTROL_PKT_HDR_BYTES} ${SPERC_DATA_PKT_HDR_BYTES} ${SPERC_INIT_SPERC_DATA_INTERVAL} ${MATCH_DEMAND_TO_FLOW_SIZE} ${SPERC_PRIO_WORKLOAD} ${SPERC_PRIO_THRESH_INDEX} ${SPERC_WEIGHTED_MAX_MIN_NUM_CLASSES} ${SPERC_JITTER} ${SPERC_UPDATE_IN_REV} "
	common_cmd="$SIM_END $LINK_RATE $MEAN_LINK_DELAY $HOST_DELAY $QUEUE_SIZE $LOAD $CONNECTIONS_PER_PAIR $MEAN_FLOW_SIZE $PARETO_SHAPE ${TOPOLOGY_TYPE} ${TOPOLOGY_X} $CDF ${FLOWS_INPUT} ${LOG_QUEUES} ${LOG_AGENTS} ${MAX_SIM_TIME} ${NUM_ACTIVE_SERVERS} ${TRACE_QUEUES} ${EXPERIMENT} ${PKTSIZE} ${QUEUE_SAMPLING_INTVAL} ${FLOW_SIZE_DISTRIBUTION}"
	cmd="time ${PERC_NS2_BINARY} sperc.tcl ${common_cmd} ${sperc_cmd} 1>> ${INSTANCE_RUN_OUT} 2>> ${INSTANCE_RUN_ERR}"
	echo $cmd
	eval $cmd 
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

	why="Change to starting directory ${DS}."
	cmd="cd $DS";
	eval $cmd
	echo $cmd
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

    fi;
done ; # for ver in 	


