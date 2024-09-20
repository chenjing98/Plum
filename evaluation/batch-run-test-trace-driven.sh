#!/bin/bash

export CORE_COUNT=20

# declare -a seeds=(14 20 25 27 30)
declare -a seeds=(1 2 3 4 5 129 777 12946)
declare -a nclients=(3 4 5 8 10 12 14 16 18 20)
declare -a serverBtlneck=(100 300 500 1000)
declare simTime=(120)
declare -a policies=(0)
declare -a ulprops=(0.8)
declare -a ackmaxcounts=(16)
declare -a datasets=(0)

export baseline_policy=0
export filename_prefix="result_trace_driven"


export NS3_THROUGHPUT_REGEX="\[VcaClient\]\[Result\] Throughput= ([0-9\.e\-]+)"

export NS3_VER="3.37"
export CURRENT_DIR=$PWD
export RESULT_DIR=${CURRENT_DIR}/results
export NS3_DIR=$PWD/../emulation/ns-allinone-${NS3_VER}/ns-${NS3_VER}

run_ns3_independent() {
    policy=$1
    seed=$2
    nclient=$3
    dataset=$4
    serverbw=$5
    simt=$6
    filename=${RESULT_DIR}/${filename_prefix}_independent
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed} --dataset=${dataset} --serverBtl=${serverbw}" 2>&1)
    avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $dataset, $serverbw, $avg_thp, $min_thp, $tail_thp >> $output_file_clean
}

run_ns3_independent_tack() {
    policy=$1
    seed=$2
    nclient=$3
    dataset=$4
    serverbw=$5
    simt=$6
    ackmaxcount=$7
    filename=${RESULT_DIR}/${filename_prefix}_independent_tack
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed} --isTack=1 --tackMaxCount=${ackmaxcount}  --dataset=${dataset} --serverBtl=${serverbw}" 2>&1)
    avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $dataset, $serverbw, $avg_thp, $min_thp, $tail_thp >> $output_file_clean
}

run_ns3_share_ap() {
    policy=$1
    seed=$2
    nclient=$3
    dataset=$4
    serverbw=$5
    simt=$6
    filename=${RESULT_DIR}/${filename_prefix}_share_ap
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex_w_share_ap --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed} --dataset=${dataset} --serverBtl=${serverbw}" 2>&1)
    avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)

    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $dataset, $serverbw, $avg_thp, $min_thp, $tail_thp >> $output_file_clean
}

run_ns3_share_ap_tack() {
    policy=$1
    seed=$2
    nclient=$3
    dataset=$4
    serverbw=$5
    simt=$6
    ackmaxcount=$7
    filename=${RESULT_DIR}/${filename_prefix}_share_ap_tack
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex_w_share_ap --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed} --isTack=1 --tackMaxCount=${ackmaxcount}  --dataset=${dataset} --serverBtl=${serverbw}" 2>&1)
    avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $dataset, $serverbw, $avg_thp, $min_thp, $tail_thp >> $output_file_clean
}


# compile first
cd $NS3_DIR
./ns3

export -f run_ns3_independent
export -f run_ns3_independent_tack
export -f run_ns3_share_ap
export -f run_ns3_share_ap_tack


# ======= Independent Topology =======
#      ======= Vanilla & Plum ========
echo "policy, nclient, seed, dataset, serverBtlneck, avg_thp, min_thp, tail_thp" > ${RESULT_DIR}/${filename_prefix}_independent.csv

parallel -j${CORE_COUNT} run_ns3_independent ::: ${policies[@]} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${datasets[@]} ::: ${serverBtlneck[@]} ::: ${simTime}

#      ============= TACK ============
echo "policy, nclient, seed, dataset, serverBtlneck, avg_thp, min_thp, tail_thp" > ${RESULT_DIR}/${filename_prefix}_independent_tack.csv

parallel -j${CORE_COUNT} run_ns3_independent_tack ::: ${baseline_policy} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${datasets[@]} ::: ${serverBtlneck[@]} ::: ${simTime} ::: ${ackmaxcounts[@]}


# ==== Two clients sharing AP Topology ====
#      ========== Vanilla & Plum ==========
echo "policy, nclient, seed, dataset, serverBtlneck, avg_thp, min_thp, tail_thp" > ${RESULT_DIR}/${filename_prefix}_share_ap.csv

parallel -j${CORE_COUNT} run_ns3_share_ap ::: ${baseline_policy} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${datasets[@]} ::: ${serverBtlneck[@]} ::: ${simTime} ::: ${ackmaxcounts[@]}

#      ============= TACK ============
echo "policy, nclient, seed, dataset, serverBtlneck, avg_thp, min_thp, tail_thp" > ${RESULT_DIR}/${filename_prefix}_share_ap_tack.csv

parallel -j${CORE_COUNT} run_ns3_share_ap_tack ::: ${baseline_policy} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${datasets[@]} ::: ${serverBtlneck[@]} ::: ${simTime} ::: ${ackmaxcounts[@]}