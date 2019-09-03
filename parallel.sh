
# parallel script that takes job list, err str list and runs in parallel using up to max_cpus
function run_in_parallel() {
    # names in caller function should be different from _job_list and _err_str_list
    # otherwise there will be a circular reference problem when calling this function
    local -n _job_list=$1;
    local -n _err_str_list=$2;    
    max_cpus=$3;

    runs=0;
    cpus=0;
    pids=();
    num_runs=${#_job_list[@]};
    finished_runs=0;
    declare -A pid_to_run
    declare -A pid_to_start

    while [ ${finished_runs} -lt ${num_runs} ]; do

	while [ $cpus -lt ${max_cpus} -a $runs -lt ${num_runs} ]; do
	    cmd="${_job_list[runs]}"
	    echo $cmd
	    eval $cmd
	    pid=$!
	    pids+=($pid)
	    pid_to_run[${pid}]=$runs
	    pid_to_start[${pid}]=$SECONDS

	    echo "Started job ${runs} (pid ${pid}), cpus ${cpus}/${max_cpus}, runs ${runs}/${num_runs}."
	    runs=$((runs+1));
	    cpus=$((cpus+1));
	done;
	# echo "pid_to_run keys ${!pid_to_run[@]}"
	# echo "pid_to_run values ${pid_to_run[@]}"
	# echo "pids array ${pids} .. has ${#pids[@]} values"
	pids_str=""
	for ((i=0;i<${#pids[@]};i++)); do
	    pids_str="${pids_str} ${pids[i]}"
	done;    
	cmd="wait -n ${pids_str}"
	# echo "$cmd"
	eval $cmd
	# status of exiting job
	success=$?
	finished_runs=$((finished_runs+1));
	# echo "update the set of running pids"
	# recalculate set of running pids, num cpus in use
	# and find which job just exited to print status
	cpus=0
	new_pids=()
	for ((i=0;i<${#pids[@]};i++)); do
	    pid=${pids[i]}
	    index=${pid_to_run[${pid}]};
	    # echo "check if $pid : $index is running"
	    if [ -n "$pid" -a -e "/proc/$pid" ]; then
		cpus=$((cpus+1))
		new_pids+=($pid) #"${running_pids} ${pids[i]}"
		# echo "add it to list of running pids, which now has ${#new_pids[@]} values"
	    else		
		start=pid_to_start[${pid}]
		duration=$((SECONDS-start))
		unset pid_to_run[${pid}]
		unset pid_to_start[${pid}]
		if [ $success != 0 ]
		then	
		    err_str=${_err_str_list[index]}
		    echo "Job ${index} (pid ${pid}) exited, after running for ${duration} s, with code ${successs}:"
		    echo "${err_str}" 
		else
		    echo "Job ${index} (pid ${pid}) exited, after running for ${duration} s, successfully with code ${success}."
		fi
	    fi
	done;
	# echo "resetting pids from ${pids} with size ${#pids[@]} to ${new_pids} with size ${#new_pids[@]}"
	pids=(${new_pids[@]}) #("${running_pids[@]}")
	# echo "new value of pids is ${pids} with size ${#pids[@]}"
    done;
}
