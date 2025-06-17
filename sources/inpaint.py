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


def _preprocess_mask(mask_image: np.ndarray) -> np.ndarray:
    """
    Negates the binary mask and fills contours to ensure solid masked regions.

    Args:
        mask_image (np.ndarray): The input mask as a NumPy array (RGB).

    Returns:
        np.ndarray: The processed mask as a NumPy array (RGB).
    """
    gray = cv2.cvtColor(mask_image, cv2.COLOR_RGB2GRAY)
    _, binary = cv2.threshold(gray, 127, 255, cv2.THRESH_BINARY)
    inverted = cv2.bitwise_not(binary)
    contours, _ = cv2.findContours(inverted, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    filled = np.zeros_like(inverted)
    cv2.drawContours(filled, contours, -1, 255, thickness=cv2.FILLED)
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
    original_size = (input_image.shape[1], input_image.shape[0])  # (width, height)

    # Preprocess the mask: negate and fill contours
    processed_mask = _preprocess_mask(mask_image)

    # Convert input and mask to PIL and resize to 512x512
    img = Image.fromarray(input_image.astype('uint8'), 'RGB').resize((512, 512))
    mask = Image.fromarray(processed_mask.astype('uint8'), 'RGB').resize((512, 512))

    # Create torch generator with seed
    generator = torch.Generator(device=device).manual_seed(seed)

    # Run the inpainting pipeline
    outcome = pipe(
        prompt=prompt,
        image=img,
        mask_image=mask,
        negative_prompt=negative_prompt,
        generator=generator,
        callback_on_step_end=_callback,
        callback_on_step_end_tensor_inputs=["latents"],
    )

    # Resize result to original size and return as NumPy array
    result = outcome.images[0].resize(original_size, resample=Image.LANCZOS)
    return np.array(result)
