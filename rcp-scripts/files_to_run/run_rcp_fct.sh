#! /bin/bash 

echo "in run_rcp_fct.sh"
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
    if [ "$CDF" == "search" ]; then MEAN_FLOW_SIZE=1661480; fi;
    if [ "$CDF" == "learning" ]; then MEAN_FLOW_SIZE=7470820;  fi;
fi;

if [ $# -ge 7 ] ; then
    REQUESTED_KW=$7
fi

echo "UNIQUE_ID=${UNIQUE_ID}; NUM_ACTIVE_SERVERS=${NUM_ACTIVE_SERVERS}; LOAD=${LOAD}; FLOW_SIZE_DISTRIBUTION=${FLOW_SIZE_DISTRIBUTION}; CDF=${CDF} MEAN_FLOW_SIZE=${MEAN_FLOW_SIZE}; PARETO_SHAPE=${PARETO_SHAPE}"

if [ "${TOPOLOGY_TYPE}" == "b4" ]; then
    LOG_AGENTS="common/mon3-a.tcl"
    LOG_QUEUES="common/mon3-q.tcl"
    echo "${TOPOLOGY_TYPE} is b4, configuring RCP to run in a WAN (LINK_RATE=1Gb/s, RCP_INIT_RTT=0.1s).\n"
    # This is a version for running in a WAN
    declare -a versions=(
	"LINK_RATE=1; PKTSIZE=1500; QUEUE_SIZE=86; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; RCP_ALPHA=0.4;RCP_BETA=0.2;RCP_INIT_RATE_FACT=0.5;RCP_INIT_RTT=0.1;RCP_SYN_DELAY=0.200;RCP_HDR_BYTES=40;RCP_SIMPLE_RTT_UPDATE=0;RCP_MIN_RATE=1500;RCP_RTO_ABS=0;RCP_RTO_FACT=2;RCP_PROBE_PKT_BYTES=40;RCP_RATE_PROBE_INTERVAL=0.500;KW=\"b4-1G-1500KB-a0.4b0.2\""
    )
elif [ "${TOPOLOGY_TYPE}" == "spine-leaf" ]; then
    LOG_AGENTS="common/mon2-a.tcl"
    LOG_QUEUES="common/mon2-q.tcl"
    echo "${TOPOLOGY_TYPE} is spine-leaf, configuring RCP to run in a DC (LINK_RATE=100Gb/s, RCP_INIT_RTT=0.0000108s).\n"
    # This is a version for running in a data center
    declare -a versions=(
	"LINK_RATE=100; QUEUE_SIZE=86; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; RCP_ALPHA=0.4;RCP_BETA=0.2;RCP_INIT_RATE_FACT=0.5;RCP_INIT_RTT=0.0000108;RCP_SYN_DELAY=0.000100;RCP_HDR_BYTES=40;RCP_SIMPLE_RTT_UPDATE=0;RCP_MIN_RATE=150000;RCP_RTO_ABS=0;RCP_RTO_FACT=2;RCP_PROBE_PKT_BYTES=40;RCP_RATE_PROBE_INTERVAL=0.000100;KW=\"a0.4b0.2\""
	#    "alpha=0.4;beta=0.2;rcp_simple_rtt_update=0;rcp_min_rate=150000;rcp_rto_abs=0;rcp_rto_fact=2;rcp_probe_pkt_bytes=40;rcp_rate_probe_interval=0.000100;rcp_init_rtt=0.0000108;topo=0;wlog=1;init_rate_fact=0.1;topology_x=1;num_servers=32;queue_size=86;trace_queues=0;kw=\"a0.4b0.2\";"
    )
else
    echo "No configurations to run RCP in ${TOPOLOGY_TYPE} topology, must be one of b4| spine-leaf."
    exit 1;
fi;

echo "Requested KW is ${REQUESTED_KW}"

DS=$PWD
echo "Current directory is ${DS}, PERC_RUN_INSTANCES is ${PERC_RUN_INSTANCES}, PERC_RUN_SCRIPTS is ${PERC_RUN_SCRIPTS}"

for ver in "${versions[@]}" ; do
    eval "$ver"	    ;
    if [ "${RCP_ALPHA}" == "0" ]; then echo "couldn't eval and set RCP_ALPHA" ; exit ; fi;
    ADJUSTED_LOAD=$(python -c "print \"%.2f\"%(${LOAD}/${TOPOLOGY_X})")
    INIT_RATE_FACT=$(python -c "print \"%.2f\"%(1/(1-${ADJUSTED_LOAD}))")
    if ( [ "${REQUESTED_KW}" == "${KW}" ] ); then 
	# FCT-specific
	echo "name is ${UNIQUE_ID}"
	PERC_RUN_INSTANCE="${PERC_RUN_INSTANCES}/${UNIQUE_ID}"
	why="Create a clean directory to run ${KW} version of RCP. ${PERC_RUN_INSTANCE}."
	cmd="mkdir -p ${PERC_RUN_INSTANCE}"
	eval $cmd
	echo $cmd
	success=$?
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

	why="Copy over scripts from ${PERC_RUN_SCRIPTS} to run ${KW} version of RCP simulation."
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

	why="Change to clean directory to run simulations for ${KW} version of RCP."
	cmd="cd ${PERC_RUN_INSTANCE}"
	eval $cmd
	echo $cmd
	success=$?
	if [ $success != 0 ]
	then
	    echo "Couldn't $why"
	    exit $success;
	fi

	why="Run ${KW} version of RCP."
	INSTANCE_RUN_OUT="${UNIQUE_ID}-ns.out"
	INSTANCE_RUN_ERR="${UNIQUE_ID}-ns.err"

	echo "Log (out) start at time: $(date)." >> ${INSTANCE_RUN_OUT}
	echo "Log (err) start at time: $(date)." >> ${INSTANCE_RUN_ERR}

	common_cmd="$SIM_END $LINK_RATE $MEAN_LINK_DELAY $HOST_DELAY $QUEUE_SIZE $LOAD $CONNECTIONS_PER_PAIR $MEAN_FLOW_SIZE $PARETO_SHAPE ${TOPOLOGY_TYPE} ${TOPOLOGY_X} $CDF ${FLOWS_INPUT} ${LOG_QUEUES} ${LOG_AGENTS} ${MAX_SIM_TIME} ${NUM_ACTIVE_SERVERS} ${TRACE_QUEUES} ${EXPERIMENT} ${PKTSIZE} ${QUEUE_SAMPLING_INTVAL} ${FLOW_SIZE_DISTRIBUTION}"
	rcp_cmd="$RCP_ALPHA $RCP_BETA $RCP_INIT_RATE_FACT $RCP_INIT_RTT $RCP_SYN_DELAY $RCP_HDR_BYTES ${RCP_SIMPLE_RTT_UPDATE} ${RCP_MIN_RATE} ${RCP_RTO_ABS} ${RCP_RTO_FACT} ${RCP_PROBE_PKT_BYTES} ${RCP_RATE_PROBE_INTERVAL}"
	cmd="time ${PERC_NS2_BINARY} rcp.tcl ${common_cmd} ${rcp_cmd} 1>> ${INSTANCE_RUN_OUT} 2>> ${INSTANCE_RUN_ERR}"

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


