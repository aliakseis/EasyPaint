from diffusers import StableDiffusionImg2ImgPipeline
import torch, cv2, numpy as np
from PIL import Image

# Load once, move to GPU, enable memory optimizations
device = "cuda"
pipe = StableDiffusionImg2ImgPipeline.from_pretrained(
    "runwayml/stable-diffusion-v1-5",
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
    # Prepare sizes & PIL image
    h, w = input_image.shape[:2]
    context = InpaintContext((w, h))
    new_w, new_h = _get_proportional_resize_dims(w, h)
    init_img = Image.fromarray(input_image, "RGB").resize((new_w, new_h), Image.LANCZOS)

    gen = torch.Generator(device=device).manual_seed(seed)

    # *** Notice `image=init_img` (not `init_image`) *** 
    out = pipe(
        prompt=prompt,
        negative_prompt=negative_prompt,
        image=init_img,
        strength=strength,
        guidance_scale=guidance_scale,
        generator=gen,
        callback_on_step_end=_make_callback(context),
        callback_on_step_end_tensor_inputs=["latents"],
    )

    # Final resize back to original
    result = out.images[0].resize((w, h), resample=Image.LANCZOS)
    return np.array(result)
