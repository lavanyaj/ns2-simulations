# check if files folders exist after we create them (rate-changes not working currently)

declare -a ENV_VARS_STR=("PERC_RUN_INSTANCES" "PERC_RUN_SCRIPTS" "PERC_NS2_BINARY" "PERC_WATERFILLING_CT_BINARY" "PERC_ANALYZE_SCRIPTS" )
# PERC_NUMERICAL
for ENV_VAR_STR in "${ENV_VARS_STR[@]}" ; do
    if [[ -z "${!ENV_VAR_STR}" ]]; then
	echo "env var ${ENV_VAR_STR} not defined."
	exit -1;
    else
	echo "env var ${ENV_VAR_STR} : ${!ENV_VAR_STR}."
    fi
done;


if [[ -z "${PERC_NUMERICAL}" ]]; then
    echo "env variable PERC_NUMERICAL not defined. source config_env.sh ?"
#    exit 1;
fi

if [ $# -lt 12 ] ; then
    echo "$0: Not enough parameters. Expected 10 (seed1 seed2 b4|spine-leaf num_servers flows_per_server link_rate epoch date num_epochs dur kw frac_replace), got $#." 1>&2
    exit 1;
else
    echo "$0: Got parameters seed1 $1 seed2 $2 topology_type $3 num_servers $4 flows_per_server $5 link_rate $6 Gb/s sepoch $7 s date $8 num_epochs $9 dur ${10} (kw ${11} frac_replace ${12})." 
fi;

SEED1=$1
SEED2=$2
TOPOLOGY_TYPE=$3
NUM_SERVERS=$4 #144 #144
FLOWS_PER_SERVER=$5 #10000 #20 #20
LINK_RATE=$6
EPOCH=$7 #2 #0.002
DATE=$8
NUM_EPOCHS=$9
DUR=${10}
KW=""
if [ $# -ge 11 ] ; then
    KW=${11}
fi
FRAC_REPLACE="0.4"
if [ $# -ge 12 ] ; then
    FRAC_REPLACE=${12}
fi

START_TIME=1.0

if [ $NUM_EPOCHS -le 0 ] ; then
    echo "invalid NUM_EPOCHS ${NUM_EPOCHS}"
    exit
else
    echo "NUM_EPOCHS is ${NUM_EPOCHS}, DUR is ${DUR}"
fi;

MAX_SIM_TIME=$(python -c "print(${START_TIME} + ${NUM_EPOCHS} * ${EPOCH} + 0.0001);")
NUM_BYTES=$(python -c "print( int(round( (${NUM_EPOCHS} * ${EPOCH} + 0.0001) * ${LINK_RATE} * 1e9 / 8.0 )));") ; #100000000000

if [ $NUM_BYTES -le 0 ] ; then
    echo "invalid NUM_BYTES ${NUM_BYTES}"
    exit
else
    echo "NUM_BYTES for ${MAX_SIM_TIME} s of simulation time, starting at ${START_TIME} s, and ${LINK_RATE} Gb/s links is ${NUM_BYTES}"
fi;

PREFIX="scheme=sperc-run_date=${DATE}"
INPUT_GENERATE_CT_FLOWS="num_servers=${NUM_SERVERS}-flows_per_server=${FLOWS_PER_SERVER}-epoch=${EPOCH}-frac_replace=${FRAC_REPLACE}-seed1=${SEED1}-seed2=${SEED2}"
INPUT_DRIVE_SPERC_CT="topology_type=${TOPOLOGY_TYPE}-link_rate=${LINK_RATE}-kw=${KW}"
UNIQUE_ID="${PREFIX}-${INPUT_GENERATE_CT_FLOWS}-${INPUT_DRIVE_SPERC_CT}"

ANALYZE_DIR="${PERC_ANALYZE_SCRIPTS}"
RUN_DIR="${PERC_RUN_SCRIPTS}"
COMMON_DIR="${RUN_DIR}/common"


why="Make directory that contains workloads for ns-2 CT experiments"

CT_INPUT_DIR="${PERC_RUN_INSTANCES}/${UNIQUE_ID}/ct_input"

cmd="mkdir -p  $CT_INPUT_DIR"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

#FLOW_FILENAME="$CT_INPUT_DIR/$SUFFIX-flow_file-${NUM_SERVERS}-$FLOWS_PER_SERVER.txt"
#flows_input_base=$(basename "${FLOW_FILENAME}" .tcl)
#echo "flows_input_base is $flows_input_base"

FLOW_FILENAME="$CT_INPUT_DIR/flow_file"
why="Generate workload for CT experiment --num_servers $NUM_SERVERS --flows_per_server $FLOWS_PER_SERVER --start_time ${START_TIME} --epoch ${EPOCH} --num_epochs ${NUM_EPOCHS} --frac_replace ${FRAC_REPLACE} "
cmd="cd $RUN_DIR && python $COMMON_DIR/generate_ct_flows.py --num_servers $NUM_SERVERS --flows_per_server $FLOWS_PER_SERVER --start_time ${START_TIME} --epoch ${EPOCH} --num_epochs ${NUM_EPOCHS} --frac_replace ${FRAC_REPLACE} --num_bytes ${NUM_BYTES} --flow_filename $FLOW_FILENAME --seed1 ${SEED1} --seed2 ${SEED2} && cd -;"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

LOG_QUEUES=0
REMAINDER=$((${SEED1} % 10))
if [ "${REMAINDER}" -eq "0" ]
then 
    if [ "${TOPOLOGY_TYPE}" == "b4" ]; then
	LOG_QUEUES="common/mon3-q.tcl"
    elif [ "${TOPOLOGY_TYPE}" == "spine-leaf" ]; then
	LOG_QUEUES="common/mon2-q.tcl"
    else
	echo "Unrecognized topology type ${TOPOLOGY_TYPE}"
	exit
    fi
fi

why="Run ns-2 simulation of SPERC using workload"
cmd="cd $RUN_DIR && ./drive_sperc_ct.sh $FLOW_FILENAME ${UNIQUE_ID} ${MAX_SIM_TIME} ${NUM_SERVERS} ${TOPOLOGY_TYPE} ${LINK_RATE} ${KW} ${LOG_QUEUES} &&  cd -;"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

why="Create directory to store inter-mediate files while we find convergence times"
CT_TMP_DIR="${PERC_RUN_INSTANCES}/${UNIQUE_ID}/ct_tmp"

#"${RESULTS_DIR}/ct-tmp-files"
cmd="mkdir -p $CT_TMP_DIR"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

# we need to know where drive_sperc_ct.sh -- run_sperc.sh -- ns2 -- is outputting
# keep this in sync with drive_sperc_ct.sh name, make sure it matches
PERC_RUN_INSTANCE="${PERC_RUN_INSTANCES}/${UNIQUE_ID}/run_dir"
why="Check if directory where we drive_sperc_ct.sh runs ./run_sperc.sh exists ${PERC_RUN_INSTANCE}. This is where ns output is."
cmd="ls -d ${PERC_RUN_INSTANCE}"
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why" 1>&2
    exit $success;
fi

why="Get one of the directories where drive_sperc_ct.sh runs"
cmd="PERC_RUN_INSTANCE=$(ls -d ${PERC_RUN_INSTANCE} | head -n 1)"
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why" 1>&2
    exit $success;
fi

# get paths
PATH_FILENAME="${CT_TMP_DIR}/paths.txt"
why="Get path of flows from SPERC simulation"
cd ${PERC_RUN_INSTANCE}
pwd
ls ${PATH_FILENAME}
cmd="grep path *out > ${PATH_FILENAME}"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

# make input for water-filling
why="Get workload for optimal Water-filling solution (same as SPERC input + paths from SPERC sim)"
WF_INPUT_FLOWS_FILENAME="${CT_TMP_DIR}/wf-input.txt"
cmd="cd $RUN_DIR && python ${COMMON_DIR}/gen_input_for_wf.py --flow_filename ${FLOW_FILENAME} --paths_filename ${PATH_FILENAME} --wf_input_filename ${WF_INPUT_FLOWS_FILENAME} 1> ${CT_TMP_DIR}/wf-input-out.txt 2> ${CT_TMP_DIR}/wf-input-err.txt && cd -;"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

cmd="ls ${WF_INPUT_FLOWS_FILENAME}"
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "No input (flows) file for WF generated. Couldn't $why. Check wf-input-err.txt?"
    exit $success;
fi

# Generate links file for Waterfilling
WF_INPUT_LINKS_FILENAME="${CT_TMP_DIR}/wf-input-links.txt"
why="Get links (in terms of node ids) and link capacities SPERC simulation"
cmd="grep queue_code ${PERC_RUN_INSTANCE}/*out | awk '{print \$NF\" \"\$(NF-1)\" \"\$(NF-4);}' >  ${WF_INPUT_LINKS_FILENAME}"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

cmd="ls ${WF_INPUT_LINKS_FILENAME}"
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "No input (links) file for WF generated. Couldn't $why. Check wf-input-err.txt?"
    exit $success;
fi



if [[ -z "${PERC_NUMERICAL}" ]];
then
    echo "env variable PERC_NUMERICAL not defined. source config_env.sh ?"
elif [[ ! -d "${PERC_NUMERICAL}" ]];
then
    echo "${PERC_NUMERICAL} does not exist.";
else
    # get CPG levels
    CPG_LEVELS_FILENAME="${CT_TMP_DIR}/cpg-levels"
    why="Get CPG levels"
    cmd="cd ${PERC_NUMERICAL} && python get_cpg_levels.py --flow_filename ${WF_INPUT_FLOWS_FILENAME} --link_filename ${WF_INPUT_LINKS_FILENAME} 1> ${CPG_LEVELS_FILENAME}.out 2> ${CPG_LEVELS_FILENAME}.err"
    echo $why
    echo $cmd
    eval $cmd
    success=$?

    if [ $success != 0 ]
    then
	echo "Couldn't $why"
	exit $success;
    fi
fi

SECONDS=0
# run water-filling and get optimal rates
why="Run optimal Water-filling to get optimal rates"
OPTIMAL_RATES_FILE="${CT_TMP_DIR}/optimal-rates.txt"
WF_TMP_PREFIX="${CT_TMP_DIR}/wf-tmp"
cmd="${PERC_WATERFILLING_CT_BINARY} ${WF_INPUT_FLOWS_FILENAME} ${WF_TMP_PREFIX}-1.txt ${WF_INPUT_LINKS_FILENAME} 1 1 ${MAX_SIM_TIME} 1> ${WF_TMP_PREFIX}.out 2> ${WF_TMP_PREFIX}.err && grep RATE_CHANGE ${WF_TMP_PREFIX}.out > ${OPTIMAL_RATES_FILE} && cd -;"
#cmd="cd ${WATERFILLING_DIR} && grep RATE_CHANGE ${WF_TMP_PREFIX}.out > ${OPTIMAL_RATES_FILE}; cd -;"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

cmd="ls ${OPTIMAL_RATES_FILE}"
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "No optimal rate file generated. Couldn't $why. Check ${WF_TMP_PREFIX}.err?"
    exit $success;
fi

echo "Waterfilling took $SECONDS seconds"

# get actual rate changes from ns2 
why="Parse ns2-logs to get changes in configured rate for all flows"
# RATE_CHANGES_PREFIX="${CT_TMP_DIR}/rate_changes"
# echo "RATE_CHANGES_PREFIX is ${RATE_CHANGES_PREFIX}"
#cd $RESULTS_DIR; 

# for RCP
# ls agent*aggr*tr | grep -v common | xargs -I_agent_aggr_file -P 20 sh -c "cat _agent_aggr_file | grep RECV_ACK | awk '{if (rate[\$2]==\$8){}else{print \$0;}rate[\$2]=\$8;}' | sort -g -k3,3 > ${RATE_CHANGES_PREFIX}-_agent_aggr_file.txt;"

# for s-PERC
# RECV_ACK fid time(s) ack_type is_exit rtt(us) num_dataPkts_received
# RECV_ACK 3 1.000011 SPERC_CTRL_SYN_REV 0 10.820480 0
# RATE_CHANGE fid time(s) sperc_data(mb/s) rcp_data(mb/s) rcp_ctrl(mb/s)

#eval
# awk '{if (rate[\$2]==\$4){}else{print \$0;}rate[\$2]=\$4;}
# -P 10

RATE_CHANGES_FILE="${CT_TMP_DIR}/actual-rate-changes.txt"
cmd="cd ${PERC_RUN_INSTANCE} && ls agent*aggr*tr | grep -v common | xargs -I_agent_aggr_file cat _agent_aggr_file | grep RATE_CHANGE | grep -v fid |  sort -g -k3,3 > ${RATE_CHANGES_FILE} && cd -"
echo $cmd
eval $cmd
success=$?
if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi


cmd="ls ${RATE_CHANGES_FILE}"
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "No rate change file generated. Couldn't $why"
    exit $success;
fi

NUM_LINES=$(cat ${RATE_CHANGES_FILE} | wc -l)
if [[ $NUM_LINES == 0 ]]; then
    echo "Rate change file is empty. Couldn't $why"
    exit
fi


# why="Collate changes in configured rate for all flows, sorted by time"
# RATE_CHANGES_FILE="${CT_TMP_DIR}/actual-rate-changes.txt"
# cmd="cd ${CT_TMP_DIR} && cat rate_changes-*txt | sort -g -k3,3 > ${RATE_CHANGES_FILE} && cd -;"
# echo ""
# echo $why
# echo $cmd
# eval $cmd
# success=$?

# if [ $success != 0 ]
# then
#     echo "Couldn't $why"
#     exit $success;
# fi

# compare actual and optimal rates and get relative errors per RTT etc.
why="Compare optimal rates with actual rates to get relative errors over time"
COMPARE_FILE_PREFIX="${CT_TMP_DIR}/actual-vs-optimal"
COMPARE_TMP_PREFIX="${CT_TMP_DIR}/cmp-tmp"
cmd="cd $ANALYZE_DIR && python ${COMMON_DIR}/compare_rates.py --optimal_rates_file ${OPTIMAL_RATES_FILE} --rate_changes_file ${RATE_CHANGES_FILE} --start_time ${START_TIME} --epoch ${EPOCH} --num_epochs ${NUM_EPOCHS} --dur ${DUR} --output_prefix=${COMPARE_FILE_PREFIX} 1> ${COMPARE_TMP_PREFIX}-out.txt 2> ${COMPARE_TMP_PREFIX}-err.txt && cd -;"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi


why="Get convergence times from time-series of relative errors in each epoch"

declare -a thresholds=(0.05 0.1 0.2 0.3)
declare -a frac_flows=(0.0 0.01 0.05 0.1 0.2)
echo ""
echo $why
for thresh in "${thresholds[@]}" ; do    
    for frac in "${frac_flows[@]}" ; do    
	CT_RESULTS_FILE="${CT_TMP_DIR}/ct-results-${thresh}-${frac}.txt"
	CT_RESULTS_TMP="${CT_TMP_DIR}/ct-results-${thresh}-${frac}-tmp"

	cmd="cd $ANALYZE_DIR && python $COMMON_DIR/get_convergence_times.py --thresh ${thresh} --frac_flows ${frac} --filename ${COMPARE_FILE_PREFIX}*txt --output_filename ${CT_RESULTS_FILE} 1> ${CT_RESULTS_TMP}.out 2> ${CT_RESULTS_TMP}.err && cd -;"

	echo $cmd
	eval $cmd
	success=$?

	if [ $success != 0 ]
	then
	    echo "Couldn't $why for $thresh $frac"
	    exit $success;
	fi
	echo "output in ${CT_RESULTS_FILE}"
    done
done

DELAY_LINKS_FILENAME="${CT_TMP_DIR}/delay-links.txt"
why="Get links (in terms of node ids) and link delays SPERC simulation" ; # symmetrical links so it's okay nf -> nf-1 instead of other way
cmd="grep queue_code ${PERC_RUN_INSTANCE}/*out | awk '{print \$(NF-1)\" \"\$(NF)\" \"\$(NF-6);}' >  ${DELAY_LINKS_FILENAME}"
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi


RTT_FLOWS_FILENAME="${CT_TMP_DIR}/rtt-flows"
why="Get flow RTTs in  SPERC simulation" ; # symmetrical links so it's okay nf -> nf-1 instead of other way
cmd="python ${COMMON_DIR}/get_flow_rtts.py  --link_delays_filename ${DELAY_LINKS_FILENAME} --paths_filename ${WF_INPUT_FLOWS_FILENAME} --output_filename ${RTT_FLOWS_FILENAME}.txt 1> /dev/null 2> ${RTT_FLOWS_FILENAME}-tmp.err "
echo ""
echo $why
echo $cmd
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

cmd="ls ${RTT_FLOWS_FILENAME}.txt"
eval $cmd
success=$?

if [ $success != 0 ]
then
    echo "No RTT_FLOWS_FILENAME?"
    exit $success;
fi

why="Remove per agent logs"
cmd="cd ${PERC_RUN_INSTANCE} && rm -f agent*aggr*tr  && cd -"
echo $cmd
eval $cmd
success=$?
if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

why="Remove rate changes file"
cmd="rm -f ${RATE_CHANGES_FILE}"
echo $cmd
eval $cmd
success=$?
if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi

why="Remove actual-vs-optimal files"
COMPARE_FILE_PREFIX="${CT_TMP_DIR}/actual-vs-optimal"
cmd="rm -f ${COMPARE_FILE_PREFIX}*"
echo $cmd
eval $cmd
success=$?
if [ $success != 0 ]
then
    echo "Couldn't $why"
    exit $success;
fi
