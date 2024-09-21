#!/bin/bash

export CORE_COUNT=20

declare -a seeds=(777 42 55 6 7 20 84 234 1000 81)
declare -a nclients=(3)
declare -a simTime=(1200)
declare -a policies=(0 2)
declare -a qoeType=(2)
declare -a ulprops=(0.8)
declare -a ackmaxcounts=(16)

export baseline_policy=0
export baseline_qoe_type=0
export filename_prefix="result_channel_model"


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
    qoet=$5
    filename=${RESULT_DIR}/${filename_prefix}
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, qoeType: $qoet, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_wifi_channel --mode=sfu --logLevel=0 --simTime=${simt} --policy=${policy} --nClient=${nclient} --qoeType=${qoet}" 2>&1)
    avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
    qoe=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -q ${qoet})
    avg_rtt=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -r)
    rtt90=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -r90)
    rtt95=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -r95)
    rtt99=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -r99)
    rtt999=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -r999)
    
    # ====== output to file =======
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed, qoeType: $qoet >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $qoet, $avg_thp, $min_thp, $tail_thp, $qoe, $avg_rtt, $rtt90, $rtt95, $rtt99, $rtt999>> $output_file_clean
}


# compile first
cd $NS3_DIR
./ns3

export -f run_ns3


echo "policy, nclient, seed, qoeType, avg_thp, min_thp, tail_thp, qoe, avg_rtt, rtt90, rtt95, rtt99, rtt999" > ${RESULT_DIR}/${filename_prefix}.csv

parallel -j${CORE_COUNT} run_ns3 ::: ${policies[@]} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${simTime} ::: ${qoeType[@]}
