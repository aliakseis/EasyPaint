import torch
import cv2
import numpy as np
from PIL import Image
from diffusers import StableDiffusionImg2ImgPipeline

# ——————————————————————————————————————————————
# Load the Img2Img pipeline once for efficiency
# ——————————————————————————————————————————————
device = "cuda"
pipe = StableDiffusionImg2ImgPipeline.from_pretrained(
    "runwayml/stable-diffusion-v1-5",         # or your favorite SD checkpoint
    torch_dtype=torch.float16,
    use_safetensors=True,
).to(device)

pipe.enable_sequential_cpu_offload()
pipe.enable_xformers_memory_efficient_attention()
pipe.enable_attention_slicing()


class InpaintContext:
    def __init__(self, original_size):
        self.original_size = original_size


def _get_proportional_resize_dims(original_width, original_height,
                                  target_area=1024 * 512, divisor=8):
    aspect_ratio = original_width / original_height
    new_height = int((target_area / aspect_ratio)**0.5)
    new_width  = int(aspect_ratio * new_height)

    new_width  = max(divisor, round(new_width  / divisor) * divisor)
    new_height = max(divisor, round(new_height / divisor) * divisor)
    return new_width, new_height


def _make_callback(context):
    """
    Callback for preview images at each diffusion step.
    Signature for Img2Img: callback(step, timestep, latents)
    """
    def _callback(step, timestep, latents):
        # Interrupt check
        if _check_interrupt():
            raise RuntimeError("Interrupt detected! Stopping generation.")

        # latents: torch.FloatTensor [batch, channels, h, w]
        with torch.no_grad():
            # scale and decode latents via VAE
            image_tensor = latents / 0.18215
            decoded = pipe.vae.decode(image_tensor).sample  # decode returns an object
            img_tensor = (decoded + 1) / 2                  # [0,1] range

            # take last in batch, convert to NumPy HWC uint8
            preview = img_tensor.cpu().permute(0, 2, 3, 1).numpy()[-1]
            image = (preview * 255).clip(0, 255).astype(np.uint8)

            # resize back to original if needed
            if context.original_size:
                image = cv2.resize(image,
                                   context.original_size,
                                   interpolation=cv2.INTER_LANCZOS4)

            _send_image(image)

    return _callback


def generate_img2img(input_image: np.ndarray,
                     prompt: str,
                     negative_prompt: str = "",
                     strength: float = 0.15,
                     guidance_scale: float = 7.5,
                     seed: int = 21) -> np.ndarray:
    """
    Transforms an input image using StableDiffusionImg2ImgPipeline.

    Args:
        input_image    : H*W*3 uint8 RGB NumPy array.
        prompt         : text prompt to guide generation.
        negative_prompt: text to steer away unwanted artifacts.
        strength       : 0.0 == keep original ; 1.0 == full generation.
        guidance_scale : classifier-free guidance scale.
        seed           : reproducible RNG seed.

    Returns:
        H*W*3 uint8 RGB NumPy array of the result.
    """
    original_size = (input_image.shape[1], input_image.shape[0])
    resize_dims  = _get_proportional_resize_dims(*original_size)
    context = InpaintContext(original_size)

    # PIL image for the pipeline
    init_img = Image.fromarray(input_image, "RGB") \
                   .resize(resize_dims, Image.LANCZOS)

    generator = torch.Generator(device=device).manual_seed(seed)

    output = pipe(
        prompt=prompt,
        negative_prompt=negative_prompt,
        init_image=init_img,
        strength=strength,
        guidance_scale=guidance_scale,
        generator=generator,
        callback=_make_callback(context),
        callback_steps=1,      # call back every step
    )

    # take the first (and only) output image, resize back and return as NumPy
    result = output.images[0].resize(original_size, resample=Image.LANCZOS)
    return np.array(result)
