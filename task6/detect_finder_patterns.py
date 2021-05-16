import os
import math
import json
import cv2
import scipy.ndimage as sci
import numpy as np
import argparse


argparser = argparse.ArgumentParser()
argparser.add_argument('--images_path', '-ip', type=str, required=True)
argparser.add_argument('--results_path', '-rp', type=str, required=True)
argparser.add_argument('--markup_path', '-mp', type=str, default=None)
argparser.add_argument('--enable_logs', '-el', action="store_true", default=False)


projection_elements = 5
center_pos = projection_elements // 2


class CProjCandidate:
    def __init__(self, position, loss, mod_size, bar_starts, offset):
        self.position = position
        self.loss = loss
        self.mod_size = mod_size
        self.begin = bar_starts[offset]
        self.end = bar_starts[offset + projection_elements]
        self.center_begin = bar_starts[offset + center_pos]
        self.center_end = bar_starts[offset + center_pos + 1]


def collect_projections(projections, bar_starts, first_is_black, position):
    loss_threshold = 0.3
    projection_weights = np.array([1., 1., 3., 1., 1.])

    new_projections = []
    bar_widths = bar_starts[1:] - bar_starts[:-1]
    start_index = 1 - int(first_is_black)
    for j in range(start_index, len(bar_starts) - projection_elements, 2):
        bars = bar_widths[j:j + projection_elements]
        mod_size = np.sum(bars * projection_weights) / np.sum(projection_weights ** 2)
        supp_bars = projection_weights * mod_size
        loss = np.mean(np.abs(bars - supp_bars) / supp_bars)
        if loss < loss_threshold:
            if len(new_projections) != 0 and bar_starts[j] < new_projections[-1].end:
                if loss < new_projections[-1].loss:
                    del new_projections[-1]
                    new_projections.append(CProjCandidate(position, loss, mod_size, bar_starts, j))
            else:
                new_projections.append(CProjCandidate(position, loss, mod_size, bar_starts, j))
    projections.extend(new_projections)


def find_projections(image):
    height, width = image.shape[0], image.shape[1]

    horizontal_projections = []
    for i in range(height):
        bar_starts = np.nonzero(image[i, 1:] - image[i, :-1])[0] + 1
        bar_starts = np.array([0] + list(bar_starts) + [width])
        first_is_black = image[i, 0] == 0
        collect_projections(horizontal_projections, bar_starts, first_is_black, i)

    vertical_projections = []
    for i in range(width):
        bar_starts = np.nonzero(image[1:, i] - image[:-1, i])[0] + 1
        bar_starts = np.array([0] + list(bar_starts) + [height])
        first_is_black = image[0, i] == 0
        collect_projections(vertical_projections, bar_starts, first_is_black, i)
    return horizontal_projections, vertical_projections


def build_masks(hor_proj, ver_proj, shape):
    hor_mask = np.zeros(shape=shape, dtype=np.bool)
    ver_mask = np.zeros(shape=shape, dtype=np.bool)

    for i, proj in enumerate(hor_proj):
        hor_mask[proj.position, proj.center_begin:proj.center_end] = True

    for i, proj in enumerate(ver_proj):
        ver_mask[proj.center_begin:proj.center_end, proj.position] = True

    return hor_mask, ver_mask


def build_consistent_mask(hor_mask, ver_mask, hor_proj, ver_proj):
    consistency_ratio = 0.5
    mask = np.zeros_like(hor_mask, dtype=np.bool)

    for proj in hor_proj:
        min_intersection = max(1, int((proj.center_end - proj.center_begin) * consistency_ratio))
        if np.sum(ver_mask[proj.position, proj.center_begin:proj.center_end]) > min_intersection:
            mask[proj.position, proj.center_begin:proj.center_end] = True

    for proj in ver_proj:
        min_intersection = max(1, int((proj.center_end - proj.center_begin) * consistency_ratio))
        if np.sum(hor_mask[proj.center_begin:proj.center_end, proj.position]) > min_intersection:
            mask[proj.center_begin:proj.center_end, proj.position] = True
    return mask


def filter_centers(center_bboxes, center_masks):
    area_ratio_threshold = 0.6
    max_aspect_ratio = 1.2
    min_aspect_ratio = 0.8

    filtered_center_bboxes, filtered_center_masks = [], []

    for bbox, mask in zip(center_bboxes, center_masks):
        left, top, right, bottom = bbox

        box_w = bottom - top
        box_h = right - left
        aspect_ratio = box_w / box_h
        if aspect_ratio < min_aspect_ratio or aspect_ratio > max_aspect_ratio:
            continue

        box_area = box_w * box_h
        obj_area = np.sum(mask)
        object_area_ratio = obj_area / box_area
        if object_area_ratio < area_ratio_threshold:
            continue

        filtered_center_bboxes.append(bbox)
        filtered_center_masks.append(mask)
    return filtered_center_bboxes, filtered_center_masks


def separate_objects(mask):
    labeled, n_objects = sci.measurements.label(mask)
    object_box_slices = sci.find_objects(labeled)

    object_bboxes, object_masks = [], []
    for i, (y_borders, x_borders) in enumerate(object_box_slices, start=1):
        top, bottom = y_borders.start, y_borders.stop
        left, right = x_borders.start, x_borders.stop
        object_bboxes.append((left, top, right, bottom))
        object_masks.append(labeled == i)

    return object_bboxes, object_masks


def retrieve_ext_frames(binary, center_bboxes, center_masks):
    height, width = binary.shape[0], binary.shape[1]
    ext_bboxes, ext_masks = [], []
    max_mod_size = 0

    for bbox, mask in zip(center_bboxes, center_masks):
        left, top, right, bottom = bbox
        area = np.sum(mask)
        curr_mod_size = math.sqrt(area) / 3.
        max_mod_size = max(max_mod_size, curr_mod_size)

        box_w = right - left
        box_h = bottom - top
        center_x = left + box_w // 2
        center_y = top + box_h // 2

        search_half = int(curr_mod_size * 4 * math.sqrt(2))
        search_top = max(center_y - search_half, 0)
        search_bottom = min(center_y + search_half, height)
        search_left = max(center_x - search_half, 0)
        search_right = min(center_x + search_half, width)

        search_subimage = binary[search_top:search_bottom, search_left:search_right]
        object_bboxes, object_masks = separate_objects(255 - search_subimage)
        object_areas = np.array(list(map(np.sum, object_masks)))

        curr_ext_mask = np.zeros(shape=(height, width), dtype=np.bool)
        appropriate_index = np.argmax(object_areas)
        curr_ext_mask[search_top:search_bottom, search_left:search_right] = object_masks[appropriate_index]
        rel_left, rel_top, rel_right, rel_bottom = object_bboxes[appropriate_index]
        ext_bbox_left = search_left + rel_left
        ext_bbox_right = search_left + rel_right
        ext_bbox_top = search_top + rel_top
        ext_bbox_bottom = search_top + rel_bottom

        ext_masks.append(curr_ext_mask)
        ext_bboxes.append((ext_bbox_left, ext_bbox_top, ext_bbox_right, ext_bbox_bottom))

    return ext_bboxes, ext_masks, max_mod_size


def build_pattern_borders(center_masks, ext_masks, max_mod_size):
    full_masks = [np.logical_or(center_mask, ext_mask) for center_mask, ext_mask in zip(center_masks, ext_masks)]

    border_width = 3
    morphology_radius = int(2 * max_mod_size)
    struct = sci.generate_binary_structure(2, 2)

    pattern_border_masks = []
    for mask in full_masks:
        dilated = sci.binary_dilation(mask, struct, iterations=morphology_radius)
        partially_erode = sci.binary_erosion(dilated, struct, iterations=morphology_radius - border_width)
        fully_back_erode = sci.binary_erosion(partially_erode, struct, iterations=border_width)
        pattern_border_masks.append(fully_back_erode ^ partially_erode)
    return pattern_border_masks


def prepare_images(image):
    gray_image = cv2.cvtColor(image, cv2.COLOR_RGB2GRAY)
    binarized_image = cv2.adaptiveThreshold(gray_image, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY, 63, 10)
    processed_binarized = cv2.medianBlur(binarized_image, 5)
    return gray_image, processed_binarized


def dump_masks(image, masks, color=(255, 0, 0)):
    image_copy = np.copy(image)
    if len(masks) != 0:
        full_mask = np.logical_or.reduce(np.array(masks))
        image_copy[full_mask != 0] = color
    return image_copy


def dump_rects(image, rects, color=(255, 0, 0), draw_width=3):
    image_copy = np.copy(image)
    for (left, top, right, bottom) in rects:
        cv2.rectangle(image_copy, (left, top), (right, bottom), color, draw_width)
    return image_copy


def detect_finder_patterns(image, logs_folder=None):
    gray_image, binary_image = prepare_images(image)

    hor_proj, ver_proj = find_projections(binary_image)

    hor_mask, ver_mask = build_masks(hor_proj, ver_proj, binary_image.shape)

    consistent_mask = build_consistent_mask(hor_mask, ver_mask, hor_proj, ver_proj)

    center_bboxes, center_masks = separate_objects(consistent_mask)

    filtered_center_bboxes, filtered_center_masks = filter_centers(center_bboxes, center_masks)

    ext_bboxes, ext_masks, mod_size = retrieve_ext_frames(binary_image, filtered_center_bboxes, filtered_center_masks)

    pattern_borders = build_pattern_borders(filtered_center_masks, ext_masks, mod_size)
    pattern_borders_viz = dump_masks(image, pattern_borders)

    if logs_folder is not None:
        full_masks = [np.logical_or(center_mask, ext_mask) for center_mask, ext_mask in
                      zip(filtered_center_masks, ext_masks)]
        logs = {
            "gray_source": gray_image,
            "binary_source": binary_image,
            "consistently_masked": dump_masks(image, [consistent_mask]),
            "filtered_centers": dump_masks(image, filtered_center_masks),
            "external_parts": dump_masks(image, ext_masks),
            "black_parts_marked": dump_masks(image, full_masks),
            "bound_boxes": dump_rects(image, ext_bboxes),
            "pattern_borders": pattern_borders_viz
        }
        for img_name, img in logs.items():
            cv2.imwrite(os.path.join(logs_folder, f'{img_name}.jpg'), cv2.cvtColor(img, cv2.COLOR_RGB2BGR))

    return ext_bboxes, pattern_borders, pattern_borders_viz


def rect_iou(lhs_rect, rhs_rect):
    lhs_left, lhs_top, lhs_right, lhs_bottom = lhs_rect
    rhs_left, rhs_top, rhs_right, rhs_bottom = rhs_rect

    intersection_left = max(lhs_left, rhs_left)
    intersection_right = min(lhs_right, rhs_right)

    intersection_top = max(lhs_top, rhs_top)
    intersection_bottom = min(lhs_bottom, rhs_bottom)

    intersection_area = (intersection_right - intersection_left) * (intersection_bottom - intersection_top)
    if intersection_area == 0:
        return 0

    lhs_area = (lhs_right - lhs_left) * (lhs_bottom - lhs_top)
    rhs_area = (rhs_right - rhs_left) * (rhs_bottom - rhs_top)

    iou = intersection_area / (lhs_area + rhs_area - intersection_area)
    return iou


def update_counters(counters, found_boxes, markup_boxes):
    iou_threshold = 0.5

    markup_found = 0
    for bbox in found_boxes:
        found_match = False
        for markup_box in markup_boxes:
            if rect_iou(bbox, markup_box) > iou_threshold:
                markup_found += 1
                found_match = True
                break
        if not found_match:
            counters["fp"] += 1
    counters["tp"] += markup_found
    counters["fn"] += len(markup_boxes) - markup_found


def dump_metrics(counters, txt_path):
    precision = counters["tp"] / (counters["tp"] + counters["fp"])
    recall = counters["tp"] / (counters["tp"] + counters["fn"])
    f_measure = 2 * precision * recall / (precision + recall)
    with open(txt_path, "w") as f:
        f.write(f'Precision: {precision:.4f}\n')
        f.write(f'Recall: {recall:.4f}\n')
        f.write(f'F-measure: {f_measure:.4f}\n')
        f.write(f'True positive: {counters["tp"]}\n')
        f.write(f'False positive: {counters["fp"]}\n')
        f.write(f'False negative: {counters["fn"]}\n')


def inference_dir(img_dir, res_dir, markup_path=None, enable_logs=False):
    viz_res_dir = os.path.join(res_dir, "marked_viz")
    os.makedirs(viz_res_dir)
    if markup_path is not None:
        markup = json.load(open(markup_path))
        markup = dict((obj['img_filename'], obj['object_bboxes']) for obj in markup)
        counters = {"tp": 0, "fp": 0, "fn": 0}
    if enable_logs:
        logs_folder = os.path.join(res_dir, "logs")
        os.makedirs(logs_folder)

    for i, img_fname in enumerate(sorted(os.listdir(img_dir))):
        img_path = os.path.join(img_dir, img_fname)
        res_path = os.path.join(viz_res_dir, img_fname)

        colored_image = cv2.imread(img_path, cv2.IMREAD_COLOR)
        colored_image = cv2.cvtColor(colored_image, cv2.COLOR_BGR2RGB)

        curr_log_folder = None
        if enable_logs:
            curr_log_folder = os.path.join(logs_folder, img_fname.split(".")[0])
            os.makedirs(curr_log_folder)

        bboxes, borders, viz = detect_finder_patterns(colored_image, curr_log_folder)
        cv2.imwrite(res_path, cv2.cvtColor(viz, cv2.COLOR_RGB2BGR))

        if markup_path is not None:
            update_counters(counters, bboxes, markup[img_fname])

    if markup_path is not None:
        dump_metrics(counters, os.path.join(res_dir, "metrics.txt"))


def main():
    args = argparser.parse_args()
    inference_dir(args.images_path, args.results_path, markup_path=args.markup_path, enable_logs=args.enable_logs)


if __name__ == '__main__':
    main()
