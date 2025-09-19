EasyPaint
=========

EasyPaint is a simple graphics painting program.

EasyPaint is a part of QtDesktop project.

EasyPaint is a part of [Razor-qt's](https://github.com/Razor-qt) [3rd party applications](https://github.com/Razor-qt/razor-qt/wiki/3rd-party-applications).

Installing
----------

Install EasyPaint using the commands, if you use CMake:

    cd ./EasyPaint
    cmake -DCMAKE_INSTALL_PREFIX=/usr
    make
    make install

License
-------

EasyPaint is distributed under the [MIT license](http://www.opensource.org/licenses/MIT).

Python scripting
----------------

Users can write Python functions that are automatically exposed in the app’s menus once loaded.

## Getting Started
### 1. Install / Build

Windows: Unzip the prebuilt package.

Linux/macOS: Build the application from source.

### 2. Python Setup

Ensure the corresponding version of Python is installed.

(Optional but recommended) Create a virtual environment for dependencies:

    python -m venv venv
    source venv/bin/activate   # Linux/macOS
    venv\Scripts\activate      # Windows

### 3. Install Dependencies

Use the provided requirements.txt to install extra libraries (e.g., Hugging Face, OpenCV):

pip install -r requirements.txt

### 4. Writing Scripts

Place your Python scripts in the designated directory.

Functions without a leading underscore (e.g., def my_tool():) will be exposed in the application’s menus when the script loads successfully.

Scripts can leverage all installed Python libraries.

- ✅ Cross-platform (Windows, Linux, macOS)
- ✅ Extensible via Python
- ✅ Supports external libs (Hugging Face, OpenCV, etc.)

## Python Scripts Overview

These Python scripts extend the paint application with **image generation, transformation, and enhancement features**.  
See scripts folder for examples.

---

### `depth2img.py`
This script focuses on **depth-guided image transformation**. It transforms images using depth maps to maintain scene geometry.

- **Functions**
  - `generate_depth_image` — Transforms an image based on depth information, preserving scene structure.

---

### `generate.py`
This script provides a **basic image generation entry point** for creating images from scratch.

- **Functions**
  - `generate_image` — Creates an image from scratch, typically using a generative model (e.g., diffusion).

---

### `img2img.py`
This script supports **image-to-image translation**, transforming an existing picture while keeping its overall structure.

- **Functions**
  - `generate_img2img` — Produces a modified version of an input image while maintaining its overall structure.

---

### `inpaint.py`
This script specializes in **inpainting**, i.e., filling in missing or masked areas with contextually appropriate content.

- **Functions**
  - `predict` — Performs inpainting by filling missing or masked image areas with context-aware content.

---

### `script.py`
This script provides **general-purpose image processing and procedural generation tools**.  
It includes filtering, corrections, resizing, and fractal/texture generators.

- **Functions**
  - `denoise_image` — Reduces unwanted noise or grain in an image.  
  - `blur_image` — Applies a blur filter.  
  - `deblur_image` — Restores sharpness to a blurred image.  
  - `gamma_correct` — Adjusts brightness and contrast through gamma correction.  
  - `enhance_contrast` — Improves visibility of details by adjusting contrast.  
  - `resize_image` — Rescales an image to new dimensions.  
  - `generate_mandelbrot` — Produces a Mandelbrot fractal image.  
  - `generate_julia` — Produces a Julia set fractal image.  
  - `generate_plasma` — Creates a plasma-like procedural texture.

---

### `style_transfer.py`
This script implements **style transfer**, allowing users to apply the artistic style of one image to another.

- **Functions**
  - `set_as_style` — Selects an image as a reference style.  
  - `transfer_style` — Transfers the reference style onto another image.

Enable Instruments -> Markup mode to use markup:

<img width="522" height="472" alt="Image" src="https://github.com/user-attachments/assets/1c988672-5008-48d5-afaf-e952605495f3" />

![image](EasyPaint_demo.jpg)
