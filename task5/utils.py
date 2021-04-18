import os
import imageio
import cv2
import numpy as np

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