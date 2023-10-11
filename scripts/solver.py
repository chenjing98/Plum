#!/usr/bin/env python3.9

import argparse
import math
import numpy as np
import scipy.optimize as opt
import matplotlib.pyplot as plt


def qoe(dl_bw, params):
    qoe = 0.0
    if params[2] == "lin":
        qoe = params[3] * dl_bw
    elif params[2] == "sqr":
        qoe = (params[3] * dl_bw + params[4]) * dl_bw
    elif params[2] == "log":
        qoe = params[3] * math.log(dl_bw + 1)

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

def utility(x, args):
    rho = args[0]
    qoes = []
    for dl_bw in x:
        qoes.append(qoe(dl_bw, args))
    avg_qoe = calc_avg_qoe(qoes)
    qoe_fairness = 1 - 2 * calc_std_dev_qoe(avg_qoe, qoes)
    return (rho - 1) * avg_qoe - rho * qoe_fairness

def solve(N, B, params):
    # parameters: N users, Bi bandwidth of user i, Bmax maximum bandwidth for the application
    # parameters: params = [rho, Bmax, QoE type, alpha, beta, v]

    # S = np.sum(B) - np.max(B)
    B_ul_max = params[1] / params[5]
    
    # Constraints
    # cons = [{'type': 'ineq', 'fun': lambda x: S - np.sum(x)}]
    cons = [{'type': 'ineq', 'fun': lambda x: np.sum(np.minimum(B - x, B_ul_max * np.ones(N))) - np.max(np.minimum(B, B_ul_max * np.ones(N) + x))}]
    # for i in range(N):
    #     cons.append({'type': 'ineq', 'fun': lambda x:  x[i]})
    #     cons.append({'type': 'ineq', 'fun': lambda x:  min(B[i], params[1]) - x[i]})
        # cons.append({'type': 'ineq', 'fun': lambda x:  S / N - x[i]})
    
    bnds = []
    for i in range(N):
        bnds.append((0, min(B[i], params[1])))
    
    x = []
    
    res = opt.minimize(utility, x0=np.ones(N) * params[7], constraints=tuple(cons), bounds=tuple(bnds), callback=x.append, args=(params), method=params[6])

    print(res.x)
    
    if(params[8]):
        plt.plot(x[:], [utility(x[i], params) for i in range(len(x))], 'o-')
        plt.savefig('opt-process.png')


def main():
    
    qoeFuncAlpha = 1.0
    qoeFuncBeta = 0.0
    numView = 25
    init_bw = 100.0
    
    parser = argparse.ArgumentParser()
    parser.add_argument('-n', '--N', type=int, default=3, help='number of users')
    parser.add_argument('-b', '--Bandwidth', nargs='+', default=[10,10,10], help='available bandwidth for each users path')
    parser.add_argument('-r', '--Rho', type=float, default=0.5, help='rho in the utility function')
    parser.add_argument('-m', '--MaxBitrate', type=float, default=10.0, help='maximum bitrate for the application')
    parser.add_argument('-q', '--QoeType', type=str, default='lin', help='type of the QoE function, to choose from [''lin'', ''log'', ''sqr-concave'', ''sqr-convex'']')
    parser.add_argument('-a', '--QoeFuncAlpha', type=float, default=0, help='alpha in the QoE function')
    parser.add_argument('-v', '--ViewNum', type=float, default=25, help='number of views')
    parser.add_argument('-p', '--Plot', action='store_true', default=False, help='plot the utility function')
    parser.add_argument('-i', '--InitBandwidth', type=float, default=20.0, help='initial bandwidth for the optimization')
    parser.add_argument('-t', '--Method', type=str, default='SLSQP', help='method to solve the optimization problem')
    args = parser.parse_args()
    
    if args.N < 3:
        print('N must be greater than 2')
        return
    if len(args.Bandwidth) != args.N:
        print('Number of bandwidths must be equal to N')
        return
    if args.Rho <= 0 or args.Rho >= 1:
        print('Rho must be in (0,1)')
        return
    if args.ViewNum < 2:
        print('ViewNum must be greater than 1')
        return
    
    
    if args.MaxBitrate <= 0:
        print('MaxBitrate must be positive')
        return
    if args.QoeType not in ['lin', 'log', 'sqr-concave', 'sqr-convex']:
        print('QoeType must be one of ''lin'', ''log'', ''sqr-concave'', ''sqr-convex''')
        return
    if args.QoeType == 'lin':
        qoeFuncAlpha = 1.0 / args.MaxBitrate
    elif args.QoeType == 'log':
        qoeFuncAlpha = 1.0 / math.log(args.MaxBitrate + 1)
    elif args.QoeType == 'sqr-concave':
        if args.QoeFuncAlpha < 0 and args.QoeFuncAlpha >= - 1.0 / args.MaxBitrate / args.MaxBitrate:
            qoeFuncAlpha = args.QoeFuncAlpha
        else:
            qoeFuncAlpha = - 1.0 / args.MaxBitrate / args.MaxBitrate
        qoeFuncBeta = 1.0 / args.MaxBitrate - qoeFuncAlpha * args.MaxBitrate
    elif args.QoeType == 'sqr-convex':
        if args.QoeFuncAlpha > 0 and args.QoeFuncAlpha < 1 / args.MaxBitrate / (args.MaxBitrate - 2):
            qoeFuncAlpha = 1.0 / args.MaxBitrate / args.MaxBitrate
        qoeFuncBeta = 1.0 / args.MaxBitrate - qoeFuncAlpha * args.MaxBitrate
        
    if args.ViewNum > args.N:
        numView = args.N
    else:
        numView = args.ViewNum
        
    if args.Method not in ['SLSQP', 'COBYLA']:
        print('Method must be one of ''SLSQP'', ''COBYLA''')
        return
    
    params = [args.Rho, args.MaxBitrate, args.QoeType, qoeFuncAlpha, qoeFuncBeta, numView, args.Method, args.InitBandwidth, args.Plot]
    
    capacities = np.array(args.Bandwidth)
    capacities = capacities.astype(float)
    solve(args.N, capacities, params)


if __name__ == "__main__":
    main()