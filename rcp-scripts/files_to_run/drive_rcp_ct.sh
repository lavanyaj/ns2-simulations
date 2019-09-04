#! /bin/bash 

declare -a ENV_VARS_STR=("PERC_RUN_INSTANCES" "PERC_RUN_SCRIPTS" "PERC_NS2_BINARY" )

for ENV_VAR_STR in "${ENV_VARS_STR[@]}" ; do
    if [[ -z "${!ENV_VAR_STR}" ]]; then
	echo "env var ${ENV_VAR_STR} not defined."
	exit -1;
    else
	echo "env var ${ENV_VAR_STR} : ${!ENV_VAR_STR}."
    fi
done;

if [ $# -lt 6 ] ; then
    echo "$0: Not enough parameters. Expected 6 (flows_input suffix max_sim_time num_servers b4|spine-leaf link_rate), got $#." 1>&2;
    exit 1;
else
    echo "$0: Got parameters. flows_input $1 suffix $2 max_sim_time $3 num_servers $4 topology_type $5 link_rate $6." 
    if [ $# -eq 7 ] ; then
	echo "$0: Got parameters. keyword $7."
	if [ $# -eq 8 ] ; then
	    echo "$0: Got parameters. log_queues $8."
	fi
    fi

fi;

DS=$PWD

FLOWS_INPUT=${1};UNIQUE_ID=$2;MAX_SIM_TIME=${3}; NUM_ACTIVE_SERVERS=${4};
 EXPERIMENT="ct"; SIM_END=100000; LOAD=0; CONNECTIONS_PER_PAIR=1;
CDF=0;MEAN_FLOW_SIZE=0; FLOW_SIZE_DISTRIBUTION="custom"; PARETO_SHAPE=0; LOG_QUEUES="mon3-q.tcl";
LOG_AGENTS="mon3-a.tcl";TRACE_QUEUES=0;LINK_RATE=100;TOPOLOGY_X=1;QUEUE_SAMPLING_INTVAL=0.001; PKTSIZE=1500;
REQUESTED_TOPOLOGY_TYPE=${5}
REQUESTED_LINK_RATE=${6};

REQUESTED_KW=""
if [ $# -ge 7 ] ; then
    REQUESTED_KW=$7
fi

echo "UNIQUE_ID=${UNIQUE_ID}; NUM_ACTIVE_SERVERS=${NUM_ACTIVE_SERVERS}; FLOWS_INPUT=${FLOWS_INPUT}"

if [ "${REQUESTED_TOPOLOGY_TYPE}" == "b4" ]; then
    LOG_AGENTS="common/mon3-a.tcl"
    LOG_QUEUES="common/mon3-q.tcl"
elif [ "${REQUESTED_TOPOLOGY_TYPE}" == "spine-leaf" ]; then
    LOG_AGENTS="common/mon2-a.tcl"
    LOG_QUEUES="common/mon2-q.tcl"
else
    echo "No configurations to run RCP in ${REQUESTED_TOPOLOGY_TYPE} topology."
    exit
fi;

if [ $# -ge 8 ] ; then
    LOG_QUEUES=$8
fi

declare -a versions=(
    # This is a version for running in WAN
    "LINK_RATE=1; PKTSIZE=1500; QUEUE_SIZE=172; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; TOPOLOGY_TYPE=\"b4\"; RCP_ALPHA=0.9;RCP_BETA=0.1;RCP_INIT_RATE_FACT=0.5;RCP_INIT_RTT=0.02;RCP_SYN_DELAY=0.200;RCP_HDR_BYTES=40;RCP_SIMPLE_RTT_UPDATE=0;RCP_MIN_RATE=15000;RCP_RTO_ABS=0;RCP_RTO_FACT=2;RCP_PROBE_PKT_BYTES=40;RCP_RATE_PROBE_INTERVAL=0.100;KW=\"172pkt-a0.9b0.1-mr15000-prb100ms-i0.02\""
    "LINK_RATE=10; PKTSIZE=1500; QUEUE_SIZE=172; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; TOPOLOGY_TYPE=\"b4\"; RCP_ALPHA=0.9;RCP_BETA=0.1;RCP_INIT_RATE_FACT=0.5;RCP_INIT_RTT=0.02;RCP_SYN_DELAY=0.200;RCP_HDR_BYTES=40;RCP_SIMPLE_RTT_UPDATE=0;RCP_MIN_RATE=150000;RCP_RTO_ABS=0;RCP_RTO_FACT=2;RCP_PROBE_PKT_BYTES=40;RCP_RATE_PROBE_INTERVAL=0.100;KW=\"172pkt-a0.9b0.1-mr150000-prb100ms-i0.02\""

    # This is a version for running in a data center
    "LINK_RATE=100; QUEUE_SIZE=86; MEAN_LINK_DELAY=0.0000002; HOST_DELAY=0.0000025; TOPOLOGY_TYPE=\"spine-leaf\"; RCP_ALPHA=0.4;RCP_BETA=0.2;RCP_INIT_RATE_FACT=0.1;RCP_INIT_RTT=0.0000108;RCP_SYN_DELAY=0.000100;RCP_HDR_BYTES=40;RCP_SIMPLE_RTT_UPDATE=0;RCP_MIN_RATE=150000;RCP_RTO_ABS=0;RCP_RTO_FACT=2;RCP_PROBE_PKT_BYTES=40;RCP_RATE_PROBE_INTERVAL=0.000100;KW=\"spineleaf-100G-86pkt-a0.4b0.2-mr150000\";"
)



for ver in "${versions[@]}" ; do
    # used for rcp queues		    
    eval "$ver"	    
    if [ "$RCP_ALPHA" == "0" ]; then echo "couldn't eval and set RCP_ALPHA" ; exit ; fi;

    # [ "${REQUESTED_KW}" == "" ] ||
    if [ "${REQUESTED_LINK_RATE}" == "${LINK_RATE}" ] && [ "${REQUESTED_TOPOLOGY_TYPE}" == "${TOPOLOGY_TYPE}" ] && ( [ "${REQUESTED_KW}" == "${KW}" ] ); then 
	# specific to CT
	echo "name is ${UNIQUE_ID}"
	PERC_RUN_INSTANCE="${PERC_RUN_INSTANCES}/${UNIQUE_ID}/run_dir"
	mkdir -p ${PERC_RUN_INSTANCE}

	cp $PERC_RUN_SCRIPTS/*.tcl $PERC_RUN_SCRIPTS/*.sh ${PERC_RUN_INSTANCE}
	cp -rL $PERC_RUN_SCRIPTS/common ${PERC_RUN_INSTANCE}
	cd ${PERC_RUN_INSTANCE}
	
	echo "in directory .."
	pwd

	rcp_cmd="$RCP_ALPHA $RCP_BETA $RCP_INIT_RATE_FACT $RCP_INIT_RTT $RCP_SYN_DELAY $RCP_HDR_BYTES ${RCP_SIMPLE_RTT_UPDATE} ${RCP_MIN_RATE} ${RCP_RTO_ABS} ${RCP_RTO_FACT} ${RCP_PROBE_PKT_BYTES} ${RCP_RATE_PROBE_INTERVAL}"
	
	common_cmd="$SIM_END $LINK_RATE $MEAN_LINK_DELAY $HOST_DELAY $QUEUE_SIZE $LOAD $CONNECTIONS_PER_PAIR $MEAN_FLOW_SIZE $PARETO_SHAPE ${TOPOLOGY_TYPE} ${TOPOLOGY_X} $CDF ${FLOWS_INPUT} ${LOG_QUEUES} ${LOG_AGENTS} ${MAX_SIM_TIME} ${NUM_ACTIVE_SERVERS} ${TRACE_QUEUES} ${EXPERIMENT} ${PKTSIZE} ${QUEUE_SAMPLING_INTVAL} ${FLOW_SIZE_DISTRIBUTION}"
	
	INSTANCE_RUN_OUT="${UNIQUE_ID}-ns.out"
	INSTANCE_RUN_ERR="${UNIQUE_ID}-ns.err"
	echo "Log (out) start at time: $(date)." > ${INSTANCE_RUN_OUT}
	echo "Log (err) start at time: $(date)." > ${INSTANCE_RUN_ERR}

	cmd="time ${PERC_NS2_BINARY} rcp.tcl ${common_cmd} ${rcp_cmd} 1>> ${INSTANCE_RUN_OUT} 2>> ${INSTANCE_RUN_ERR}"
	

	echo $cmd
	eval $cmd 

	cd $DS
    fi;
done ; # for ver in 	
