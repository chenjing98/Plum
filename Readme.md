# "Bidirectional Bandwidth Coordination under Half-Duplex Bottlenecks for Video Streaming"

This repository contains the artifacts for our ICNP'24 paper titled "Bidirectional Bandwidth Coordination under Half-Duplex Bottlenecks for Video Streaming".

**Table of Contents:**

*  [Prerequisites and Installation Instructions](#prerequisites-and-installation-instructions)
*  [Artifacts Overview](#artifacts-overview)
    * [Theoretical Analysis](#theoretical-analysis)
    * [NS-3 Experiments](#ns-3-experiments)
    * [Android Program](#android-program)
*  [Contact](#contact) 


## Prerequisites and Installation Instructions
The artifacts software is developed using C++ and Python3. 
**This software requires Ubuntu 20.04.4 LTS, as it has been tested on this platform.**
Please ensure the following dependencies are installed on your system:

#### Step 1: Install Basic Packages
Open a terminal and execute the following commands to install `cmake`, `g++` and `python3`:
```bash
sudo apt update
sudo apt install cmake g++ python3
```

#### Step 2: Clone this repository and setup NS-3 environment

```bash
cd emulation
bash ./setupenv.sh
```

This script would automatically download the NS 3.37 package, link our module and scratch files, and compile.


## Artifacts Overview

The artifacts are divided into three main sections: theoretical analysis, NS-3 Experiments and an Android program.

The [Theoretical Analysis](#theoretical-analysis) part provides a theoretical estimation for the improvement of uneven bidirectional bandwidth (Figure 4).

The [NS-3 Experiments](#ns-3-experiments) part provides the simulation code in our evaluation section (all figures in Section IV-A, IV-B, IV-C, IV-D).

The [Android Program](#android-program) part provides our program for our real-time traffic tracker (Figure 6).

### Theoretical Analysis

For running this code, please install `seaborn` and `pandas` beforehand to plot the result:

```bash
pip install seaborn pandas
pip install --upgrade seaborn
```

The theoretical analysis uses Tr-Game traces. Although this traceset could not be opensourced temporarily, you might use this script on any other traceset to estimate the performance improvement. The format of Tr-Game traceset is a .csv file with bandwidth (in Mbps) in its third row.

To run the script, please run the following the commands:

```bash
cd scripts
python theory-improve -n ${Number-of-clients} -b ${Bottleneck-bandwidth-in-Mbps} -a ${Application-maximum-bandwidth-in-Mbps} -k ${uplink-bandwidth/total-bandwidth} -f ${Directory-to-traceset}
```

The resulted figure would be saved under `scripts/` directory.

### NS-3 Experiments

Our NS-3 codes mainly include a new module `videoconf` (which simulates the server and client for video streaming application under [WebRTC SFU architecture](https://getstream.io/blog/what-is-a-selective-forwarding-unit-in-webrtc/)) and scratch files (to run tests). 

To reproduce our results, please go the `evaluation/` folder and run our bash scripts:

```bash
cd evaluation
mkdir results # the session-level statistics would be saved here
mkdir results/trlogs # the real-time bitrate (collected every second) would be saved here

bash batch-run-test-trace-driven.sh # this include the experiments in Figure 11-13 (for Plum, Vanilla and TACK)
bash batch-run-test-channel-model.sh # this include the experiments in Figure 14
```

Before running the experiment for Figure 14, please keep our solver running in the first place. The solver uses SLSQP algorithm to solve the target bidirectional bandwidth allocations (see more details in Section III-C in our paper). The VcaServer in our videoconf module would later uses Linux IPC sockets to get these solutions.

```bash
cd scripts/solver
python solver.py -n ${Number-of-Clients}
``` 

To collect the bitrate distribution (Figure 12, Figure 14(a)), please uncomment the code line  `vcaClientApp->SetLogFile(...` in the scratch files (which are under `emulation/scratch/`).

To reproduce the results for policy LastN, please checkout to another branch `lastn`. We separate the code of this baseline policy, as it involves different modifications to NS-3 original modules.


### Android Program

For the Android program that we use to get Figure 6, please refer to the `AppTrafficRealtimeTracker/` directory. The program requires several user permissions to invoke the `NetworkStatsManager` API (please see them in `AppTrafficRealtimeTracker/app/src/main/AndroidManifest.xml`). You might also use our Python script `scripts/traffic-tracker-processor.py` to process the collected data.

*Note: We debugged the program with a Google Pixel 3 smartphone with root privilege, but running the program do not require root privilege.*

## Contact
For any questions or issues, please contact Jing Chen at (`j-c23 (at) mails.tsinghua.edu.cn`).