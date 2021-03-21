import os
import imageio
import cv2
import numpy as np
import argparse
from matplotlib import pyplot as plt
from timeit import default_timer as timer

argparser = argparse.ArgumentParser()
argparser.add_argument('--frames_path', '-fp', type=str, required=True)
argparser.add_argument('--results_path', '-rp', type=str, required=True)

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

    # Ограничения на диапазон координат по каждой координате в картинке-визуализации
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

    # Для каждой пары ключей определяем вектор сдвига
    x, y = [], []
    for match in matches:
        src_index, tgt_index, desc_distance, des_ratio = match
        x.append(tgt_points[tgt_index][0] - src_points[src_index][0])
        y.append(tgt_points[tgt_index][1] - src_points[src_index][1])
    x, y = np.array(x), np.array(y)

    # Сам RANSAC будем использовать так:
    # Каждую отдельную точку (пару ключей) рассматриваем как модель сдвига
    # Оценивающая функция для определения inlier/outlier - евклидово расстояние до выбранной точки
    l2_thres = 8.0
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

    # уточняем модель по inlier-м
    model_x = np.sum(x * best_inliers_mask) / best_inliers_number
    model_y = np.sum(y * best_inliers_mask) / best_inliers_number
    # Визуализируем, если требуется
    if vis_path is not None:
        visualize_ransac(x, y, (best_x, best_y), (model_x, model_y), l2_thres, best_inliers_mask, vis_path)
    return (model_x, model_y)


def estimate_shift(anchor_keypoints, anchor_descriptors, keypoints, descriptors, vis_path=None):
    """ Оценка сдвига между кадрами на основе сопоставления ключевых точек
    """
    assert len(anchor_keypoints) == len(anchor_descriptors)
    assert len(keypoints) == len(descriptors)
    # Сопоставление ключевых точек по их дескрипторам
    # Отсечение по отношению расстояния между ближайшими двумя соседями
    distance_ratio_tolerance = 0.8
    matches = []
    for i, anchor_descriptor in enumerate(anchor_descriptors):
        descriptor_distances = np.zeros(shape=len(descriptors))
        for j, descriptor in enumerate(descriptors):
            descriptor_distances[j] = np.linalg.norm(anchor_descriptor - descriptor)
        sorted_indices = np.argsort(descriptor_distances)
        distance_ratio = descriptor_distances[sorted_indices[0]] / descriptor_distances[sorted_indices[1]]
        if distance_ratio < distance_ratio_tolerance:
            matches.append((i, sorted_indices[0], descriptor_distances[sorted_indices[0]], distance_ratio))
    matches = sorted(matches, key=lambda tup: tup[2])
    # Оцениваем параметры модели с помощью RANSAC
    return estimate_by_ransac(anchor_keypoints, keypoints, matches, vis_path), matches


def save_txt_shifts(shifts, results_dir):
    """ Сохранение сдвигов в отдельные текстовые файлы
    """
    txt_shifts_dir = os.path.join(results_dir, "shifts")
    if not os.path.exists(txt_shifts_dir):
        os.makedirs(txt_shifts_dir, exist_ok=True)
    for i, shift in enumerate(shifts, start=1):
        with open(os.path.join(txt_shifts_dir, f'frame_{i}_shift.txt'), 'w') as f:
            f.write("deltaX: {:.3f}\n".format(shift[0]))
            f.write("deltaY: {:.3f}\n".format(shift[1]))


def save_timers(timediffs, results_dir):
    """ Сохранение счетчиков времени работы и
        построение графика зависимости времени алгоритма от числа обработанных кадров
    """
    with open(os.path.join(results_dir, 'timers.txt'), 'w') as f:
        for i, timediff in enumerate(timediffs, start=2):
            f.write(f'{i} frames processsed, elapsed time:\t {timediff}\n')
    fig = plt.figure()
    plt.plot(np.arange(2, 2 + len(timediffs)), timediffs, marker='o', color='b')
    plt.title("Time (Frames processed) dependency")
    plt.xlabel("Frames processed")
    plt.ylabel("Time, sec")
    plt.savefig(os.path.join(results_dir, 'time_dependency.png'))


def save_as_gif(src_dir, gif_path, fps=6):
    """ Слияние отдельных кадров в общий gif
    """
    images = []
    fnames = [os.path.join(src_dir, f) for f in os.listdir(src_dir)]
    for filename in fnames:
        images.append(imageio.imread(filename))
    imageio.mimsave(gif_path, images, fps=fps)


def shift_image(image, _delta_x, _delta_y):
    """ Сдвиг изображения
    """
    delta_x = int(round(_delta_x))
    delta_y = int(round(_delta_y))
    height, width = image.shape[0], image.shape[1]
    shifted = np.zeros_like(image, dtype=np.uint8)
    # todo: сделать без костылей через нормальный бродкаст
    if delta_x >= 0 and delta_y >= 0:
        shifted[delta_y:height,delta_x:width] = image[0:height-delta_y,0:width-delta_x]
        shifted[0:delta_y,delta_x:width,:] = image[0:1,0:width-delta_x]
        shifted[delta_y:height,0:delta_x] = image[0:height-delta_y,0:1]
        shifted[0:delta_y,0:delta_x] = image[0,0]
    elif delta_x < 0 and delta_y > 0:
        shifted[delta_y:height,0:width+delta_x] = image[0:height-delta_y:,-delta_x:width]
        shifted[0:delta_y,0:width+delta_x] = image[0:1,-delta_x:width]
        shifted[delta_y:height,width+delta_x:width] = image[0:height-delta_y,-1:-2:-1]
        shifted[0:delta_y,width+delta_x:width] = image[0,-1]
    elif delta_x > 0 and delta_y < 0:
        shifted[0:height+delta_y,delta_x:width] = image[-delta_y:height,0:width-delta_x]
        shifted[0:height+delta_y,0:delta_x] = image[-delta_y:height, 0:1]
        shifted[height+delta_y:height,delta_x:width] = image[-1:-2:-1,0:width-delta_x]
        shifted[height+delta_y:height,0:delta_x] = image[-1,0]
    else:
        shifted[0:height+delta_y,0:width+delta_x] = image[-delta_y:height,-delta_x:width]
        shifted[height+delta_y:height,0:width+delta_x] = image[-1:-2:-1,-delta_x:width]
        shifted[0:height+delta_y, width+delta_x:width] = image[-delta_y:height,-1:-2:-1]
        shifted[height+delta_y:height,width+delta_x:width] = image[-1,-1]
    return shifted


def save_balanced_imgs(balanced_dir, src_imgs, shifts):
    """ Сохранение изображений, сдвинутых с целью компенсации относительного движения между кадрами
    """
    if not os.path.exists(balanced_dir):
        os.makedirs(balanced_dir, exist_ok=False)
    for i, (shift, img) in enumerate(zip(shifts, src_imgs[1:]), start=1):
        shifted_image = shift_image(img, -shift[0], -shift[1])
        cv2.imwrite(os.path.join(balanced_dir, f'balanced_{i}.png'), shifted_image)
    cv2.imwrite(os.path.join(balanced_dir, f'balanced_0.png'), src_imgs[0])


def estimate_shifts(src_frames_dir, results_dir, dump_vis=True):
    """ Запуск алгоритма оценки глобальных сдвигов кадров (относительно первого кадра)
    """
    frame_fnames = [os.path.join(src_frames_dir, f) for f in os.listdir(src_frames_dir)]
    frames = [cv2.imread(f, cv2.IMREAD_COLOR) for f in sorted(frame_fnames)]
    anchor_frame, shifted_frames = frames[0], frames[1:]

    # Создаем директорию под визуализации RANSAC
    vis_dir = None
    if dump_vis:
        vis_dir = os.path.join(results_dir, "visualizations")
        os.makedirs(vis_dir, exist_ok=True)

    start_time = timer()
    feature_extractor = cv2.SIFT_create()
    # Извлекаем ключевые точки с первого кадра
    anchor_keypoints, anchor_descriptors = feature_extractor.detectAndCompute(anchor_frame, None)
    # Для каждого следующего кадра оцениваем его сдвиг относительно первого кадра
    frame_shifts, timestamps = [], []
    for i, shifted_frame in enumerate(shifted_frames, start=1):
        frame_keypoints, frame_descriptors = feature_extractor.detectAndCompute(shifted_frame, None)
        vis_path = os.path.join(vis_dir, f'frame_{i}_vis.png') if dump_vis else None
        shift, _ = estimate_shift(anchor_keypoints, anchor_descriptors, frame_keypoints, frame_descriptors, vis_path)
        frame_shifts.append(shift)
        timestamps.append(timer())
    timediffs = [t - start_time for t in timestamps]

    # Сохраняем сдвиги в текстовых файлах
    save_txt_shifts(frame_shifts, results_dir)
    # Сохраняем счетчики времени
    save_timers(timediffs, results_dir)

    # Строим изображения с компенсацией движения
    balanced_dir = os.path.join(results_dir, "balanced_imgs")
    save_balanced_imgs(balanced_dir, frames, frame_shifts)
    # Сохраняем последовательности кадров в gif-формат
    save_as_gif(balanced_dir, os.path.join(results_dir, 'balanced.gif'))
    save_as_gif(src_frames_dir, os.path.join(results_dir, 'unbalanced.gif'))


def main():
    args = argparser.parse_args()
    estimate_shifts(args.frames_path, args.results_path, dump_vis=True)


if __name__ == '__main__':
    main()
