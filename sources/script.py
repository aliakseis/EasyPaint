import cv2
import numpy as np
from PIL import Image

def list_functions():
    """
    Returns a list of names of functions defined in this module.
    
    Returns:
        list: A list of strings representing the names of callable functions.
    """
    return [name for name, obj in globals().items() if callable(obj) and not name.startswith("__")]

def denoise_image(image: np.ndarray, strength: int = 10) -> np.ndarray:
    """Applies denoising to an image using OpenCV's fastNlMeansDenoising.

    Args:
        image (np.ndarray): Input image as a NumPy array.
        strength (int, optional): Denoising strength. Defaults to 10.

    Returns:
        np.ndarray: Denoised image.
    """
    return cv2.fastNlMeansDenoisingColored(image, None, strength, strength, 7, 21)

def deblur_image(image: np.ndarray, kernel_size: int = 5) -> np.ndarray:
    """Applies Wiener filtering for basic image deblurring.

    Args:
        image (np.ndarray): Input blurry image as a NumPy array.
        kernel_size (int, optional): Size of the blur kernel. Defaults to 5.

    Returns:
        np.ndarray: Deblurred image.
    """
    image_gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    kernel = np.ones((kernel_size, kernel_size), np.float32) / (kernel_size ** 2)
    deblurred = cv2.filter2D(image_gray, -1, kernel)
    return cv2.cvtColor(deblurred, cv2.COLOR_GRAY2BGR)

def gamma_correct(image: np.ndarray, gamma: float = 1.2) -> np.ndarray:
    """Adjusts image brightness using gamma correction.

    Args:
        image (np.ndarray): Input image as a NumPy array.
        gamma (float, optional): Gamma correction factor. Defaults to 1.2.

    Returns:
        np.ndarray: Gamma-adjusted image.
    """
    inv_gamma = 1.0 / gamma
    lookup_table = np.array([((i / 255.0) ** inv_gamma) * 255 for i in range(256)]).astype("uint8")
    return cv2.LUT(image, lookup_table)

def enhance_contrast(image: np.ndarray, clip_limit: float = 2.0, tile_grid_size: tuple = (8, 8)) -> np.ndarray:
    """Enhances image contrast using CLAHE (Contrast Limited Adaptive Histogram Equalization).

    Args:
        image (np.ndarray): Input grayscale image as a NumPy array.
        clip_limit (float, optional): Threshold for contrast clipping. Defaults to 2.0.
        tile_grid_size (tuple, optional): Grid size for local histogram equalization. Defaults to (8, 8).

    Returns:
        np.ndarray: Contrast-enhanced image.
    """
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    clahe = cv2.createCLAHE(clipLimit=clip_limit, tileGridSize=tile_grid_size)
    return clahe.apply(gray)
