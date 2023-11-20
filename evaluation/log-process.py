import argparse
import csv
import numpy as np
import math

MAX_POLICIES = 2



def qoe(dl_bw, qoeType):
    # lin = 0
    # log = 1
    # sqr_concave = 3
    # sqr_convex = 2
    qoe = 0.0
    max_bitrate = 3000.0 # 待修改：Magic Number 
    alpha = 0
    beta = 0
    if qoeType == 0:
        alpha = 1.0 / max_bitrate
    elif qoeType == 1:
        alpha = 1.0 / math.log(max_bitrate + 1)
    elif qoeType == 3:
        if alpha < 0 and alpha >= - 1.0 / max_bitrate / max_bitrate:
            alpha = alpha
        else:
            alpha = - 1.0 / max_bitrate / max_bitrate
        beta = 1.0 / max_bitrate - alpha * max_bitrate
    elif qoeType == 2:
        if alpha > 0 and alpha < 1 / max_bitrate / (max_bitrate - 2):
            alpha = alpha
        else:
            alpha = 1.0 / max_bitrate / max_bitrate
        beta = 1.0 / max_bitrate - alpha * max_bitrate

    if qoeType == 0:
        qoe = alpha * dl_bw
    elif qoeType == 2 or qoeType == 3:
        qoe = (alpha * dl_bw + beta) * dl_bw
    elif qoeType == 1:
        qoe = alpha * math.log(dl_bw + 1)
    return qoe

def calc_avg_qoe(qoes):
    avg_qoe = 0.0
    if len(qoes) == 0:
        return avg_qoe
    avg_qoe = np.sum(qoes) / len(qoes)
    return avg_qoe

def calc_std_dev_qoe(avg_qoe, qoes):
    std_dev_qoe = 0.0
    if len(qoes) == 0:
        return std_dev_qoe
    for i in range(len(qoes)):
        std_dev_qoe += (qoes[i] - avg_qoe) ** 2
    std_dev_qoe /= len(qoes)
    std_dev_qoe = std_dev_qoe ** 0.5
    return std_dev_qoe

def CalQoE(thplist, qoeType):
    if len(thplist) <= 0:
        return 0
    rho = 0.1
    qoes = []
    for dl_bw in thplist:
        qoes.append(qoe(dl_bw/1000.0, qoeType))
    avg_qoe = calc_avg_qoe(qoes)
    qoe_fairness = 1 - 2 * calc_std_dev_qoe(avg_qoe, qoes)
    return (1 - rho) * avg_qoe + rho * qoe_fairness



def CalAverageThroughput(thplist):
    if len(thplist) <= 0:
        return 0
    sum = 0
    for i in range(len(thplist)):
        sum += thplist[i]
    return sum / len(thplist)


def ReadThroughputLogs(logs):
    loglist = logs.split(' ')
    avg_thp_list = []
    tail_thp_list = []
    for i in range(len(loglist)):
        if loglist[i] == 'AvgThroughput=':
            avg_thp_list.append(float(loglist[i+1]))
        if loglist[i] == 'TailThroughput=':
            tail_thp_list.append(float(loglist[i+1]))
    return avg_thp_list, tail_thp_list

def AggregateCsvLog(csv_file):
    avg_thp_results = {}
    tail_thp_results = {}

    csv_reader = csv.reader(open(csv_file))
    for line in csv_reader:
        if len(line) == 0:
            continue
        if line[0] == "policy":
            continue
        policy = int(line[0])
        nclient = int(line[1])
        key = (policy, nclient)
        if key not in avg_thp_results:
            avg_thp_results[key] = []
        avg_thp_results[key].append(float(line[3]))
        if key not in tail_thp_results:
            tail_thp_results[key] = []
        tail_thp_results[key].append(float(line[4]))

    # print(results)

    avg_thp_aggr_results = {}
    for key in avg_thp_results:
        avg_thp_aggr_results[key] = (
            np.mean(np.array(avg_thp_results[key])), np.std(np.array(avg_thp_results[key])))
    tail_thp_aggr_results = {}
    for key in tail_thp_results:
        tail_thp_aggr_results[key] = (np.mean(
            np.array(tail_thp_results[key])), np.std(np.array(tail_thp_results[key])))

    return avg_thp_aggr_results, tail_thp_aggr_results


def PrintAggregatedResults(aggr_results):
    print("policy nclient avg std")
    # for key in aggr_results:
    #     print("%d %d %.2f %.2f" % (key[0], key[1], aggr_results[key][0], aggr_results[key][1]))

    for key, val in sorted(aggr_results.items()):
        print("%d %d %.2f %.2f" % (key[0], key[1], val[0], val[1]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--log', '-l', type=str, default='')
    parser.add_argument('--process', '-p', action='store_true')
    parser.add_argument('--csv', '-c', type=str, default='')
    parser.add_argument('--avg', '-a', action='store_true')
    parser.add_argument('--tail', '-t', action='store_true')
    parser.add_argument("--min", "-m", action='store_true')
    parser.add_argument("--qoeType", "-q", type=int, default=-1)
    parser.add_argument("--clientInfo", "-i", type=int, default=-1)

    args = parser.parse_args()

    if not args.process:
        avg_thp_list, tail_thp_list = ReadThroughputLogs(args.log)
        if args.avg:
            print("%.2f" % CalAverageThroughput(avg_thp_list))
        elif args.tail:
            print("%.2f" % CalAverageThroughput(tail_thp_list))
        elif args.min:
            print("%.2f" % min(avg_thp_list))
        elif args.qoeType >= 0:
            print("%.2f" % CalQoE(avg_thp_list, args.qoeType))
        elif args.clientInfo >= 0:
            print("%.2f" % avg_thp_list[args.clientInfo])
    else:
        avg_aggr_results, tail_aggr_results = AggregateCsvLog(args.csv)
        # print(aggr_results)
        PrintAggregatedResults(avg_aggr_results)
        PrintAggregatedResults(tail_aggr_results)


if __name__ == '__main__':
    main()
