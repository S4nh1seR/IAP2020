#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
from matplotlib import pyplot as plt
import numpy as np
import re
import argparse

argparser = argparse.ArgumentParser()
argparser.add_argument('--results_path', '-rp', type=str, required=True)

def main():
    args = argparser.parse_args()
    results_dir = args.results_path

    fig = plt.figure()
    res_names = os.listdir(results_dir)
    res_names = list(filter(lambda s: not s.startswith("."), res_names))
    for res in res_names:
        path = os.path.join(results_dir, res)
        txt_ratio_log_name = os.path.join(path, 'inliers_ratio_per_frame.txt')
        with open(txt_ratio_log_name, 'r') as f:
            txt = f.read()
            matches = re.findall(r"Frame (\d{1,2}) inliers ratio: (0.\d{3})", txt)
            matches = sorted(list(map(lambda t: (int(t[0]), float(t[1])), matches)), key=lambda t: t[0])
            curr_ratios = list(zip(*matches))[1]
            plt.plot(np.arange(2, 2 + len(curr_ratios)), curr_ratios, marker='o', label=res)
    plt.legend()
    plt.title("Inliers ratio (Frame number) dependency")
    plt.xlabel("Frame number")
    plt.ylabel("Inliers ratio")
    plt.savefig(os.path.join(results_dir, 'ratio_dependency.png'))


if __name__ == '__main__':
    main()
