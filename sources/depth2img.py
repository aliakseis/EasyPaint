import torch
import numpy as np
from PIL import Image
from diffusers import StableDiffusionDepth2ImgPipeline


# Load pipeline once for efficiency
device = "cuda"
pipe = StableDiffusionDepth2ImgPipeline.from_pretrained(
    "stabilityai/stable-diffusion-2-depth",
    torch_dtype=torch.float16,
    use_safetensors=True,
).to(device)

pipe.enable_sequential_cpu_offload()
pipe.enable_xformers_memory_efficient_attention()
pipe.enable_attention_slicing()


# Callback function to monitor generation
def _callback(iteration, step, timestep, extra_step_kwargs):
    """Interrupts generation if needed & sends intermediate images."""
    if _check_interrupt():
        raise RuntimeError("Interrupt detected! Stopping generation.")

    image_tensor = extra_step_kwargs.get("image")
    if image_tensor is None:
        return extra_step_kwargs  # No image available, return unchanged

    with torch.no_grad():
        processed_image = image_tensor.cpu().permute(0, 2, 3, 1).float().numpy()
        _send_image(processed_image[-1])  # Send preview during processing

    return extra_step_kwargs


# Core function to run Depth2Img
def generate_depth_image(image: np.ndarray, prompt: str, negative_prompt: str = "", strength: float = 0.15, seed: int = 21) -> np.ndarray:
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
    img = Image.fromarray(image.astype('uint8'), 'RGB')
    generator = torch.Generator(device=device).manual_seed(seed)

    outcome = pipe(
        prompt=prompt,
        image=img,
        negative_prompt=negative_prompt,
        strength=strength,
        generator=generator,
        callback_on_step_end=_callback,  # Integrate callback
    )

    return np.array(outcome.images[0])
