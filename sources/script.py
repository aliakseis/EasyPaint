import cv2
import numpy as np
from PIL import Image

def denoise_image(image: np.ndarray, strength: int = 10) -> np.ndarray:
    """Applies denoising to an image using OpenCV's fastNlMeansDenoisingColored.

    Args:
        image (np.ndarray): Input image as a NumPy array (should be BGR).
        strength (int, optional): Denoising strength. Defaults to 10.

    Returns:
        np.ndarray: Denoised image.
    """
    return cv2.fastNlMeansDenoisingColored(image, None, strength, strength, 7, 21)

def blur_image(image: np.ndarray, kernel_size: int = 5) -> np.ndarray:
    """Applies filtering for basic image blurring, preserving color.

    Args:
        image (np.ndarray): Input blurry image as a NumPy array (should be BGR).
        kernel_size (int, optional): Size of the blur kernel. Defaults to 5.

    Returns:
        np.ndarray: Deblurred image.
    """
    kernel = np.ones((kernel_size, kernel_size), np.float32) / (kernel_size ** 2)
    
    # Apply the filter to each color channel separately to preserve color.
    channels = cv2.split(image)
    blurred_channels = [cv2.filter2D(channel, -1, kernel) for channel in channels]
    
    # Merge channels back
    return cv2.merge(blurred_channels)

def deblur_image(image: np.ndarray, kernel_size: int = 5) -> np.ndarray:
    """Applies Wiener filtering for image deblurring while preserving colors.

    Args:
        image (np.ndarray): Blurry image (BGR format).
        kernel_size (int, optional): Size of the blur kernel. Defaults to 5.

    Returns:
        np.ndarray: Deblurred image.
    """
    # Convert image to float32 for better accuracy
    image = np.float32(image) / 255.0

    # Apply Wiener filter to each channel separately
    channels = cv2.split(image)
    deblurred_channels = [cv2.GaussianBlur(channel, (kernel_size, kernel_size), 0) for channel in channels]

    # Boost the original image by subtracting the blurred version
    sharpened_channels = [cv2.addWeighted(orig, 1.5, blur, -0.5, 0) for orig, blur in zip(channels, deblurred_channels)]

    # Merge channels back and convert to uint8
    deblurred_image = cv2.merge(sharpened_channels)
    deblurred_image = np.clip(deblurred_image * 255, 0, 255).astype(np.uint8)

    return deblurred_image

def gamma_correct(image: np.ndarray, gamma: float = 1.2) -> np.ndarray:
    """Adjusts image brightness using gamma correction.

    Args:
        image (np.ndarray): Input image as a NumPy array (should be BGR).
        gamma (float, optional): Gamma correction factor. Defaults to 1.2.

    Returns:
        np.ndarray: Gamma-adjusted image.
    """
    inv_gamma = 1.0 / gamma
    lookup_table = np.array([((i / 255.0) ** inv_gamma) * 255 for i in range(256)]).astype("uint8")
    
    # Apply gamma correction to all channels
    return cv2.LUT(image, lookup_table)

def enhance_contrast(image: np.ndarray, clip_limit: float = 2.0, tile_grid_width: int = 8, tile_grid_height: int = 8) -> np.ndarray:
    """Enhances contrast of a color image using CLAHE applied to the L channel in LAB space.

    Args:
        image (np.ndarray): Input color image (BGR format).
        clip_limit (float, optional): Threshold for contrast clipping. Defaults to 2.0.
        tile_grid_width (int, optional): Width of the grid for local histogram equalization. Defaults to 8.
        tile_grid_height (int, optional): Height of the grid for local histogram equalization. Defaults to 8.

    Returns:
        np.ndarray: Contrast-enhanced image.
    """
    # Convert to LAB color space
    lab = cv2.cvtColor(image, cv2.COLOR_BGR2LAB)
    
    # Apply CLAHE only to the L channel
    l, a, b = cv2.split(lab)
    clahe = cv2.createCLAHE(clipLimit=clip_limit, tileGridSize=(tile_grid_width, tile_grid_height))
    l = clahe.apply(l)

    # Merge channels back and convert to BGR
    enhanced_lab = cv2.merge([l, a, b])
    enhanced_image = cv2.cvtColor(enhanced_lab, cv2.COLOR_LAB2BGR)

    return enhanced_image

def resize_image(image: np.ndarray, width: int = 320, height: int = 320) -> np.ndarray:
    """Resizes an image to the specified width and height.

    Args:
        image (np.ndarray): Input image as a NumPy array.
        width (int): Target width in pixels.
        height (int): Target height in pixels.

    Returns:
        np.ndarray: Resized image.
    """
    return cv2.resize(image, (width, height), interpolation=cv2.INTER_LINEAR)

def generate_mandelbrot(width: int = 800, height: int = 800, max_iter: int = 100) -> np.ndarray:
    """Generates a Mandelbrot fractal image.

    Args:
        width (int): Width of the output image.
        height (int): Height of the output image.
        max_iter (int): Maximum iterations to determine divergence.

    Returns:
        np.ndarray: Grayscale Mandelbrot fractal.
    """
    x_min, x_max, y_min, y_max = -2.0, 1.0, -1.5, 1.5
    x, y = np.linspace(x_min, x_max, width), np.linspace(y_min, y_max, height)
    X, Y = np.meshgrid(x, y)
    C = X + 1j * Y
    Z = np.zeros_like(C, dtype=np.complex128)

    mandelbrot = np.zeros(C.shape, dtype=np.uint8)

    for i in range(max_iter):
        mask = np.abs(Z) < 2
        Z[mask] = Z[mask]**2 + C[mask]
        mandelbrot[mask] = (i * 255) // max_iter
    
    return cv2.applyColorMap(mandelbrot, cv2.COLORMAP_INFERNO)

def generate_julia(width: int = 800, height: int = 800, max_iter: int = 100, c: complex = -0.7 + 0.27015j) -> np.ndarray:
    """Generates a Julia fractal image.

    Args:
        width (int): Width of the output image.
        height (int): Height of the output image.
        max_iter (int): Maximum iterations to determine divergence.
        c (complex): Complex constant for Julia set transformation.

    Returns:
        np.ndarray: Grayscale Julia fractal.
    """
    x_min, x_max, y_min, y_max = -1.5, 1.5, -1.5, 1.5
    x, y = np.linspace(x_min, x_max, width), np.linspace(y_min, y_max, height)
    X, Y = np.meshgrid(x, y)
    Z = X + 1j * Y

    julia = np.zeros(Z.shape, dtype=np.uint8)

    for i in range(max_iter):
        mask = np.abs(Z) < 2
        Z[mask] = Z[mask]**2 + c
        julia[mask] = (i * 255) // max_iter
    
    return cv2.applyColorMap(julia, cv2.COLORMAP_MAGMA)

def generate_plasma(width: int = 800, height: int = 800) -> np.ndarray:
    """Generates a plasma-like color texture.

    Args:
        width (int): Width of the output image.
        height (int): Height of the output image.

    Returns:
        np.ndarray: Colorized plasma effect.
    """
    x = np.linspace(-3, 3, width)
    y = np.linspace(-3, 3, height)
    X, Y = np.meshgrid(x, y)
    Z = np.sin(X * 3) + np.cos(Y * 3)

    plasma = cv2.normalize(Z, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    return cv2.applyColorMap(plasma, cv2.COLORMAP_TWILIGHT)

