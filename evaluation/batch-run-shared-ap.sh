#!/bin/bash

export CORE_COUNT=18

# declare -a seeds=(14 20 25 27 30)
declare -a seeds=(1 2 3 4 5 129 777 12946)
declare -a nclients=(3 4 5 8 10 12 14 16 18 20)
declare -a serverBtlneck=(1000)
declare simTime=(120)
declare -a policies=(0)
declare -a ulprops=(0.8)
declare -a ackmaxcounts=(16)
declare -a datasets=(0)

export baseline_policy=0
export filename_prefix="result_fig11_shareap_server1000_game_last_01other"

export NS3_VER="3.37"
export CURRENT_DIR=$PWD
export RESULT_DIR=${CURRENT_DIR}/results
export NS3_DIR=$PWD/../emulation/ns-allinone-${NS3_VER}/ns-${NS3_VER}

run_ns3_share_ap() {
    policy=$1
    seed=$2
    nclient=$3
    dataset=$4
    serverbw=$5
    simt=$6
    filename=${RESULT_DIR}/${filename_prefix}
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex_w_share_ap --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed} --dataset=${dataset} --serverBtl=${serverbw}" 2>&1)
    # avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
    avg_thp_client0and1=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -av01)
    avg_thp_otherclients=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -avo)
    min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
    # miss_rate=$(awk 'BEGIN{printf "%.010f\n",'$missed_frame_cnt'/'$total_frame_cnt'}')
    # bw_loss_rate=$(awk 'BEGIN{printf "%.04f\n",'$bw_loss'/'$total_send_pkt_cnt'}')

    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $dataset, $serverbw, $avg_thp_client0and1, $avg_thp_otherclients, $min_thp, $tail_thp >> $output_file_clean
}

# compile first
cd $NS3_DIR
./ns3

export -f run_ns3_share_ap

echo "policy, nclient, seed, dataset, serverBtlneck, avg_thp_client0and1, avg_thp_otherclients, min_thp, tail_thp" > ${RESULT_DIR}/${filename_prefix}.csv

parallel -j${CORE_COUNT} run_ns3_share_ap ::: ${baseline_policy} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${datasets[@]} ::: ${serverBtlneck[@]} ::: ${simTime} ::: ${ackmaxcounts[@]}