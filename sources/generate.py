import torch
from diffusers import StableDiffusionPipeline, DPMSolverMultistepScheduler
import numpy as np


device = "cuda"

# Load the pipeline correctly
pipe = StableDiffusionPipeline.from_pretrained(
    "runwayml/stable-diffusion-v1-5",
    torch_dtype=torch.float16
).to(device)

# Replace the default scheduler with a DPM scheduler.
# This creates a DPMSolverMultistepScheduler instance with the same config as the pipeline's current scheduler.
pipe.scheduler = DPMSolverMultistepScheduler.from_config(pipe.scheduler.config)

# Optional: Enable attention slicing (helps with memory usage) without quality loss.
pipe.enable_attention_slicing()
# Optional: For large image generation, you can also try VAE slicing.
pipe.enable_vae_slicing()

# Callback function to monitor generation
def _callback(iter, i, t, extra_step_kwargs):

    # https://huggingface.co/docs/diffusers/using-diffusers/callback
    if _check_interrupt():
        raise RuntimeError("Interrupt detected! Stopping generation.")

    latents = extra_step_kwargs.get("latents")
    with torch.no_grad():
        latents = 1 / 0.18215 * latents
        #latents = 1 / 0.13025 * latents

        image = pipe.vae.decode(latents).sample
        
        if image.dim() == 4:

            image = (image / 2 + 0.5).clamp(0, 1).cpu().permute(0, 2, 3, 1).float().numpy()
            #image = (image * 255).round().astype(np.uint8)

            _send_image(image[-1])

    return extra_step_kwargs

# Correct function signature with parameters
def generate_image(prompt: str, negative_prompt: str = "", guidance_scale: float = 7.5, num_steps: int = 50):
    """Generates an image using Stable Diffusion with monitoring."""
    image = pipe(
        prompt=prompt,
        negative_prompt=negative_prompt,
        guidance_scale=guidance_scale,
        num_inference_steps=num_steps,
        callback_on_step_end=_callback
    ).images[0]
    return np.array(image)  # Convert RGB to BGR (OpenCV format)
