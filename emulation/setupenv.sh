#!/bin/bash

current_dir=$(pwd)


# ns3 version 3.37
ns3_ver="3.37"
ns3_folder="ns-allinone-${ns3_ver}"

# download ns3 to current dir
if [ ! -d "${current_dir}/${ns3_folder}" ]
then
    ns3_file_name="ns-allinone-${ns3_ver}.tar.bz2"
    url="https://www.nsnam.org/releases/${ns3_file_name}"
    if [ ! -f "${current_dir}/${ns3_file_name}" ]
    then
        echo "Downloading NS3-${ns3_ver}..."
        wget --show-progress --quiet $url
    fi
    # unzip
    echo "Unzipping ${ns3_file_name}..."
    tar xjf ${ns3_file_name}
fi

ns3_src="${current_dir}/${ns3_folder}/ns-${ns3_ver}/src"
ns3_scratch="${current_dir}/${ns3_folder}/ns-${ns3_ver}/scratch"

app_folder="videoconf"
scratch_folder="scratch"

# creat soft link
if [ ! -d "${current_dir}/${app_folder}" ]
then
    echo "${app_folder} does not exist!"
    return
else
    # if soft link already exists, delete it
    if [ -d "${ns3_src}/${app_folder}" ]
    then
        # check out if the var exists
        rm -rf ${ns3_src:?}/${app_folder:?}
    fi
    echo "Linking all files..."
    # linking ./videoconf
    ln -s -v ${current_dir}/${app_folder} ${ns3_src}/${app_folder}
    # linking ./scratch
    ##find -wholename "${current_dir}/scratch/*.cc" -exec ln -s -f -v {} "${ns3_scratch}" \;
    ln -s -v ${current_dir}/${scratch_folder}/* ${ns3_scratch}/
fi

# create folder for pcap
echo "Creating folder for pcap..."
if [ ! -d "${current_dir}/${ns3_folder}/ns-${ns3_ver}/results" ]
then
    mkdir ${current_dir}/${ns3_folder}/ns-${ns3_ver}
fi

# compile (opitonal)
echo "Compiling ns3..."
cd ${current_dir}/${ns3_folder} || return
./build.py
