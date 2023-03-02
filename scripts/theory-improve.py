# import numpy as np
import os
import csv
import argparse
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd


def generate_init_send_rate(num_client, wireless_bw):
    # wireless_bw: list with length num_client
    # send_rate[i][j] is the send rate from client j to client i (p2p) or client j's video transcoded from the media server (sfu)
    send_rate = []
    for i in range(num_client):
        send_rate_i = []
        for j in range(num_client):
            if i == j:
                send_rate_i.append(0)
            else:
                send_rate_i.append(min(wireless_bw[i], wireless_bw[j]))
        send_rate.append(send_rate_i)
    return send_rate


def enforce_upstream_bound(num_client, ul_send_rate, send_rate):
    for i in range(num_client):
        for j in range(num_client):
            if i == j:
                continue
            if ul_send_rate[j] < send_rate[i][j]:
                send_rate[i][j] = ul_send_rate[j]
    return send_rate


def enforce_abw_bound(num_client, wireless_bw, send_rate, client_id, vca_type):
    if vca_type == "p2p":
        avg_bw = wireless_bw[client_id] / (num_client - 1) / 2
    elif vca_type == "sfu":
        avg_bw = wireless_bw[client_id] / num_client
    else:
        raise Exception("Wrong vca_type")

    for i in range(num_client):
        if i == client_id:
            continue
        if send_rate[i][client_id] > avg_bw:
            send_rate[i][client_id] = avg_bw
        if send_rate[client_id][i] > avg_bw:
            send_rate[client_id][i] = avg_bw

    return send_rate


def cc_up(num_client, wireless_bw, ul_send_rate, send_rate):
    if len(ul_send_rate) == 0:  # p2p
        pass
    elif len(send_rate) >= num_client:  # sfu
        pass
    else:
        raise Exception("[cc_up] Wrong dimension of send_rate")


def cc_down(num_client, wireless_bw, ul_send_rate, send_rate):
    is_valid = True
    if len(ul_send_rate) == 0:  # p2p
        for i in range(num_client):
            col = [row[i] for row in send_rate]
            row = send_rate[i]
            if sum(col) + sum(row) > wireless_bw[i]:
                is_valid = False
                send_rate = enforce_abw_bound(
                    num_client, wireless_bw, send_rate, i, "p2p")
    elif len(send_rate) >= num_client:  # sfu
        send_rate = enforce_upstream_bound(num_client, ul_send_rate, send_rate)
        for i in range(num_client):
            col = [row[i] for row in send_rate]
            row = send_rate[i]
            if sum(col) + sum(row) > wireless_bw[i]:
                is_valid = False
                send_rate = enforce_abw_bound(
                    num_client, wireless_bw, send_rate, i, "sfu")
    else:
        raise Exception("[cc_down] Wrong dimension of send_rate")

    return is_valid, send_rate


def cc_converge(num_client, wireless_bw, ul_send_rate, send_rate):
    if len(ul_send_rate) == 0:  # p2p
        pass
    elif len(send_rate) >= num_client:  # sfu
        pass
    else:
        raise Exception("[cc_converge] Wrong dimension of send_rate")


def p2p(num_client, wireless_bw):
    # wireless_bw: list with length num_client

    if len(wireless_bw) < num_client:
        raise Exception("Not enough wireless bandwidth")
    send_rate = generate_init_send_rate(num_client, wireless_bw)


def sfu(num_client, wireless_bw):
    # wireless_bw: list with length num_client

    if len(wireless_bw) < num_client:
        raise Exception("Not enough wireless bandwidth")

    # ul_send_rate[i] is the send rate from client i to the media server
    ul_send_rate = []
    for i in range(num_client):
        ul_send_rate.append(wireless_bw[i])

    send_rate = generate_init_send_rate(num_client, wireless_bw)


def Coordinated(num_client, wireless_bw):
    if len(wireless_bw) < num_client:
        raise Exception("Not enough wireless bandwidth")


def p2p_static_single_bottleneck(num_client, bottleneck_bw, app_bw):
    playback_bw = []
    if bottleneck_bw < num_client * app_bw:
        playback_bw.append(min(bottleneck_bw / 2 / (num_client - 1), app_bw))
        for _ in range(num_client - 1):
            playback_bw.append(min(bottleneck_bw / 2, app_bw))
    else:  # no congestion
        for _ in range(num_client):
            playback_bw.append(app_bw)

    return playback_bw


def coordinated_static_single_bottleneck(num_client, bottleneck_bw, app_bw, k):
    playback_bw = []
    if bottleneck_bw < num_client * app_bw:
        # (app_bw + bottleneck_bw - n * app_bw) / bottleneck_bw <= k <= app_bw / bottleneck_bw
        if k > app_bw / bottleneck_bw:
            k = app_bw / bottleneck_bw
        playback_bw.append(
            min((1 - k) * bottleneck_bw / (num_client - 1), app_bw))
        for _ in range(num_client - 1):
            playback_bw.append(min(k * bottleneck_bw, app_bw))
    else:  # no congestion
        for _ in range(num_client):
            playback_bw.append(app_bw)

    return playback_bw


def main():
    parser = argparse.ArgumentParser(description='Theory Improve')
    parser.add_argument('-n', '--num_client', type=int,
                        default=3, help='Number of clients')
    parser.add_argument('-b', '--bottleneck_bw', type=float,
                        default=10, help='Bottleneck bandwidth')
    parser.add_argument('-a', '--app_bw', type=float,
                        default=100, help='Application bandwidth')
    parser.add_argument('-k', type=float, default=0.5, help='k')
    parser.add_argument
    args = parser.parse_args()

    trace_folder = "./traces/wifi/"

    num_client = args.num_client
    app_bw = args.app_bw
    k = args.k

    bottleneck_bws = []
    baseline_bws = []
    coordinated_bws = []
    bottleneck_labels = []
    baseline_labels = []
    coordinated_labels = []

    for file in os.listdir(trace_folder):
        first_line = True
        with open(os.path.join(trace_folder, file)) as f:
            for row in csv.reader(f):
                if first_line:
                    first_line = False
                    continue
                bottleneck_bw = float(row[2])
                for _ in range(num_client):
                    bottleneck_bws.append(bottleneck_bw)
                    bottleneck_labels.append("bottleneck")
                    baseline_labels.append("vanilla")
                    coordinated_labels.append("coordinated")
                playback_bw_baseline = p2p_static_single_bottleneck(
                    num_client, bottleneck_bw, app_bw)
                playback_bw_coordinated = coordinated_static_single_bottleneck(
                    num_client, bottleneck_bw, app_bw, k)
                baseline_bws += playback_bw_baseline
                coordinated_bws += playback_bw_coordinated

    dataframe = pd.DataFrame({
        'rate': bottleneck_bws + baseline_bws + coordinated_bws,
        'label': bottleneck_labels + baseline_labels + coordinated_labels})

    sns.set_theme(context='notebook', style="whitegrid", font_scale=1.5)
    dist_fig = sns.displot(x="rate", hue="label", kind='ecdf', data=dataframe)
    # dist_fig.fig.set_figwidth(7)
    # dist_fig.fig.set_figheight(4)
    plt.xlabel("Bitrate (Mbps)")
    plt.ylabel("CDF")
    plt.xlim(-5, 35)
    plt.legend(loc='lower left')
    plt.show()
    plt.tight_layout()
    save_filename = "./dist_n" + \
        str(num_client) + "_a" + str(app_bw) + "_k" + str(k) + ".png"
    plt.savefig(save_filename, bbox_inches="tight", pad_inches=0.0)


if __name__ == "__main__":
    main()
