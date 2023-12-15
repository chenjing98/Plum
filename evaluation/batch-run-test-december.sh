#!/bin/bash

export CORE_COUNT=3

declare -a seeds=(777 42 55 6 7 20 84 234 1000 81)
# declare -a seeds=(1 2 3 4 5 8 9 10 11 12)
declare -a nclients=(3)
declare -a simTime=(1200)
declare -a policies=(2)
declare -a qoeType=(0 1 2 3)
declare -a ulprops=(0.8)
declare -a ackmaxcounts=(16)
declare -a rhos=(0.5)
declare -a wifiversions=(6)
declare -a topotypes=(0 1)
declare -a mobilitytypes=(0)

export baseline_policy=0
export filename_prefix="raw_result_indoor_80211ac_otherseed"


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
    rho=$6
    wifiversion=$7
    topotype=$8
    mobilitytype=$9
    filename=${RESULT_DIR}/${filename_prefix}
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, qoeType: $qoet, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_wifi_channel --mode=sfu --logLevel=0 --simTime=${simt} --policy=${policy} --nClient=${nclient} --qoeType=${qoet} --rho=${rho} --vWifi=${wifiversion} --topoType=${topotype} --mobiModel=${mobilitytype}" 2>&1)
    avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
    qoe=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -q ${qoet} -r ${rho})
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed, qoeType: $qoet, rho: $rho, wifiVersion: $wifiversion, topoType: $topotype, mobilityType: $mobilitytype >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $qoet, $rho, $wifiversion, $topotype, $mobilitytype, $avg_thp, $min_thp, $tail_thp, $qoe>> $output_file_clean
}

run_ns3_baseline() {
    policy=$baseline_policy
    seed=$1
    nclient=$2
    simt=$3
    rho=$4
    wifiversion=$5
    topotype=$6
    mobilitytype=$7
    filename=${RESULT_DIR}/${filename_prefix}_baseline
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, qoeType: $qoet, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_wifi_channel --mode=sfu --logLevel=0 --simTime=${simt} --policy=${policy} --nClient=${nclient} --qoeType=0 --rho=${rho} --vWifi=${wifiversion} --topoType=${topotype} --mobiModel=${mobilitytype}" 2>&1)
    avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
    qoe=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -q ${qoet} -r ${rho})
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed, qoeType: 0, rho: $rho, wifiVersion: $wifiversion, topoType: $topotype, mobilityType: $mobilitytype >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, 0, $rho, $wifiversion, $topotype, $mobilitytype, $avg_thp, $min_thp, $tail_thp, $qoe>> $output_file_clean
}

# compile first
cd $NS3_DIR
./ns3

export -f run_ns3
export -f run_ns3_baseline


echo "policy, nclient, seed, qoeType, rho, wifi, topo, mobility, avg_thp, min_thp, tail_thp, qoe" > ${RESULT_DIR}/${filename_prefix}.csv

parallel -j${CORE_COUNT} run_ns3 ::: ${policies[@]} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${simTime} ::: ${qoeType[@]} ::: ${rhos[@]} ::: ${wifiversions[@]} ::: ${topotypes[@]} ::: ${mobilitytypes[@]}

echo "policy, nclient, seed, qoeType, rho, wifi, topo, mobility, avg_thp, min_thp, tail_thp, qoe" > ${RESULT_DIR}/${filename_prefix}_baseline.csv

parallel -j${CORE_COUNT} run_ns3_baseline ::: ${seeds[@]} ::: ${nclients[@]} ::: ${simTime} ::: ${rhos[@]} ::: ${wifiversions[@]} ::: ${topotypes[@]} ::: ${mobilitytypes[@]}
