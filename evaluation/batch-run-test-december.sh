#!/bin/bash

export CORE_COUNT=50

declare -a seeds=(777)
declare -a nclients=(3)
declare -a simTime=(1000)
declare -a policies=(0 1)
declare -a ulprops=(0.8)
declare -a ackmaxcounts=(16)

export baseline_policy=0
export filename_prefix="result_dec"


export NS3_THROUGHPUT_REGEX="\[VcaClient\]\[Result\] Throughput= ([0-9\.e\-]+)"

export NS3_VER="3.37"
export CURRENT_DIR=$PWD
export RESULT_DIR=${CURRENT_DIR}/results
export NS3_DIR=$PWD/../emulation/ns-allinone-${NS3_VER}/ns-${NS3_VER}

run_ns3() {
    policy=$1
    seed=$2
    nclient=$3
    simt=$4
    filename=${RESULT_DIR}/${filename_prefix}
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed}" 2>&1)
    avg_thp=$(python3 /home/xuzy/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 /home/xuzy/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 /home/xuzy/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -t)
    qoe=$(python3 /home/xuzy/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -q)
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $avg_thp, $min_thp, $tail_thp, $qoe>> $output_file_clean
}

# compile first
cd $NS3_DIR
./ns3

export -f run_ns3


echo "policy, nclient, seed, avg_thp, min_thp, tail_thp, qoe" > ${RESULT_DIR}/${filename_prefix}.csv

parallel -j${CORE_COUNT} run_ns3 ::: ${policies[@]} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${simTime}