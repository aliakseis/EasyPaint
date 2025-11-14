import logging
import torch
import cv2
import random
import numpy as np
from PIL import Image
from diffusers import (
    StableDiffusionControlNetImg2ImgPipeline,
    ControlNetModel,
    DPMSolverMultistepScheduler,
)
from diffusers import logging as dlogging

logger = logging.getLogger("diffusers")

device = "cuda"

# Base text-to-image model (choose one you have access to)
BASE_MODEL = "runwayml/stable-diffusion-v1-5"

# ControlNet depth model (widely used)
CONTROLNET_MODEL = "lllyasviel/sd-controlnet-depth"

# Load ControlNet and pipeline
controlnet = ControlNetModel.from_pretrained(
    CONTROLNET_MODEL, torch_dtype=torch.float16
)

pipe = StableDiffusionControlNetImg2ImgPipeline.from_pretrained(
    BASE_MODEL,
    controlnet=controlnet,
    torch_dtype=torch.float16,
    use_safetensors=True,
).to(device)

# Replace scheduler with DPM solver
pipe.scheduler = DPMSolverMultistepScheduler.from_config(pipe.scheduler.config, use_karras_sigmas=True)

pipe.enable_sequential_cpu_offload()
pipe.enable_xformers_memory_efficient_attention()
pipe.enable_attention_slicing()

class InpaintContext:
    def __init__(self, original_size):
        self.original_size = original_size


def _get_proportional_resize_dims(original_width, original_height, target_area=1024*512, divisor=8):
    aspect_ratio = original_width / original_height
    new_height = int((target_area / aspect_ratio) ** 0.5)
    new_width = int(aspect_ratio * new_height)
    new_width = max(divisor, round(new_width / divisor) * divisor)
    new_height = max(divisor, round(new_height / divisor) * divisor)
    return new_width, new_height


# Lightweight depth estimator (optional). Produces a single-channel depth-like image (uint8).
# If you already have a reliable depth map, skip this and pass it to generate_depth_image via control_image.
def _estimate_depth_map(input_image: np.ndarray, target_size=(512, 512)) -> Image.Image:
    gray = cv2.cvtColor(input_image, cv2.COLOR_RGB2GRAY)
    # Fast edge-aware smoothing + distance transform as a cheap depth proxy
    blur = cv2.bilateralFilter(gray, d=9, sigmaColor=75, sigmaSpace=75)
    edges = cv2.Canny(blur, 50, 150)
    inv_edges = 255 - edges
    dist = cv2.distanceTransform(inv_edges, cv2.DIST_L2, 5)
    norm = cv2.normalize(dist, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    depth = cv2.resize(norm, target_size, interpolation=cv2.INTER_LINEAR)
    # Convert to 3-channel image because ControlNet usually expects an RGB-like control image
    depth_rgb = cv2.cvtColor(depth, cv2.COLOR_GRAY2RGB)
    return Image.fromarray(depth_rgb)

# Callback function reused from your code
def _make_callback(context):
    def _callback(iteration, step, timestep, extra_step_kwargs):
        if _check_interrupt():
            raise RuntimeError("Interrupt detected! Stopping generation.")

        image_tensor = extra_step_kwargs.get("image")
        if image_tensor is None:
            image_tensor = extra_step_kwargs.get("latents")
            if image_tensor is None:
                return extra_step_kwargs

            if hasattr(image_tensor, "is_sparse") and image_tensor.is_sparse:
                image_tensor = image_tensor.to_dense()
            with torch.no_grad():
                image_tensor = image_tensor / 0.18215
                decoded = pipe.vae.decode(image_tensor)
                image_tensor = (decoded.sample + 1) / 2
        else:
            if hasattr(image_tensor, "is_sparse") and image_tensor.is_sparse:
                image_tensor = image_tensor.to_dense()

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


# Core function: uses Img2Img + ControlNet depth conditioning
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
    original_size = (input_image.shape[1], input_image.shape[0])
    resize_dims = _get_proportional_resize_dims(*original_size)
    context = InpaintContext(original_size)

    init_img = Image.fromarray(input_image.astype("uint8"), "RGB").resize(resize_dims, Image.LANCZOS)

    control_image = _estimate_depth_map(input_image, target_size=resize_dims)

    # control_image should be PIL.Image matching the init_img resolution
    control_image = control_image.resize(resize_dims, Image.LANCZOS)

    if seed == -1:
        seed = random.randint(0, 2**31 - 1)
        logger.info(f"Using seed: {seed}")
    generator = torch.Generator(device=device).manual_seed(seed)

    # You can tweak controlnet_conditioning_scale to control how strongly the depth map is followed
    outcome = pipe(
        prompt=prompt,
        negative_prompt=negative_prompt,
        image=init_img,
        control_image=control_image,
        strength=strength,
        guidance_scale=guidance_scale,
        num_inference_steps=num_steps,
        generator=generator,
        callback_on_step_end=_make_callback(context),
        controlnet_conditioning_scale=1.0,
    )

    result = outcome.images[0].resize(original_size, resample=Image.LANCZOS)
    return np.array(result)
