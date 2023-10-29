import argparse
import csv
import numpy as np

MAX_POLICIES = 2



def qoe(dl_bw):
    qoe = 0.0
    max_bitrate = 10.0
    qoe = dl_bw / max_bitrate
    # if params[2] == "lin":
    #     qoe = params[3] * dl_bw
    # elif params[2] == "sqr_concave" or params[2] == "sqr_convex":
    #     qoe = (params[3] * dl_bw + params[4]) * dl_bw
    # elif params[2] == "log":
    #     qoe = params[3] * math.log(dl_bw + 1)
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

def CalQoE(thplist):
    if len(thplist) <= 0:
        return 0
    rho = 0.5
    qoes = []
    for dl_bw in thplist:
        qoes.append(qoe(dl_bw))
    avg_qoe = calc_avg_qoe(qoes)
    qoe_fairness = 1 - 2 * calc_std_dev_qoe(avg_qoe, qoes)
    return (rho - 1) * avg_qoe - rho * qoe_fairness



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
    parser.add_argument("--qoe", "-q", action='store_true')

    args = parser.parse_args()

    if not args.process:
        avg_thp_list, tail_thp_list = ReadThroughputLogs(args.log)
        if args.avg:
            print("%.2f" % CalAverageThroughput(avg_thp_list))
        elif args.tail:
            print("%.2f" % CalAverageThroughput(tail_thp_list))
        elif args.min:
            print("%.2f" % min(avg_thp_list))
        elif args.qoe:
            print("%.2f" % CalQoE(avg_thp_list))
    else:
        avg_aggr_results, tail_aggr_results = AggregateCsvLog(args.csv)
        # print(aggr_results)
        PrintAggregatedResults(avg_aggr_results)
        PrintAggregatedResults(tail_aggr_results)


if __name__ == '__main__':
    main()
