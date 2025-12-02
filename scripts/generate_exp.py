import logging
from diffusers import logging as dlogging
import torch
import random
from diffusers import StableDiffusionPipeline, DPMSolverMultistepScheduler
import numpy as np
import re
from typing import List, Tuple

logger = logging.getLogger("diffusers")

device = "cuda"

# Load the pipeline correctly
pipe = StableDiffusionPipeline.from_pretrained(
    "runwayml/stable-diffusion-v1-5",
    torch_dtype=torch.float16,
    use_safetensors=True,
    safety_checker=None
).to(device)

# Replace the default scheduler with a DPM scheduler.
# This creates a DPMSolverMultistepScheduler instance with the same config as the pipeline's current scheduler.
pipe.scheduler = DPMSolverMultistepScheduler.from_config(pipe.scheduler.config, use_karras_sigmas=True, algorithm_type="dpmsolver++")

#pipe.enable_sequential_cpu_offload()
pipe.enable_xformers_memory_efficient_attention()
# Optional: Enable attention slicing (helps with memory usage) without quality loss.
pipe.enable_attention_slicing()
# Optional: For large image generation, you can also try VAE slicing.
pipe.enable_vae_slicing()

dlogging.set_verbosity_info()


# --- Prompt weighting helpers -------------------------------------------------
def _replace_innermost(text: str, open_ch: str, close_ch: str, weight: float) -> Tuple[str, bool]:
    pattern = re.compile(rf"\{open_ch}([^\\{open_ch}\{close_ch}]*)\{close_ch}")
    m = pattern.search(text)
    if not m:
        return text, False
    inner = m.group(1).strip()
    # skip if inner already ends with :number (explicit weight)
    if re.search(r":\s*[-+]?[0-9]*\.?[0-9]+$", inner):
        new_group = f"{open_ch}{inner}{close_ch}"
    else:
        # convert to explicit-weight parentheses to keep readable; not strictly necessary
        new_group = f"({inner}:{weight:.6g})"
    new_text = text[:m.start()] + new_group + text[m.end():]
    return new_text, True

def _prompt_to_weighted_subprompts(prompt: str, base_emph: float = 1.1) -> List[Tuple[str, float]]:
    """
    Parse prompt into list of (subprompt_text, cumulative_weight).
    - Splits by explicit separators (| or ,) only after grouping handling.
    - Honors explicit in-prompt weights like "dog:1.3".
    """
    txt = prompt
    parts: List[Tuple[str, float]] = []
    stack = [1.0]
    cur = []
    i = 0
    emph = base_emph
    deemph = 1.0 / base_emph

    while i < len(txt):
        ch = txt[i]
        if ch == "(":
            if cur:
                parts.append(("".join(cur).strip(), stack[-1]))
                cur = []
            stack.append(stack[-1] * emph)
            i += 1
        elif ch == "[":
            if cur:
                parts.append(("".join(cur).strip(), stack[-1]))
                cur = []
            stack.append(stack[-1] * deemph)
            i += 1
        elif ch == ")" or ch == "]":
            if cur:
                parts.append(("".join(cur).strip(), stack[-1]))
                cur = []
            if len(stack) > 1:
                stack.pop()
            i += 1
        else:
            cur.append(ch)
            i += 1
    if cur:
        parts.append(("".join(cur).strip(), stack[-1]))

    # split on separators and extract explicit numeric weights if provided
    final: List[Tuple[str, float]] = []
    for text_part, w in parts:
        if not text_part:
            continue
        for chunk in re.split(r"\s*\|\s*|\s*,\s*", text_part):
            s = chunk.strip()
            if not s:
                continue
            m = re.match(r"^(.*?):\s*([-+]?[0-9]*\.?[0-9]+)\s*$", s)
            if m:
                final.append((m.group(1).strip(), float(m.group(2)) * w))
            else:
                final.append((s, w))
    return final

# --- Embedding encoding and concatenation ------------------------------------
def _encode_subprompts_to_embeds(subprompts: List[Tuple[str, float]],
                                tokenizer,
                                text_encoder,
                                device: torch.device,
                                max_length: int) -> Tuple[torch.FloatTensor, torch.LongTensor]:
    """
    Batch-encode subprompts. Tokenize all non-empty subprompts together with padding='longest'
    so we only pad to the longest subprompt in this batch, not to model_max_length per item.
    Returns prompt_embeds (1, total_seq_len, dim) and attention_mask (1, total_seq_len).
    """
    device = torch.device(device)
    # Collect only non-empty subprompts
    items = [(t.strip(), w) for t, w in subprompts if t and t.strip()]
    if not items:
        # fallback to encode empty prompt once
        inputs = tokenizer("", padding=True, truncation=True, max_length=max_length, return_tensors="pt")
        input_ids = inputs.input_ids.to(device)
        attention_mask = inputs.attention_mask.to(device)
        with torch.no_grad():
            out = text_encoder(input_ids=input_ids, attention_mask=attention_mask)
        return out.last_hidden_state, attention_mask

    texts, weights = zip(*items)  # lists
    # Tokenize the batch, pad to longest in this batch only
    inputs = tokenizer(
        list(texts),
        padding="longest",
        truncation=True,
        max_length=max_length,
        return_tensors="pt",
    )
    input_ids = inputs.input_ids.to(device)
    attention_mask = inputs.attention_mask.to(device)

    with torch.no_grad():
        outputs = text_encoder(input_ids=input_ids, attention_mask=attention_mask)
        emb_batch = outputs.last_hidden_state  # (batch, seq_len, dim)

    batch_size, seq_len, dim = emb_batch.shape

    # Apply weights per-sentence by multiplying token embeddings for each row
    if any(float(w) != 1.0 for w in weights):
        # weights -> tensor (batch, 1, 1) to broadcast over seq_len and dim
        w_tensor = torch.tensor(weights, dtype=emb_batch.dtype, device=emb_batch.device).view(batch_size, 1, 1)
        emb_batch = emb_batch * w_tensor

    # Trim each row to real token length (attention_mask sum per row) and concatenate
    trimmed_embeds = []
    trimmed_masks = []
    for i in range(batch_size):
        real_len = int(attention_mask[i].sum().item())
        if real_len == 0:
            # keep at least one token (safety)
            real_len = 1
        emb_i = emb_batch[i:i+1, :real_len, :].contiguous()               # (1, real_len, dim)
        mask_i = attention_mask[i:i+1, :real_len].contiguous()            # (1, real_len)
        trimmed_embeds.append(emb_i)
        trimmed_masks.append(mask_i)

    # Concatenate along seq_len to form (1, total_seq_len, dim) and (1, total_seq_len)
    prompt_embeds = torch.cat(trimmed_embeds, dim=1)   # (1, total_seq_len, dim)
    attention_mask = torch.cat(trimmed_masks, dim=1)  # (1, total_seq_len)

    return prompt_embeds.to(device), attention_mask.to(device)

# Helper to build classifier-free guidance embeddings expected by the pipeline
def _build_prompt_embeds(pipe, prompt: str, negative_prompt: str = "", base_emph: float = 1.1):
    tokenizer = pipe.tokenizer
    text_encoder = pipe.text_encoder
    max_length = tokenizer.model_max_length

    # Parse and encode conditional prompt into weighted subprompts
    conditional_subs = _prompt_to_weighted_subprompts(prompt, base_emph=base_emph)
    cond_embeds, cond_mask = _encode_subprompts_to_embeds(conditional_subs, tokenizer, text_encoder, pipe.device, max_length)

    # Negative prompt handling
    if negative_prompt and negative_prompt.strip():
        negative_subs = _prompt_to_weighted_subprompts(negative_prompt, base_emph=base_emph)
        neg_embeds, neg_mask = _encode_subprompts_to_embeds(negative_subs, tokenizer, text_encoder, pipe.device, max_length)
    else:
        # encode empty negative prompt (standard CF guidance)
        inputs = tokenizer("", padding="max_length", truncation=True, max_length=max_length, return_tensors="pt")
        input_ids = inputs.input_ids.to(pipe.device)
        attention_mask = inputs.attention_mask.to(pipe.device)
        with torch.no_grad():
            out = pipe.text_encoder(input_ids=input_ids, attention_mask=attention_mask)
        neg_embeds = out.last_hidden_state
        neg_mask = attention_mask

    # The pipeline expects embeddings of shape (batch*2, seq_len, dim) for internal guidance handling.
    # We will return prompt_embeds and negative_prompt_embeds separately and let the pipeline handle concatenation.
    return cond_embeds, cond_mask, neg_embeds, neg_mask


def _pad_embeds_and_masks(embeds_a: torch.Tensor, mask_a: torch.Tensor,
                         embeds_b: torch.Tensor, mask_b: torch.Tensor,
                         dtype: torch.dtype = None, device: torch.device = None):
    """
    Right-pad embeddings and attention masks so embeds_a/embeds_b end up with same seq_len.
    Args:
      embeds_a: (batch, seq_a, dim)
      mask_a:   (batch, seq_a) or None
      embeds_b: (batch, seq_b, dim)
      mask_b:   (batch, seq_b) or None
    Returns:
      (embeds_a_padded, mask_a_padded, embeds_b_padded, mask_b_padded)
    """
    if dtype is None:
        dtype = embeds_a.dtype
    if device is None:
        device = embeds_a.device

    batch, seq_a, dim = embeds_a.shape
    _, seq_b, _ = embeds_b.shape
    target_seq = max(seq_a, seq_b)

    if seq_a < target_seq:
        pad_len = target_seq - seq_a
        pad_emb = torch.zeros((batch, pad_len, dim), dtype=dtype, device=device)
        embeds_a = torch.cat([embeds_a.to(device=device), pad_emb], dim=1)
        if mask_a is None:
            mask_a = torch.ones((batch, seq_a), dtype=torch.long, device=device)
        mask_pad = torch.zeros((batch, pad_len), dtype=mask_a.dtype, device=device)
        mask_a = torch.cat([mask_a.to(device=device), mask_pad], dim=1)
    else:
        embeds_a = embeds_a.to(device=device)
        if mask_a is not None:
            mask_a = mask_a.to(device=device)

    if seq_b < target_seq:
        pad_len = target_seq - seq_b
        pad_emb = torch.zeros((batch, pad_len, dim), dtype=dtype, device=device)
        embeds_b = torch.cat([embeds_b.to(device=device), pad_emb], dim=1)
        if mask_b is None:
            mask_b = torch.ones((batch, seq_b), dtype=torch.long, device=device)
        mask_pad = torch.zeros((batch, pad_len), dtype=mask_b.dtype, device=device)
        mask_b = torch.cat([mask_b.to(device=device), mask_pad], dim=1)
    else:
        embeds_b = embeds_b.to(device=device)
        if mask_b is not None:
            mask_b = mask_b.to(device=device)

    return embeds_a, mask_a, embeds_b, mask_b



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
def generate_image(prompt: str, negative_prompt: str = "", guidance_scale: float = 7.5, num_steps: int = 50, seed: int = -1):
    if seed == -1:
        seed = random.randint(0, 2**31 - 1)
    gen = torch.Generator(device=device).manual_seed(seed)
    logger.info(f'Using seed: {seed}')


    # === 1. Convert weighted prompt into embeddings ===
    cond_embeds, cond_mask, neg_embeds, neg_mask = _build_prompt_embeds(
        pipe,
        prompt,
        negative_prompt=negative_prompt,
        base_emph=1.1
    )

    # Cast embeddings to the same dtype as pipeline (important for fp16 models)
    cond_embeds = cond_embeds.to(dtype=pipe.text_encoder.dtype, device=pipe.device)
    neg_embeds = neg_embeds.to(dtype=pipe.text_encoder.dtype, device=pipe.device)
    cond_mask = cond_mask.to(pipe.device)
    neg_mask = neg_mask.to(pipe.device)


    # cond_embeds: (1, seq_cond, dim), cond_mask: (1, seq_cond)
    # neg_embeds: (1, seq_neg, dim), neg_mask: (1, seq_neg)
    cond_embeds, cond_mask, neg_embeds, neg_mask = _pad_embeds_and_masks(
        cond_embeds, cond_mask, neg_embeds, neg_mask,
        dtype=cond_embeds.dtype, device=cond_embeds.device
    )

    # === 2. Generate image using precomputed embeddings ===
    image = pipe(
        prompt_embeds=cond_embeds,
        attention_mask=cond_mask,
        negative_prompt_embeds=neg_embeds,
        negative_attention_mask=neg_mask,
        guidance_scale=guidance_scale,
        num_inference_steps=num_steps,
        generator=gen,
        callback_on_step_end=_callback
    ).images[0]
    return np.array(image)  # Convert RGB to BGR (OpenCV format)
