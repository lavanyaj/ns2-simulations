#!/bin/bash

source parallel.sh

function install {
    echo "function install called with 1: $1"
    ROOT_DIR=`pwd`
    scheme=$1;
    #cmd=" echo fakesetup 1>  ${ROOT_DIR}/install-${scheme}.out 2> ${ROOT_DIR}/install-${scheme}.err"
    cmd=" cd ${ROOT_DIR}/ns2-${scheme}; ./install 1>  ${ROOT_DIR}/install-${scheme}.out 2> ${ROOT_DIR}/install-${scheme}.err"
    echo $cmd
    eval $cmd;
}

function setup {
    # build NS-2 binaries for s-PERC, RCP, and DCTCP (same binary used to run p-Fabric)
    max_cpus=3;
    declare -a schemes=(  "sperc" "rcp" "dctcp" )
    i=0;
    num_schemes=${#schemes[@]};
    job_list=()
    err_str_list=()
    for ((i=0;i<${num_schemes};i++)); do
	scheme=${schemes[i]}
	job_list+=(" ./setup.sh install ${scheme} &")
	err_str_list+=("couldn't install ${scheme}, see  install-${scheme}.err for more info")    
	echo "${job_list[i]}"
	#echo "error str would be ${err_str_list[i]}"
    done;
    read -p "Will use up to ${max_cpus} CPUs. Okay to run? (y/n)" ok
    if [ "${ok}" == "y" ]; then
	run_in_parallel job_list err_str_list ${max_cpus}
    fi
}

echo "1: $1, 2: $2, 3:$3"
if [ "$1" == "install" ] ; then
    echo "installing $2"
    install $2
else
    setup
fi;
