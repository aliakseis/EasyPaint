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

See scripts folder for examples.

Enable Instruments -> Markup mode to use markup:

<img width="522" height="472" alt="Image" src="https://github.com/user-attachments/assets/1c988672-5008-48d5-afaf-e952605495f3" />

![image](EasyPaint_demo.jpg)
