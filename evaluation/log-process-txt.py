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
    max_bitrate = 3000.0 # Magic Number 
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



def CalAverage(list):
    if len(list) <= 0:
        return 0
    sum = 0
    for i in range(len(list)):
        sum += list[i]
    return sum / len(list)


def ReadThroughputLogs(logs):
    loglist = logs.split(' ')
    avg_thp_list = []
    tail_thp_list = []
    avg_rtt_list = []
    tail_rtt_90_list = []
    tail_rtt_95_list = []
    tail_rtt_99_list = []
    tail_rtt_999_list = []
    avg_thp_client0and1 = []
    avg_thp_others = []

    for i in range(len(loglist)):
        if loglist[i] == 'AvgThroughput=':
            avg_thp_list.append(float(loglist[i+1]))
        if loglist[i] == 'TailThroughput=':
            tail_thp_list.append(float(loglist[i+1]))
        if loglist[i] == 'AvgRtt=':
            avg_rtt_list.append(float(loglist[i+1]))
        if loglist[i] == 'TailRtt90=':
            tail_rtt_90_list.append(float(loglist[i+1]))
        if loglist[i] == 'TailRtt95=':
            tail_rtt_95_list.append(float(loglist[i+1]))
        if loglist[i] == 'TailRtt99=':
            tail_rtt_99_list.append(float(loglist[i+1]))
        if loglist[i] == 'TailRtt999=':
            tail_rtt_999_list.append(float(loglist[i+1]))
        if loglist[i] == 'NodeId=':
            if (int(loglist[i+1]) == 0) or (int(loglist[i+1]) == 1):
                avg_thp_client0and1.append(avg_thp_list[-1])
            else:
                avg_thp_others.append(avg_thp_list[-1])

    return avg_thp_list, tail_thp_list, avg_rtt_list, tail_rtt_90_list, tail_rtt_95_list, tail_rtt_99_list, tail_rtt_999_list, avg_thp_client0and1, avg_thp_others

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





def process_txt_to_csv(logfile_prefix):
    input_files = logfile_prefix + '.txt'
    output_files = logfile_prefix + '-new.csv'
    csv_data = [["policy", " nclient", " seed", " dataset", " serverBtlneck", " avg_thp_client0and1", " avg_thp_otherclients", " min_thp", " tail_thp"]]

    with open(input_files, 'r') as file:
        file_content = file.read()
    modified_content = file_content.replace('\n', ' ')
    modified_content = modified_content.replace(',', ' ')
    loglist = modified_content.split(' ')

    avg_thp_client0and1 = []
    avg_thp_others = []
    avg_thp_list = []
    tail_thp_list = []
    policy = 0
    nclient = 0
    seed = 0
    dataset = 0
    serverbltneck = 0

    for i in range(len(loglist)):
        print('loop:', loglist[i])
        if loglist[i] == 'At' or i == len(loglist)-1:
            if i != 0:
                csv_data.append([
                    f"{policy}", 
                    f" {nclient}", 
                    f" {seed}", 
                    f" {dataset}", 
                    f" {serverbltneck}", 
                    f" {round(CalAverage(avg_thp_client0and1), 2)}", 
                    f" {round(CalAverage(avg_thp_others), 2)}", 
                    f" {round(min(avg_thp_list), 2)}", 
                    f" {round(CalAverage(tail_thp_list), 2)}"
                ])
            avg_thp_client0and1 = []
            avg_thp_others = []
            avg_thp_list = []
            tail_thp_list = []
            policy = 0
            nclient = 0
            seed = 0
            dataset = 0
            serverbltneck = 0
            
        if loglist[i] == 'policy:':
            policy = int(loglist[i+1])
        if loglist[i] == 'nclient:':
            nclient = int(loglist[i+1])
        if loglist[i] == 'seed:':
            seed = int(loglist[i+1])
        if loglist[i] == 'TailThroughput=':
            tail_thp_list.append(float(loglist[i+1]))
        if loglist[i] == 'AvgThroughput=':
            avg_thp_list.append(float(loglist[i+1]))
        if loglist[i] == 'NodeId=':
            if (int(loglist[i+1]) == 0) or (int(loglist[i+1]) == 1):
                avg_thp_client0and1.append(avg_thp_list[-1])
            else:
                avg_thp_others.append(avg_thp_list[-1])
    


    input_csv = logfile_prefix+'.csv'
    with open(input_csv, mode='r') as file:
        csv_content = list(csv.reader(file)) 
        for i in range(len(csv_content)):
            row = csv_content[i]
            if csv_data[i][0] != row[0] or csv_data[i][1] != row[1] or csv_data[i][2] != row[2]:
                print("Error: csv file does not match txt file")
            if csv_data[i][0] != row[0]:
                print(csv_data[i][0], row[0], 'HERE\n')
            csv_data[i][3] = row[3] # dataset
            csv_data[i][4] = row[4] # serverBtlneck
    
    with open(output_files, mode='w') as file:
        writer = csv.writer(file)
        writer.writerows(csv_data)
    exit(0)





def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logfile_prefix', '-lf', type=str, default='')
    parser.add_argument('--log', '-l', type=str, default='')
    parser.add_argument('--process', '-p', action='store_true')
    parser.add_argument('--csv', '-c', type=str, default='')
    parser.add_argument('--avg', '-a', action='store_true')
    parser.add_argument('--avg_client0and1', '-av01', action='store_true')
    parser.add_argument('--avg_otherclients', '-avo', action='store_true')
    parser.add_argument('--tail', '-t', action='store_true')
    parser.add_argument("--min", "-m", action='store_true')
    parser.add_argument("--qoeType", "-q", type=int, default=-1)
    parser.add_argument("--clientInfo", "-i", type=int, default=-1)
    parser.add_argument("--rtt", "-r", action='store_true')
    parser.add_argument("--rtt90", "-r90", action='store_true')
    parser.add_argument("--rtt95", "-r95", action='store_true')
    parser.add_argument("--rtt99", "-r99", action='store_true')
    parser.add_argument("--rtt999", "-r999", action='store_true')

    args = parser.parse_args()

    if not args.process:
        # txt->csv mode
        if args.logfile_prefix != '':
            process_txt_to_csv(args.logfile_prefix)


        avg_thp_list, tail_thp_list, avg_rtt_list, tail_rtt_90_list, tail_rtt_95_list, tail_rtt_99_list, tail_rtt_999_list, avg_thp_client0and1, avg_thp_others = ReadThroughputLogs(args.log)
        if args.avg:
            print("%.2f" % CalAverage(avg_thp_list))
        if args.avg_client0and1:
            print("%.2f" % CalAverage(avg_thp_client0and1))
        if args.avg_otherclients:
            print("%.2f" % CalAverage(avg_thp_others))
        elif args.tail:
            print("%.2f" % CalAverage(tail_thp_list))
        elif args.min:
            print("%.2f" % min(avg_thp_list))
        elif args.qoeType >= 0:
            print("%.2f" % CalQoE(avg_thp_list, args.qoeType))
        elif args.rtt:
            print("%.2f" % CalAverage(avg_rtt_list))
        elif args.rtt90:
            print("%.2f" % CalAverage(tail_rtt_90_list))
        elif args.rtt95:
            print("%.2f" % CalAverage(tail_rtt_95_list))
        elif args.rtt99:
            print("%.2f" % CalAverage(tail_rtt_99_list))
        elif args.rtt999:
            print("%.2f" % CalAverage(tail_rtt_999_list))
        elif args.clientInfo >= 0:
            print("%.2f" % avg_thp_list[args.clientInfo])
    else:
        avg_aggr_results, tail_aggr_results = AggregateCsvLog(args.csv)
        # print(aggr_results)
        PrintAggregatedResults(avg_aggr_results)
        PrintAggregatedResults(tail_aggr_results)


if __name__ == '__main__':
    main()