import torch
import cv2
import numpy as np
from PIL import Image
from diffusers import StableDiffusionInpaintPipeline

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

class InpaintContext:
    def __init__(self, original_size):
        self.original_size = original_size

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
            seed: int = 21) -> np.ndarray:
    """
    Inpaints an image using Stable Diffusion Inpainting Pipeline and returns it
    in the original resolution.

    Parameters:
        input_image (np.ndarray): The source image as a NumPy array; must be RGB.
        mask_image (np.ndarray): The mask image as a NumPy array; must be RGB.
        prompt (str): The text prompt guiding the inpainting operation.
        negative_prompt (str, optional): Text prompt to suppress unwanted features.
        seed (int, optional): Random seed for reproducibility.

    Returns:
        np.ndarray: The inpainted image resized to original image dimensions.
    """
    # Store original image dimensions
    original_size = (input_image.shape[1], input_image.shape[0])
    context = InpaintContext(original_size)

    processed_mask = _preprocess_mask(mask_image)

    img = Image.fromarray(input_image.astype('uint8'), 'RGB').resize((1024, 1024))
    mask = Image.fromarray(processed_mask.astype('uint8'), 'RGB').resize((1024, 1024))

    generator = torch.Generator(device=device).manual_seed(seed)

    outcome = pipe(
        prompt=prompt,
        image=img,
        mask_image=mask,
        negative_prompt=negative_prompt,
        generator=generator,
        callback_on_step_end=_make_callback(context),
        callback_on_step_end_tensor_inputs=["latents"],
    )

    result = outcome.images[0].resize(original_size, resample=Image.LANCZOS)
    return np.array(result)
