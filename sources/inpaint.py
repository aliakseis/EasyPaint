import torch
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

# ------------------------------------------------------------------------------
# Dummy helper functions.
# ------------------------------------------------------------------------------
def _check_interrupt():
    """
    Dummy interrupt check. Replace this with your own interrupt logic if needed.
    """
    return False

def _send_image(image_np):
    """
    Dummy sender function for intermediate results.
    Here we simply print that an update is available.
    """
    print("Intermediate image preview updated.")

# ------------------------------------------------------------------------------
# Callback function to monitor generation.
# ------------------------------------------------------------------------------
def _callback(iteration, step, timestep, extra_step_kwargs):
    """
    Callback to monitor the inpainting process during generation.
    
    Parameters:
        iteration: Overall iteration count.
        step: Current denoising step.
        timestep: Current timestep.
        extra_step_kwargs: Additional keyword arguments, which may include tensors.
    
    Returns:
        The (possibly modified) extra_step_kwargs.
    """
    # Interrupt check.
    if _check_interrupt():
        raise RuntimeError("Interrupt detected! Stopping generation.")

    # Try to obtain the tensor from extra_step_kwargs (e.g., latents).
    image_tensor = extra_step_kwargs.get("latents")
    if image_tensor is not None:
        # If the image tensor is sparse, convert it to a dense tensor.
        if hasattr(image_tensor, "is_sparse") and image_tensor.is_sparse:
            image_tensor = image_tensor.to_dense()
        with torch.no_grad():
            # Scale the latent tensor using the coefficient before decoding.
            image_tensor = image_tensor / 0.18215
            decoded = pipe.vae.decode(image_tensor)
            # Convert decoded output from [-1, 1] to [0, 1].
            image_tensor = (decoded.sample + 1) / 2

        # Ensure the tensor has a batch dimension.
        if image_tensor.dim() == 3:
            image_tensor = image_tensor.unsqueeze(0)

        # Convert tensor from [batch, channels, height, width] to [batch, height, width, channels]
        with torch.no_grad():
            processed_image = image_tensor.cpu().permute(0, 2, 3, 1).float().numpy()
            # Send the latest preview image (last in batch).
            _send_image(processed_image[-1])

    return extra_step_kwargs


def dummy(input_image: np.ndarray,
            mask_image: np.ndarray,
            seed: int = 21) -> np.ndarray:
    return mask_image


# ------------------------------------------------------------------------------
# Core function to perform inpainting.
# ------------------------------------------------------------------------------
def predict(input_image: np.ndarray,
            mask_image: np.ndarray,
            prompt: str,
            negative_prompt: str = "",
            seed: int = 21) -> np.ndarray:
    """
    Inpaints an image using Stable Diffusion Inpainting Pipeline.
    
    Parameters:
        input_image (np.ndarray): The source image as a NumPy array; must be RGB.
        mask_image (np.ndarray): The mask image as a NumPy array; must be RGB.
        prompt (str): The text prompt guiding the inpainting operation.
        negative_prompt (str, optional): Text prompt to suppress unwanted features.
        seed (int, optional): Random seed for reproducibility.
    
    Returns:
        np.ndarray: The resulting inpainted image as a NumPy array.
    """
    # Convert the source image to a PIL Image in RGB format and resize to 512x512.
    img = Image.fromarray(input_image.astype('uint8'), 'RGB').resize((512, 512))
    
    # Convert the mask image to a PIL Image in RGB format and resize to 512x512.
    mask = Image.fromarray(mask_image.astype('uint8'), 'RGB').resize((512, 512))
    
    # Create a torch Generator on the selected device with the specified seed.
    generator = torch.Generator(device=device).manual_seed(seed)
    
    # Run the inpainting pipeline.
    outcome = pipe(
        prompt=prompt,
        image=img,
        mask_image=mask,
        negative_prompt=negative_prompt,
        generator=generator,
        callback_on_step_end=_callback,                   # Monitor generation progress.
        callback_on_step_end_tensor_inputs=["latents"],     # Ensure latents are passed to the callback.
    )
    
    # Return the first output image from the generated batch as a NumPy array.
    return np.array(outcome.images[0])
