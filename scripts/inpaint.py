import logging
import torch
import cv2
import random
import numpy as np
from PIL import Image
from diffusers import StableDiffusionInpaintPipeline
from diffusers import logging as dlogging

logger = logging.getLogger("diffusers")

# ------------------------------------------------------------------------------
# Select device and create the inpainting pipeline.
# ------------------------------------------------------------------------------
device = "cuda"
model_path = "runwayml/stable-diffusion-inpainting"

pipe = StableDiffusionInpaintPipeline.from_pretrained(
    model_path,
    torch_dtype=torch.float16,
).to(device)

# Enable performance optimizations if supported.
pipe.enable_sequential_cpu_offload()
pipe.enable_xformers_memory_efficient_attention()
pipe.enable_attention_slicing()

dlogging.set_verbosity_info()

class InpaintContext:
    def __init__(self, original_size):
        self.original_size = original_size

def _get_proportional_resize_dims(original_width, original_height, target_area=1024*512, divisor=8):
    """
    Computes new dimensions that preserve aspect ratio and approximate the target area.
    Rounds dimensions to be divisible by `divisor` (default: 8).

    Args:
        original_width (int): Width of the original image.
        original_height (int): Height of the original image.
        target_area (int): Target pixel area (default 1024x1024 = 1M).
        divisor (int): Ensures dimensions are divisible by this value.

    Returns:
        (int, int): New width and height.
    """
    aspect_ratio = original_width / original_height
    new_height = int((target_area / aspect_ratio) ** 0.5)
    new_width = int(aspect_ratio * new_height)

    # Round to nearest multiple of `divisor`
    new_width = max(divisor, round(new_width / divisor) * divisor)
    new_height = max(divisor, round(new_height / divisor) * divisor)

    return new_width, new_height


# ------------------------------------------------------------------------------
# Callback function to monitor generation.
# ------------------------------------------------------------------------------
def _make_callback(context):
    def _callback(iteration, step, timestep, extra_step_kwargs):
        if _check_interrupt():
            raise RuntimeError("Interrupt detected! Stopping generation.")

        image_tensor = extra_step_kwargs.get("latents")
        if image_tensor is not None:
            if hasattr(image_tensor, "is_sparse") and image_tensor.is_sparse:
                image_tensor = image_tensor.to_dense()

            with torch.no_grad():
                image_tensor = image_tensor / 0.18215
                decoded = pipe.vae.decode(image_tensor)
                image_tensor = (decoded.sample + 1) / 2

            if image_tensor.dim() == 3:
                image_tensor = image_tensor.unsqueeze(0)

            with torch.no_grad():
                preview = image_tensor.cpu().permute(0, 2, 3, 1).float().numpy()[-1]
                image = (preview * 255).clip(0, 255).astype(np.uint8)

                if context.original_size:
                    image = cv2.resize(image, context.original_size, interpolation=cv2.INTER_LANCZOS4)

                _send_image(image)

        return extra_step_kwargs
    return _callback


def _preprocess_mask(mask_image: np.ndarray) -> np.ndarray:
    """
    Preprocesses a binary mask image by reflecting borders equal to the image size,
    ensuring edge-touching contours are correctly handled. The mask is negated and 
    closed contours are filled.

    Args:
        mask_image (np.ndarray): The input mask as a NumPy array (RGB).

    Returns:
        np.ndarray: The processed and filled mask as a NumPy array (RGB).
    """
    gray = cv2.cvtColor(mask_image, cv2.COLOR_RGB2GRAY)
    _, binary = cv2.threshold(gray, 127, 255, cv2.THRESH_BINARY)

    # Invert the mask
    inverted = cv2.bitwise_not(binary)

    h, w = inverted.shape
    padded = cv2.copyMakeBorder(inverted, h, h, w, w, borderType=cv2.BORDER_REFLECT)

    # Find and fill contours
    contours, _ = cv2.findContours(padded, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    filled_padded = np.zeros_like(padded)
    cv2.drawContours(filled_padded, contours, -1, 255, thickness=cv2.FILLED)

    # Crop back to original size
    filled = filled_padded[h:h + h, w:w + w]

    return cv2.cvtColor(filled, cv2.COLOR_GRAY2RGB)

# ------------------------------------------------------------------------------
# Core function to perform inpainting.
# ------------------------------------------------------------------------------
def predict(input_image: np.ndarray,
            mask_image: np.ndarray,
            prompt: str,
            negative_prompt: str = "",
            seed: int = -1,
            num_inference_steps: int = 50,
            guidance_scale: float = 7.5) -> np.ndarray:
    """
    Inpaints masked regions of an input image using a Stable Diffusion inpainting pipeline.

    The function:
      - Preserves the original image's aspect ratio by resizing it proportionally to a 
        target area while ensuring dimensions are divisible by 8.
      - Preprocesses the input mask for robust contour handling.
      - Reconstructs the masked area based on a user-provided prompt and optional 
        negative prompt.
      - Rescales the final image back to the original dimensions.

    Args:
        input_image (np.ndarray): Input RGB image as a NumPy array.
        mask_image (np.ndarray): Corresponding RGB mask where white = masked region.
        prompt (str): Positive text prompt guiding image inpainting.
        negative_prompt (str, optional): Prompt describing what to avoid generating.
        seed (int, optional): Random seed for reproducibility.
        num_inference_steps (int, optional): Number of diffusion steps (higher = higher quality).
        guidance_scale (float, optional): Classifier-free guidance strength 
                                          (higher = more prompt adherence).

    Returns:
        np.ndarray: Inpainted image as a NumPy array in the original image's resolution.
    """
    original_size = (input_image.shape[1], input_image.shape[0])
    resize_dims = _get_proportional_resize_dims(*original_size)
    context = InpaintContext(original_size)

    processed_mask = _preprocess_mask(mask_image)

    img = Image.fromarray(input_image.astype('uint8'), 'RGB').resize(resize_dims, Image.LANCZOS)
    mask = Image.fromarray(processed_mask.astype('uint8'), 'RGB').resize(resize_dims, Image.LANCZOS)

    if seed == -1:
        seed = random.randint(0, 2**31 - 1)
        logger.info(f'Using seed: {seed}')
    generator = torch.Generator(device=device).manual_seed(seed)

    outcome = pipe(
        prompt=prompt,
        image=img,
        mask_image=mask,
        negative_prompt=negative_prompt,
        generator=generator,
        num_inference_steps=num_inference_steps,
        guidance_scale=guidance_scale,
        callback_on_step_end=_make_callback(context),
        callback_on_step_end_tensor_inputs=["latents"],
    )

    result = outcome.images[0].resize(original_size, resample=Image.LANCZOS)
    return np.array(result)
