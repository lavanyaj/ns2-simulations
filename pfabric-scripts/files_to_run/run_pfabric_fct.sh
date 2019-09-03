#! /bin/bash 

echo "in run_pfabric_fct.sh"
declare -a ENV_VARS_STR=("PERC_RUN_INSTANCES" "PERC_RUN_SCRIPTS" "PERC_NS2_BINARY" )

for ENV_VAR_STR in "${ENV_VARS_STR[@]}" ; do
    if [[ -z "${!ENV_VAR_STR}" ]]; then
	echo "env var ${ENV_VAR_STR} not defined."
	exit -1;
    else
	echo "env var ${ENV_VAR_STR} : ${!ENV_VAR_STR}."
    fi
done;

DS=$PWD

if [ $# -lt 5 ] ; then
    echo "Not enough parameters. Expected 5 (unique_id load workload=cdf search|learning kw), got $#." 
    exit
fi 

UNIQUE_ID=$1
LOAD=$2
FLOW_SIZE_DISTRIBUTION=$3
CDF=$4
REQUESTED_KW=$5
if [ "${FLOW_SIZE_DISTRIBUTION}" == "cdf" ]; then 
    if [ "$CDF" == "search" ]; then MEAN_FLOW_SIZE=1661480;  fi;
    if [ "$CDF" == "learning" ]; then MEAN_FLOW_SIZE=7470820; fi;
fi;
MEAN_LINK_DELAY=0.0000002
HOST_DELAY=0.0000025
PARETO_SHAPE=1.05 # not used
TOPOLOGY_TYPE="spine-leaf"
FLOWS_INPUT=0 # not used
LOG_QUEUES=0 #"common/mon2-q.tcl"
LOG_AGENTS=0 #"common/mon2-a.tcl"
TRACE_QUEUES=1
EXPERIMENT="fct"
PKT_SIZE=1500
QUEUE_SAMPLING_INTVAL=0.0001

QUEUE_SIZE=0;
# 100000
versions=(
"SIM_END=100000; LINK_RATE=100; QUEUE_SIZE=240; CONNECTIONS_PER_PAIR=1; NUM_SERVERS=144; PERSISTENT_CONNECTIONS=1; INIT_CWND=120; MIN_RTO=0.000045; TOPOLOGY_X=1; MAX_SIM_TIME=100;  KW=\"basic\";"
)

for ver in "${versions[@]}" ; do
    eval "$ver"
    if [ "$QUEUE_SIZE" == "0" ]; then echo "couldn't eval and set QUEUE_SIZE" ; exit ; fi;
    if ( [ "${REQUESTED_KW}" == "${KW}" ] ); then 
	echo "name is ${UNIQUE_ID}"
	PERC_RUN_INSTANCE="${PERC_RUN_INSTANCES}/${UNIQUE_ID}"
	mkdir -p ${PERC_RUN_INSTANCE}
	cp ${PERC_RUN_SCRIPTS}/*.tcl ${PERC_RUN_SCRIPTS}/*.sh ${PERC_RUN_INSTANCE}
	cp -rL ${PERC_RUN_SCRIPTS}/common ${PERC_RUN_INSTANCE}
	cd ${PERC_RUN_INSTANCE}

	INSTANCE_RUN_OUT="${UNIQUE_ID}-ns.out"
	INSTANCE_RUN_ERR="${UNIQUE_ID}-ns.err"

	echo "Log (out) start at time: $(date)." >> ${INSTANCE_RUN_OUT}
	echo "Log (err) start at time: $(date)." >> ${INSTANCE_RUN_ERR}

	cmd="${SIM_END} ${LINK_RATE} ${MEAN_LINK_DELAY} ${HOST_DELAY} ${QUEUE_SIZE} ${LOAD} ${CONNECTIONS_PER_PAIR} ${MEAN_FLOW_SIZE} ${PARETO_SHAPE} ${TOPOLOGY_TYPE} ${TOPOLOGY_X} ${CDF} ${FLOWS_INPUT} ${LOG_QUEUES} ${LOG_AGENTS} ${MAX_SIM_TIME} ${NUM_SERVERS} ${TRACE_QUEUES} ${EXPERIMENT} ${PKT_SIZE} ${QUEUE_SAMPLING_INTVAL} ${FLOW_SIZE_DISTRIBUTION}"
	pfabric_cmd="${INIT_CWND} ${MIN_RTO} ${PERSISTENT_CONNECTIONS}"

	cmd="time ${PERC_NS2_BINARY} tcp.tcl ${cmd} ${pfabric_cmd} 1>> ${INSTANCE_RUN_OUT} 2>> ${INSTANCE_RUN_ERR}"
	echo $cmd
	eval $cmd 
	success=$?
	if [ $success != 0 ]
	then
	    echo "Couldn't run ns-2 simulation for p-fabric, check ${INSTANCE_RUN_ERR}"
	    exit $success;
	fi
    fi;
    cd $DS	
done   ;  # for versions
