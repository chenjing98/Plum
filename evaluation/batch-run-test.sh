#!/bin/bash

export CORE_COUNT=60

declare -a seeds=(1 2 3 4 5 12946 129 777)
declare -a nclients=(3 4 5)
declare simTime=(1800)
declare -a policies=(0 1)
declare -a ulprops=(0.8)
declare -a ackmaxcounts=(32)

# export FPS=60
# export BITRATE=10  # Mbps
# mtu=1406
# export DATA_PER_FRAME=$(awk 'BEGIN{printf "%d\n", '$BITRATE'*1e6/8/'$FPS'/'$mtu+1'}')

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
    filename=${RESULT_DIR}/result_p2p
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed}" 2>&1)
    avg_thp=$(python3 /home/chenj/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 /home/chenj/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 /home/chenj/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -t)
    # miss_rate=$(awk 'BEGIN{printf "%.010f\n",'$missed_frame_cnt'/'$total_frame_cnt'}')
    # bw_loss_rate=$(awk 'BEGIN{printf "%.04f\n",'$bw_loss'/'$total_send_pkt_cnt'}')
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $avg_thp, $min_thp, $tail_thp >> $output_file_clean
}

run_ns3_tack() {
    policy=$1
    seed=$2
    nclient=$3
    simt=$4
    ackmaxcount=$5
    filename=${RESULT_DIR}/result_p2p_tack
    output_file=${filename}.txt
    output_file_clean=${filename}.csv
    echo At policy: $policy, nclient: $nclient, seed: $seed, output_file: $filename.txt/csv...
    ns3_output=$(NS_GLOBAL_VALUE="RngRun=$seed" ./ns3 run "scratch/test_half_duplex --mode=sfu --logLevel=0 --simTime=${simt} --policy=0 --nClient=${nclient} --varyBw --traceMode=${policy} --seed=${seed} --isTack=1 --tackMaxCount=${ackmaxcount}" 2>&1)
    avg_thp=$(python3 /home/chenj/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -a)
    min_thp=$(python3 /home/chenj/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -m)
    tail_thp=$(python3 /home/chenj/UplinkCoordination/evaluation/log-process.py -l "${ns3_output}" -t)
    # miss_rate=$(awk 'BEGIN{printf "%.010f\n",'$missed_frame_cnt'/'$total_frame_cnt'}')
    # bw_loss_rate=$(awk 'BEGIN{printf "%.04f\n",'$bw_loss'/'$total_send_pkt_cnt'}')
    
    # output to file
    # dirty output
    echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
    echo $ns3_output >> $output_file
    # clean output
    echo $policy, $nclient, $seed, $avg_thp, $min_thp, $tail_thp >> $output_file_clean
}


# compile first
cd $NS3_DIR
./ns3

export -f run_ns3
export -f run_ns3_tack


echo "policy, nclient, seed, avg_thp, min_thp, tail_thp" > ${RESULT_DIR}/result_p2p.csv

parallel -j${CORE_COUNT} run_ns3 ::: ${policies[@]} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${simTime}

echo "policy, nclient, seed, avg_thp, min_thp, tail_thp" > ${RESULT_DIR}/result_p2p_tack.csv

parallel -j${CORE_COUNT} run_ns3_tack ::: ${policies[@]} ::: ${seeds[@]} ::: ${nclients[@]} ::: ${simTime} ::: ${ackmaxcounts[@]}
