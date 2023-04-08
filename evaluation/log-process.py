import argparse
import csv
import numpy as np

MAX_POLICIES = 2

def CalAverageThroughput(thplist):
    if len(thplist) <= 0:
        return 0
    sum = 0
    for i in range(len(thplist)):
        sum += thplist[i]
    return sum / len(thplist)


def ReadThroughputLogs(logs):
    loglist = logs.split(' ')
    thplist = []
    for i in range(len(loglist)):
        if loglist[i] == 'Throughput=':
            thplist.append(float(loglist[i+1]))
    return thplist

def AggregateCsvLog(csv_file):
    results = {}
    
    csv_reader = csv.reader(open(csv_file))
    for line in csv_reader:
        if len(line) == 0:
            continue
        if line[0] == "policy":
            continue
        policy = int(line[0])
        nclient = int(line[1])
        key = (policy, nclient)
        if key not in results:
            results[key] = []
        results[key].append(float(line[3]))
    
    # print(ressults)
    
    aggr_results = {}
    for key in results:
        aggr_results[key] = (np.mean(np.array(results[key])), np.std(np.array(results[key])))
        
    return aggr_results

def PrintAggregatedResults(aggr_results):
    print("policy nclient thp_avg thp_std")
    for key in aggr_results:
        print("%d %d %.2f %.2f" % (key[0], key[1], aggr_results[key][0], aggr_results[key][1]))
        

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--log', '-l', type=str, default='')
    parser.add_argument('--process', '-p', action='store_true')
    parser.add_argument('--csv', '-c', type=str, default='')
    args = parser.parse_args()

    if not args.process:
        thplist = ReadThroughputLogs(args.log)

        print("%.2f" % CalAverageThroughput(thplist))
    else:
        aggr_results = AggregateCsvLog(args.csv)
        print(aggr_results)
        PrintAggregatedResults(aggr_results)

if __name__ == '__main__':
    main()
