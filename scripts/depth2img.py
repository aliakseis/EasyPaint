import logging
import torch
import cv2
import random
import numpy as np
from PIL import Image
from diffusers import StableDiffusionDepth2ImgPipeline, DPMSolverMultistepScheduler
from diffusers import logging as dlogging

logger = logging.getLogger("diffusers")

# Load pipeline once for efficiency
device = "cuda"
pipe = StableDiffusionDepth2ImgPipeline.from_pretrained(
    "stabilityai/stable-diffusion-2-depth",
    torch_dtype=torch.float16,
    use_safetensors=True,
).to(device)

# Replace the default scheduler with a DPM scheduler.
# This creates a DPMSolverMultistepScheduler instance with the same config as the pipeline's current scheduler.
pipe.scheduler = DPMSolverMultistepScheduler.from_config(pipe.scheduler.config, use_karras_sigmas=True)


pipe.enable_sequential_cpu_offload()
pipe.enable_xformers_memory_efficient_attention()
pipe.enable_attention_slicing()

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


# Callback function to monitor generation
def _make_callback(context):
    def _callback(iteration, step, timestep, extra_step_kwargs):
        # Interrupt check
        if _check_interrupt():
            raise RuntimeError("Interrupt detected! Stopping generation.")

        # Try to get the tensor; if not, try "latents"
        image_tensor = extra_step_kwargs.get("image")
        if image_tensor is None:
            image_tensor = extra_step_kwargs.get("latents")
            if image_tensor is None:
                return extra_step_kwargs  # Nothing to process

            # Convert to dense if sparse
            if hasattr(image_tensor, "is_sparse") and image_tensor.is_sparse:
                image_tensor = image_tensor.to_dense()
            with torch.no_grad():
                image_tensor = image_tensor / 0.18215
                decoded = pipe.vae.decode(image_tensor)
                # Access the tensor inside the DecoderOutput object via .sample,
                # then scale from [-1, 1] to [0, 1]
                image_tensor = (decoded.sample + 1) / 2
        else:
            # If the provided image_tensor is sparse, convert to dense
            if hasattr(image_tensor, "is_sparse") and image_tensor.is_sparse:
                image_tensor = image_tensor.to_dense()

        # Ensure image_tensor has a batch dimension (dimensions: [batch, channels, height, width])
        if image_tensor.dim() == 3:
            image_tensor = image_tensor.unsqueeze(0)

        # Option: Only process every n-th step (e.g., every 5 steps)
        #if step % 5 != 0:
        #    return extra_step_kwargs

        with torch.no_grad():
            # Convert tensor shape from [batch, channels, height, width] to [batch, height, width, channels]
            #processed_image = image_tensor.cpu().permute(0, 2, 3, 1).float().numpy()
            #_send_image(processed_image[-1])  # Send the latest preview image
            preview = image_tensor.cpu().permute(0, 2, 3, 1).float().numpy()[-1]
            image = (preview * 255).clip(0, 255).astype(np.uint8)

            if context.original_size:
                image = cv2.resize(image, context.original_size, interpolation=cv2.INTER_LANCZOS4)

            _send_image(image)

        return extra_step_kwargs
    return _callback


# Core function to run Depth2Img
def generate_depth_image(input_image: np.ndarray, prompt: str, negative_prompt: str = "", strength: float = 0.15, guidance_scale: float = 7.5, num_steps: int = 50, seed: int = -1) -> np.ndarray:
    """Enhances an image using Stable Diffusion Depth2Img.

    Args:
        image (np.ndarray): Input image (should be RGB format).
        prompt (str): Text prompt for image transformation.
        negative_prompt (str, optional): Text prompt to avoid unwanted elements.
        strength (float, optional): Degree of modification (0.0 = original, 1.0 = full generation).
        seed (int, optional): Random seed for consistent output.

    Returns:
        np.ndarray: Transformed image.
    """
    #img = Image.fromarray(image.astype('uint8'), 'RGB')

    original_size = (input_image.shape[1], input_image.shape[0])
    resize_dims = _get_proportional_resize_dims(*original_size)
    context = InpaintContext(original_size)

    img = Image.fromarray(input_image.astype('uint8'), 'RGB').resize(resize_dims, Image.LANCZOS)

    if seed == -1:
        seed = random.randint(0, 2**31 - 1)
        logger.info(f'Using seed: {seed}')
    generator = torch.Generator(device=device).manual_seed(seed)

    outcome = pipe(
        prompt=prompt,
        image=img,
        negative_prompt=negative_prompt,
        strength=strength,
        guidance_scale=guidance_scale,
        num_inference_steps=num_steps,
        generator=generator,
        callback_on_step_end=_make_callback(context),  # Integrate callback
        callback_on_step_end_tensor_inputs=["latents"],  # Ensure latents are passed
    )

    #return np.array(outcome.images[0])
    result = outcome.images[0].resize(original_size, resample=Image.LANCZOS)
    return np.array(result)
