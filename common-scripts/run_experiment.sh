#!/bin/bash

source ../parallel.sh

function set_env_vars {
    SCHEME=$1;

    export HOME_DIR=`echo ~`;
    export PERC_DIR="${HOME_DIR}/perc/ns2-simulations"
    export PERC_COMMON="$PERC_DIR/common-scripts"

    export PERC_WATERFILLING_CT_BINARY="${HOME_DIR}/perc/waterfilling/wsim-ct"

    if [ "$1" == "sperc" ] ; then
	export PERC_NS2_BINARY="$PERC_DIR/ns2-sperc/bin/ns"
	export PERC_RUN_SCRIPTS="$PERC_DIR/sperc-scripts/files_to_run"
	export PERC_ANALYZE_SCRIPTS="$PERC_DIR/sperc-scripts/files_to_analyze"

    elif [ "$1" == "rcp" ] ; then 
	export PERC_NS2_BINARY="$PERC_DIR/ns2-rcp/bin/ns"
	export PERC_RUN_SCRIPTS="$PERC_DIR/rcp-scripts/files_to_run"
	export PERC_ANALYZE_SCRIPTS="$PERC_DIR/rcp-scripts/files_to_analyze"

    elif [ "$1" == "pfabric" ] ; then 
	export PERC_NS2_BINARY="$PERC_DIR/ns2-dctcp/bin/ns"
	export PERC_RUN_SCRIPTS="$PERC_DIR/pfabric-scripts/files_to_run"
    else
	echo "cannot set_env_vars for scheme $1, not implemented."
	return -1;
    fi;

    export PERC_RUN_INSTANCES="${PERC_RUN_SCRIPTS}/results"

    export PERC_RUN_LOG="${PERC_RUN_SCRIPTS}/drive-out"
    if [ ! -e ${PERC_RUN_LOG} ];  then
	mkdir ${PERC_RUN_LOG}
	success=$?
	if [ $success != 0 ]
	then
     	    echo "Couldn't create dir. for logs from run scripts, ${PERC_RUN_LOG}."
     	    return -1;
	fi
    else
	echo "Dir. for logs from run scripts exists: ${PERC_RUN_LOG}."
    fi;

    export PERC_SYMLINK_COMMON="${PERC_RUN_SCRIPTS}/common"
    if [ ! -e ${PERC_SYMLINK_COMMON} ];  then
	ln -s   ${PERC_COMMON} ${PERC_SYMLINK_COMMON}
	success=$?
	if [ $success != 0 ]
	then
     	    echo "Couldn't create symlink to ${PERC_COMMON} in ${PERC_RUN_SCRIPTS}"
     	    return -1;
	fi
    else
	echo "Symlink ${PERC_SYMLINK_COMMON} exists."
    fi;

}


function run_experiment_using_config {
    EXPERIMENT=$1;
    CONFIG=$2;
    DATE=$3;  # just a keyword to distinguish runs

    echo "EXPERIMENT ${EXPERIMENT}"
    echo "DATE ${DATE}"

    # echo $CONFIG
    eval $CONFIG

    echo "SCHEME ${SCHEME}"
    echo "CONFIG ${CONFIG}"

    START_DIR=`pwd`;

    cd ../$SCHEME-scripts/*run/
    success=$?
    if [ $success != 0 ]
    then
     	echo "Couldn't switch to directory to run $SCHEME"
     	return -1;
    fi
    echo "Switched directory to run $SCHEME"

    # set env vars
    set_env_vars ${SCHEME}
    success=$?
    if [ $success != 0 ]
    then
     	echo "Couldn't set env vars, symlinks, and dirs for running $SCHEME"
     	return -1;
    fi
    echo "Set env vars, symlinks, and dirs for running $SCHEME"


    if ( [ "${EXPERIMENT}" == "fct" ] ); then
	NAME="cdf${CDF}_load${LOAD}_topo${TOPOLOGY_TYPE}_${NUM_SERVERS}_${SCHEME}_${KW}_${DATE}"
    elif ( [ "${EXPERIMENT}" == "ct" ] ); then
	NAME="s1${SEED1}_s2${SEED2}_topo${TOPOLOGY_TYPE}_${NUM_SERVERS}_${FLOWS_PER_SERVER}_link${LINK_RATE}_${SCHEME}_${KW}_${DATE}"
    else
	echo "EXPERIMENT ${EXPERIMENT} not implemented."
	return -1;
    fi;

    echo "NAME ${NAME}"

    if ( [ "${EXPERIMENT}" == "fct" ] ); then
	LOG_OUT="${PERC_RUN_LOG}/${NAME}-run_${SCHEME}_fct.out"
	LOG_ERR="${PERC_RUN_LOG}/${NAME}-run_${SCHEME}_fct.err"
    else
	LOG_OUT="${PERC_RUN_LOG}/${NAME}-${SCHEME}-drive_ct.out"
	LOG_ERR="${PERC_RUN_LOG}/${NAME}-${SCHEME}-drive_ct.err"
    fi;

    echo "Log (out) start at time: $(date)." >> ${LOG_OUT}
    echo "Log (err) start at time: $(date)." >> ${LOG_ERR}

    if ( [ "${SCHEME}" == "sperc" ] || [ "${SCHEME}" == "rcp" ] ); then
	if ( [ "${EXPERIMENT}" == "fct" ] ); then	
     	    cmd="./run_${SCHEME}_fct.sh ${NAME} ${TOPOLOGY_TYPE} ${NUM_SERVERS} $LOAD cdf $CDF ${KW} 1>> ${LOG_OUT} 2>> ${LOG_ERR}"
	else
	    cmd="./drive_ct.sh $SEED1 $SEED2  ${TOPOLOGY_TYPE} ${NUM_SERVERS} ${FLOWS_PER_SERVER} ${LINK_RATE} ${EPOCH} ${DATE} ${NUM_EPOCHS} ${DUR} ${KW} ${FRAC_REPLACE} 1>> ${LOG_OUT} 2>> ${LOG_ERR}"
	fi;
    else
	# TOPOLOGY_TYPE=spine-leaf, NUM_SERVERS=144 defined in run_..sh
     	cmd="./run_${SCHEME}_fct.sh ${NAME} ${LOAD} cdf $CDF ${KW} 1>> ${PERC_RUN_LOG}/${NAME}-run_${SCHEME}_fct.out 2>> ${PERC_RUN_LOG}/${NAME}-run_${SCHEME}_fct.err"
    fi;
    
    echo $cmd;
    eval $cmd;
    success=$?
    if [ $success != 0 ]
    then
	if ( [ "${EXPERIMENT}" == "fct" ] ); then	
     	    echo "Couldn't run run_${SCHEME}_fct.sh cdf ${LOAD} ${CDF} ${DATE} .., check ${LOG_ERR}"
	else
     	    echo "Couldn't run drive_ct.sh for ${SCHEME} SEED1 $SEED1 SEED2 $SEED2 TOPOLOGY_TYPE ${TOPOLOGY_TYPE} LINK_RATE ${LINK_RATE} .. DATE ${DATE} KW ${KW} .., check ${LOG_ERR}"
	fi;
     	return -1;
    fi

    cd ${START_DIR};
}

function setup_for_fct {
    max_cpus=3;
    KW="sep2c"
    EXPERIMENT="fct"
    declare -a SCHEMES=("pfabric" "sperc" "rcp" ) # "pfabric" )
    declare -a CDFS=("search" ) #"learning" )
    declare -a LOADS=( 0.6 ) #0.2 ) # 0.8) #  0.6) #=(0.3 0.4 0.6) 
    i=0;
    job_list=()
    err_str_list=()

    for CDF in "${CDFS[@]}" ; do
	for LOAD in "${LOADS[@]}" ; do
	    for SCHEME in "${SCHEMES[@]}" ; do
		if [ "${SCHEME}" == "sperc" ] ; then 
		    CONFIG="SCHEME=${SCHEME};TOPOLOGY_TYPE=spine-leaf;NUM_SERVERS=144;LOAD=$LOAD;CDF=$CDF;KW=\"wt1-pri2-match-ign\""
		elif [ "${SCHEME}" == "rcp" ] ; then 
		    CONFIG="SCHEME=${SCHEME};TOPOLOGY_TYPE=spine-leaf;NUM_SERVERS=144;LOAD=$LOAD;CDF=$CDF;KW=\"a0.4b0.2\""
		elif [ "${SCHEME}" == "pfabric" ] ; then 
		    # "TOPOLOGY_TYPE=spine-leaf;NUM_SERVERS=144; default
		    CONFIG="SCHEME=${SCHEME};LOAD=$LOAD;CDF=$CDF;KW=\"basic\""
		else
		    echo "${SCHEME} not supported"
		    exit -1;
		fi;
		    job_list+=(" ./run_experiment.sh run_experiment_using_config ${EXPERIMENT} \"${CONFIG}\"  ${KW} &")
		    echo "job $i: ${job_list[i]}"
		    err_str_list+=("couldn't run fct for CDF: ${CDF}, LOAD: ${LOAD}, SCHEME: ${SCHEME}")
		    i=$((i+1));
	    done;
	done;
    done;

    read -p "Will use up to ${max_cpus} CPUs. Okay to run? (y/n)" ok
    if [ "${ok}" == "y" ]; then
	run_in_parallel job_list err_str_list ${max_cpus}
    fi
}

function setup_for_ct {
    max_cpus=3;
    KW="sep2c"
    NUM_RUNS_EACH=1;
    SEED1_START=711;
    SEED2_START=7011;
    EXPERIMENT="ct"

    declare -a CONFIGS=(  
"SCHEME=sperc;TOPOLOGY_TYPE=spine-leaf;NUM_SERVERS=144;FLOWS_PER_SERVER=20;LINK_RATE=100;EPOCH=0.002;NUM_EPOCHS=2;DUR=0.00001;KW=\"c4-h2-mt20\";FRAC_REPLACE=0.4"
#"SCHEME=sperc;TOPOLOGY_TYPE=b4;NUM_SERVERS=18;FLOWS_PER_SERVER=100;LINK_RATE=1;EPOCH=4;FRAC_REPLACE=0.7;KW=b4-1G-1500B-j0;"
# "SCHEME=sperc;TOPOLOGY_TYPE=b4;NUM_SERVERS=18;FLOWS_PER_SERVER=100;LINK_RATE=10;EPOCH=4;NUM_EPOCHS=11;DUR=0.01"
"SCHEME=rcp;TOPOLOGY_TYPE=spine-leaf;NUM_SERVERS=144;FLOWS_PER_SERVER=20;LINK_RATE=100;EPOCH=0.01;NUM_EPOCHS=2;DUR=0.00001;KW=\"spineleaf-100G-86pkt-a0.4b0.2-mr150000\";FRAC_REPLACE=0.4"
#"SCHEME=rcp;TOPOLOGY_TYPE=b4;NUM_SERVERS=18;FLOWS_PER_SERVER=100;LINK_RATE=10;EPOCH=10;KW=172pkt-a0.9b0.1-mr150000-prb100ms-i0.02"
#"SCHEME=rcp;TOPOLOGY_TYPE=b4;NUM_SERVERS=18;FLOWS_PER_SERVER=100;LINK_RATE=1;EPOCH=10;FRAC_REPLACE=0.7;KW=172pkt-a0.9b0.1-mr15000-prb100ms-i0.02" 
)

    SEED1=${SEED1_START};
    SEED2=${SEED2_START};
    i=0;
    job_list=()
    err_str_list=()
    for ((run=0;run<${NUM_RUNS_EACH};run++)); do
	for CONFIG in "${CONFIGS[@]}" ; do
	    job_list+=(" ./run_experiment.sh run_experiment_using_config ${EXPERIMENT} \"SEED1=${SEED1};SEED2=${SEED2};${CONFIG}\" ${KW} &")
	    echo "job $i (run ${run}): ${job_list[i]}"
	    err_str_list+=("couldn't run ${EXPERIMENT} for SEEDS ${SEED1} ${SEED2} ${CONFIG}")
	    i=$((i+1));
	done;
	SEED1=$((SEED1+1))
	SEED2=$((SEED2+1))	
    done;

    read -p "Will use up to ${max_cpus} CPUs. Okay to run? (y/n)" ok
    if [ "${ok}" == "y" ]; then
	run_in_parallel job_list err_str_list ${max_cpus}
    fi
}

if [ "$1" == "run_experiment_using_config" ] ; then
    echo "run_experiment_using_config"
    echo "EXPERIMENT $2";
    echo "CONFIG $3"
    echo "DATE $4"
    run_experiment_using_config $2 $3 $4
elif [ "$1" == "ct" ] ; then
    setup_for_ct
elif [ "$1" == "fct" ] ; then
    setup_for_fct
else
    echo "expected ct or fct"
fi;
