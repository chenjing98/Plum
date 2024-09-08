#!/usr/bin/env python3.9

# Copyright 2023, Gurobi Optimization, LLC

# This example formulates and solves the following simple MIP model:
#  maximize
#        x +   y + 2 z
#  subject to
#        x + 2 y + 3 z <= 4
#        x +   y       >= 1
#        x, y, z binary

import gurobipy as gp
from gurobipy import GRB
import ipyopt
import math

PSNR_MIN_THP = 2.662 # in Kbps


def calc_individual_qoe(dl_bws, Bmax, qoe_type):
    qoes = []
    if qoe_type == "psnr":
        for i in range(len(dl_bws)):
            qoes.append(calc_psnr(dl_bws[i], Bmax))
        qoes = [max(min(qoe, 1), 0) for qoe in qoes]
    else:
        qoes = dl_bws    
    return qoes
            

def calc_psnr(thp, Bmax):
    psnr_max_thp = psnr_thp_mapping(Bmax)
    return psnr_thp_mapping(thp) / psnr_max_thp

def psnr_thp_mapping(thp):
    return 5.1166 * math.log(thp) - 5.0093

def calc_avg_qoe(N, qoes):
    avg_qoe = 0.0
    for i in range(N):
        avg_qoe += qoes[i]
    avg_qoe /= N
    return avg_qoe

def calc_std_dev_qoe(N, avg_qoe, qoes):
    std_dev_qoe = 0.0
    for i in range(len(qoes)):
        std_dev_qoe += (qoes[i] - avg_qoe) ** 2
    std_dev_qoe /= N
    std_dev_qoe = std_dev_qoe ** 0.5
    return std_dev_qoe

def calc_utility(N, Bmax, dl_bws, rho, qoe_type):
    qoes = calc_individual_qoe(dl_bws, Bmax, qoe_type)
    avg_qoe = calc_avg_qoe(N, qoes)
    qoe_fairness = 1 - 2 * calc_std_dev_qoe(N, avg_qoe, qoes)
    return (1 - rho) * avg_qoe + rho * qoe_fairness

def solve_nlp(N, B, Bmax, rho, qoe_type="ssim"):
    # parameters: N users, Bi bandwidth of user i, Bmax maximum bandwidth for the application

    try:
        
        nlp = ipyopt.Problem()

        # Create a new model
        m = gp.Model("bwalloc")
        
        # Create variables
        dl_bws = m.addVars(N, vtype=GRB.CONTINUOUS, name="dl_bw")

        # Set objective
        m.setObjective(calc_utility(N, Bmax, dl_bws, rho, qoe_type), GRB.MAXIMIZE)

        # Add constraint: 
        m.addConstr(dl_bws >= 0, "positive_bw")
        m.addConstr(dl_bws <= B, "abw_per_path")
        m.addConstr(dl_bws * 2 < sum(dl_bws), "uplink_bounding")

        # Optimize model
        m.optimize()

        for v in m.getVars():
            print('%s %g' % (v.VarName, v.X))

        print('Obj: %g' % m.ObjVal)

    except gp.GurobiError as e:
        print('Error code ' + str(e.errno) + ': ' + str(e))

    except AttributeError:
        print('Encountered an attribute error')
        

if __name__ == "__main__":
    N = 4
    B = 10000
    Bmax = 10000
    rho = 0.5
    solve_nlp(N, B, Bmax, rho)