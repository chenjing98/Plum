#!/bin/bash

export filename_prefix="result_a"


export NS3_VER="3.37"
export CURRENT_DIR=$PWD
export RESULT_DIR=${CURRENT_DIR}/results
export NS3_DIR=$PWD/../emulation/ns-allinone-${NS3_VER}/ns-${NS3_VER}

# compile first
cd $NS3_DIR

echo "policy, nclient, seed, avg_thp, min_thp, tail_thp, qoe" > ${RESULT_DIR}/${filename_prefix}.csv
filename=${RESULT_DIR}/${filename_prefix}
output_file=${filename}.txt
output_file_clean=${filename}.csv
ns3_output=$(<"${CURRENT_DIR}/logs-for-eva/a.log")

avg_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -a)
min_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -m)
tail_thp=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -t)
qoe=$(python3 ${CURRENT_DIR}/log-process.py -l "${ns3_output}" -q)

# output to file
# dirty output
echo At policy: $policy, nclient: $nclient, seed: $seed >> $output_file
echo $ns3_output >> $output_file
# clean output
echo $policy, $nclient, $seed, $avg_thp, $min_thp, $tail_thp, $qoe>> $output_file_clean
