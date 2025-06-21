import numpy as np
from PIL import Image
import tensorflow as tf
import tensorflow_hub as hub
from typing import Union

# ——————————————————————————————————————————————
# Load TF-Hub style-transfer model once
# ——————————————————————————————————————————————
hub_model = hub.load(
    "https://tfhub.dev/google/magenta/arbitrary-image-stylization-v1-256/2"
)

# Wrap model call in tf.function for max throughput
@tf.function
def _stylize_graph(content: tf.Tensor, style: tf.Tensor) -> tf.Tensor:
    # returns [1,H,W,3] in [0,1]
    return hub_model(content, style)[0]

# Global to hold the style tensor once set
_style_tf: tf.Tensor = None

# Maximum side length when we resize (preserves aspect ratio)
_MAX_DIM = 1024

# ——————————————————————————————————————————————
# Internal utility: normalize any image to [H,W,3] float32 in [0,1]
# accepts either a NumPy array or a PIL.Image
# ——————————————————————————————————————————————
def _normalize_img(
    img: Union[np.ndarray, Image.Image],
    max_dim: int = _MAX_DIM
) -> np.ndarray:
    # 1) Convert to PIL.Image in RGB
    if isinstance(img, Image.Image):
        pil = img.convert("RGB")
    elif isinstance(img, np.ndarray):
        if img.ndim != 3 or img.shape[2] != 3:
            raise ValueError("NumPy array must be H*W*3")
        if img.dtype == np.uint8:
            pil = Image.fromarray(img, mode="RGB")
        else:
            # assume float in [0,1]
            arr = np.clip(img, 0.0, 1.0)
            pil = Image.fromarray((arr * 255).astype(np.uint8), mode="RGB")
    else:
        raise TypeError("Unsupported image type: use np.ndarray or PIL.Image")

    # 2) Resize so longest side == max_dim
    width, height = pil.size
    scale = max_dim / max(width, height)
    if scale < 1.0:
        new_size = (round(width * scale), round(height * scale))
        pil = pil.resize(new_size, Image.LANCZOS)

    # 3) Convert back to float32 NumPy [H,W,3] in [0,1]
    arr = np.array(pil).astype(np.float32) / 255.0
    return arr

# ——————————————————————————————————————————————
# Public API — interfaces unchanged
# ——————————————————————————————————————————————

def set_as_style(input_image: np.ndarray) -> None:
    """
    Set the global style image. Must be called before transfer_style().
    input_image: H*W*3 uint8 or float32 NumPy array (or PIL.Image via ndarray).
    """
    global _style_tf
    style_arr = _normalize_img(input_image)
    # Batch dimension + convert to tf.Tensor
    _style_tf = tf.convert_to_tensor(style_arr, dtype=tf.float32)[tf.newaxis, ...]

def transfer_style(input_image: np.ndarray) -> np.ndarray:
    """
    Stylize a new content image using the last style set by set_as_style().
    Returns an H*W*3 uint8 NumPy array.
    """
    if _style_tf is None:
        raise ValueError("Style image not set. Call set_as_style() first.")

    content_arr = _normalize_img(input_image)
    content_tf = tf.convert_to_tensor(content_arr, dtype=tf.float32)[tf.newaxis, ...]

    # Run the stylization graph (fast, GPU-ready)
    stylized_tf = _stylize_graph(content_tf, _style_tf)

    # Clip to [0,1], remove batch, convert to uint8 NumPy
    styl = tf.clip_by_value(stylized_tf, 0.0, 1.0)[0]
    out = (styl.numpy() * 255.0).round().astype(np.uint8)
    return out
