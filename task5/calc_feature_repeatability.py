#!/usr/bin/env python
# -*- coding: utf-8 -*-

import argparse
from matplotlib import pyplot as plt
from timeit import default_timer as timer
from utils import *

argparser = argparse.ArgumentParser()
argparser.add_argument('--frames_path', '-fp', type=str, required=True)
argparser.add_argument('--results_path', '-rp', type=str, required=True)
argparser.add_argument('--detector_type', '-dt', type=str, required=True)

supported_detectors = ["SIFT", "BRISK", "AKAZE", "SHI_TOMASI"]

def visualize_ransac(x, y, center, model, thres, inliers_mask, vis_path):
    """ Визуализация метода RANSAC на графике
    """
    center_x, center_y = center
    model_x, model_y = model

    inliers_x, inliers_y = x[inliers_mask], y[inliers_mask]
    outliers_x, outliers_y = x[~inliers_mask], y[~inliers_mask]
    inliers_number = np.sum(inliers_mask)
    outliers_number = inliers_mask.shape[0] - inliers_number
    figure, axes = plt.subplots()
    border_circle = plt.Circle(center, radius=thres, color='g', fill=False, label='Border')
    inliers, = axes.plot(inliers_x, inliers_y, 'ro', color='r', markersize=1,
                         label="Inliers ({})".format(inliers_number))
    outliers, = axes.plot(outliers_x, outliers_y, 'ro', color='b', markersize=1,
                          label="Outliers ({})".format(outliers_number))
    model = axes.scatter(model_x, model_y, s=25, color='k', label="Model")
    axes.add_artist(border_circle)

    lower_quantile = 0.2
    upper_quantile = 0.8
    min_radius_ratio = 2
    lowX = min(np.quantile(x, q=lower_quantile), center_x - min_radius_ratio * thres)
    uppX = max(np.quantile(x, q=upper_quantile), center_x + min_radius_ratio * thres)
    lowY = min(np.quantile(y, q=lower_quantile), center_y - min_radius_ratio * thres)
    uppY = max(np.quantile(y, q=upper_quantile), center_y + min_radius_ratio * thres)
    axes.set_xlim([lowX, uppX])
    axes.set_ylim([lowY, uppY])

    axes.legend(handles=[model, inliers, outliers, border_circle])
    axes.set(xlabel='$\Delta X$', ylabel=r'$\Delta Y$')
    axes.set_title('RANSAC evaluated shift: ($\Delta X, \Delta Y$)=({:.3f}, {:.3f})'.format(model_x, model_y))
    plt.savefig(vis_path)


def estimate_by_ransac(anchor_keypoints, keypoints, matches, vis_path=None):
    """ Оценка параметров модели (сдвига) по найденным парам ключевых точек методом RANSAC
    """
    src_points = [key_point.pt for key_point in anchor_keypoints]
    tgt_points = [key_point.pt for key_point in keypoints]

    x, y = [], []
    for match in matches:
        src_index, tgt_index, desc_distance, des_ratio = match
        x.append(tgt_points[tgt_index][0] - src_points[src_index][0])
        y.append(tgt_points[tgt_index][1] - src_points[src_index][1])
    x, y = np.array(x), np.array(y)

    l2_thres = 10.0
    best_x, best_y = None, None
    best_inliers_number, best_inliers_mask = 0, None
    for i, (x_src, y_src) in enumerate(zip(x, y)):
        inliers_number = 0
        inliers_mask = np.zeros(shape=len(matches), dtype=np.bool)
        for j, (x_curr, y_curr) in enumerate(zip(x, y)):
            curr_dist = np.sqrt((x_curr - x_src) ** 2 + (y_curr - y_src) ** 2)
            if curr_dist < l2_thres:
                inliers_number += 1
                inliers_mask[j] = 1
        if inliers_number > best_inliers_number:
            best_inliers_number = inliers_number
            best_x = x_src
            best_y = y_src
            best_inliers_mask = inliers_mask

    model_x = np.sum(x * best_inliers_mask) / best_inliers_number
    model_y = np.sum(y * best_inliers_mask) / best_inliers_number

    full_inliers_mask = np.zeros(shape=len(anchor_keypoints), dtype=np.bool)
    first_match_indices = np.array(list(zip(*matches))[0], dtype=np.int32)
    full_inliers_mask[first_match_indices[best_inliers_mask]] = 1

    if vis_path is not None:
        visualize_ransac(x, y, (best_x, best_y), (model_x, model_y), l2_thres, best_inliers_mask, vis_path)
    return (model_x, model_y), full_inliers_mask


def estimate_model(anchor_keypoints, anchor_descriptors, keypoints, descriptors, descriptor_metric, vis_path=None):
    """ Оценка сдвига между кадрами на основе сопоставления ключевых точек
    """
    assert len(anchor_keypoints) == len(anchor_descriptors)
    assert len(keypoints) == len(descriptors)
    matches = []
    for i, anchor_descriptor in enumerate(anchor_descriptors):
        descriptor_distances = np.zeros(shape=len(descriptors))
        for j, descriptor in enumerate(descriptors):
            descriptor_distances[j] = descriptor_metric(anchor_descriptor, descriptor)
        sorted_indices = np.argsort(descriptor_distances)
        distance_ratio = descriptor_distances[sorted_indices[0]] / descriptor_distances[sorted_indices[1]]
        matches.append((i, sorted_indices[0], descriptor_distances[sorted_indices[0]], distance_ratio))
    matches = sorted(matches, key=lambda tup: tup[2])
    return estimate_by_ransac(anchor_keypoints, keypoints, matches, vis_path)


def launch_feature_algo(detect, compute, img):
    """ Запуск детекции ключевых точек на изображении, подсчет их дескрипторов
    """
    start_time = timer()
    keypoints = detect(img)
    end_time = timer()
    detector_time_per_point = (end_time - start_time) / len(keypoints)
    keypoints, descriptors = compute(img, keypoints)
    return keypoints, descriptors, detector_time_per_point


def save_ratios(frame_inliers_ratio, results_dir):
    """ Сохранение доли воспроизведенных точек в каждом кадре относительно первого опорного,
        построение графика зависимости доли воспроизведенных точек от номера кадра
    """
    with open(os.path.join(results_dir, 'inliers_ratio_per_frame.txt'), 'w') as f:
        for i, ratio in enumerate(frame_inliers_ratio, start=1):
            f.write("Frame {} inliers ratio: {:.3f}\n".format(i, ratio))

    fig = plt.figure()
    plt.plot(np.arange(2, 2 + len(frame_inliers_ratio)), frame_inliers_ratio, marker='o', color='b')
    plt.title("Inliers ratio (Frame number) dependency")
    plt.xlabel("Frame number")
    plt.ylabel("Inliers ratio")
    plt.savefig(os.path.join(results_dir, 'ratio_dependency.png'))


def estimate_repeatability(src_frames_dir, results_dir, features_stuff, dump_vis=True):
    """ Оценка воспроизводимости детектора особых точек
    """
    features_algo, descriptor_metric, algo_name = features_stuff

    detect, compute = None, None
    if isinstance(features_algo, tuple):
        detector, descriptor = features_algo
        detect = lambda img: detector.detect(img, None)
        compute = lambda img, kp: descriptor.compute(img, kp)
    else:
        detect = lambda img: features_algo.detect(img)
        compute = lambda img, kp: features_algo.compute(img, kp)

    frame_fnames = [os.path.join(src_frames_dir, f) for f in os.listdir(src_frames_dir)]
    gray_frames = [cv2.imread(f, cv2.IMREAD_GRAYSCALE) for f in sorted(frame_fnames)]
    color_frames = [cv2.imread(f, cv2.IMREAD_COLOR) for f in sorted(frame_fnames)]

    anchor_frame, shifted_frames = gray_frames[0], gray_frames[1:]

    results_dir = os.path.join(results_dir, algo_name)
    os.makedirs(results_dir, exist_ok=True)
    vis_dir = None
    if dump_vis:
        vis_dir = os.path.join(results_dir, "ransac_viz")
        os.makedirs(vis_dir, exist_ok=True)

    detector_time_per_kp = []
    anchor_keypoints, anchor_descriptors, anchor_time = launch_feature_algo(detect, compute, anchor_frame)
    detector_time_per_kp.append(anchor_time)

    keypoint_frequencies = np.zeros(shape=len(anchor_keypoints))
    frame_shifts, frame_inliers_ratio = [], []
    for i, shifted_frame in enumerate(shifted_frames, start=1):
        frame_keypoints, frame_descriptors, frame_time = launch_feature_algo(detect, compute, shifted_frame)
        detector_time_per_kp.append(frame_time)

        vis_path = os.path.join(vis_dir, f'frame_{i}_vis.png') if dump_vis else None
        shift, inliers_mask = estimate_model(anchor_keypoints, anchor_descriptors,
                                             frame_keypoints, frame_descriptors, descriptor_metric, vis_path)
        keypoint_frequencies += inliers_mask
        frame_inliers_ratio.append(np.sum(inliers_mask) / len(anchor_keypoints))
        frame_shifts.append(shift)

    repeatability = np.sum(keypoint_frequencies) / len(anchor_keypoints) / len(shifted_frames)
    with open(os.path.join(results_dir, 'repeatability.txt'), 'w') as f:
        f.write("Repeatability: {:.3f}\n".format(repeatability))

    detector_time = sum(detector_time_per_kp) / len(detector_time_per_kp)
    with open(os.path.join(results_dir, "time.txt"), 'w') as f:
        f.write("Detector time (per point): {}\n".format(detector_time))

    save_ratios(frame_inliers_ratio, results_dir)

    balanced_dir = os.path.join(results_dir, "balanced_imgs")
    save_balanced_imgs(balanced_dir, color_frames, frame_shifts)
    save_as_gif(balanced_dir, os.path.join(results_dir, 'balanced.gif'))
    save_as_gif(src_frames_dir, os.path.join(results_dir, 'unbalanced.gif'))


def get_feature_tools(detector_type):
    l2_metric = lambda anchor_des, des: np.linalg.norm(anchor_des - des)
    hamming_metric = lambda anchor_des, des: np.sum(np.unpackbits(np.bitwise_xor(anchor_des, des)))

    if detector_type == "SIFT":
        features_algo = cv2.SIFT_create()
        return (features_algo, l2_metric, "SIFT")
    elif detector_type == "BRISK":
        features_algo = cv2.BRISK_create()
        return (features_algo, hamming_metric, "BRISK")
    elif detector_type == "AKAZE":
        features_algo = cv2.AKAZE_create()
        return (features_algo, hamming_metric, "AKAZE")
    elif detector_type == "SHI_TOMASI":
        detector = cv2.GFTTDetector_create()
        descriptor = cv2.SIFT_create()
        return ((detector, descriptor), l2_metric, "SHI_TOMASI_SIFT")


def main():
    args = argparser.parse_args()
    estimate_repeatability(args.frames_path, args.results_path, get_feature_tools(args.detector_type))


if __name__ == '__main__':
    main()
