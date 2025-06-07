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
    # Check for interrupt signal (your custom function)
    if _check_interrupt():
        raise RuntimeError("Interrupt detected! Stopping generation.")

    # Retrieve the latent tensor (by default, it’s passed as "latents")
    latents = extra_step_kwargs.get("latents")
    if latents is None:
        return extra_step_kwargs  # Nothing to decode, return unchanged

    # To avoid too much overhead, you could selectively decode at some intervals
    if step % 5 != 0:  # For example, decode only every 5 steps
        return extra_step_kwargs

    with torch.no_grad():
        # Decode latents to get an intermediate image
        decoded = pipe.vae.decode(latents)
        # Assume your batch is the first dimension; take the last image in the batch
        image_tensor = decoded[-1]

        # Post-process the tensor:
        # Typical VAE outputs lie in range [-1, 1]. Normalize to [0, 1] then convert to uint8.
        image_tensor = (image_tensor + 1) / 2  # Scale from [-1,1] to [0,1]
        image_np = image_tensor.clamp(0, 1).cpu().permute(1, 2, 0).numpy() * 255
        image_np = image_np.astype(np.uint8)

        _send_image(image_np)  # Your function to send or display the preview

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
        callback_on_step_end_tensor_inputs=["latents"],  # Ensure latents are passed
    )

    return np.array(outcome.images[0])
