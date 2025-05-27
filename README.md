# video-preview-generator
Small CLI & GUI tool to quickly generate 5x4 video preview grids with metadata, and optional watermark and text.

## Installation

Either download the Windows executable for the GUI (MacOS and Linux will follow) or clone the repository, create a local venv using `setup.bat` and run via `run_gui.bat`.

## Command-Line Arguments

Use the following command-line arguments to customize the output:

- `video`: **(positional)** Path to the input video file. *(Required)*
- `-o`, `--output`: Path to the output PNG file. If omitted, defaults to the input file directory.
- `--scale`: Scale factor for the output image resolution. Default is `1.0`. I don't recommend going lower or over `2.0`.
- `--logo-path`: Path to a PNG image to be used as a logo overlay.
- `--logo-opacity`: Opacity of the logo, from `0` (transparent) to `1` (fully opaque). Default is `1.0`.
- `--watermark-text`: Optional watermark text to display on the image.
- `--font-size`: Font size for the watermark text. Default is `25`.
- `--text-opacity`: Opacity of the watermark text, from `0` to `1`. Default is `1.0`.

Examples:
```bash
python cli.py input.mp4
```
```bash
python cli.py input.mp4 -o ./previews/output.png --logo-path logo.png --watermark-text "Sample" --font-size 50 --text-opacity 0.5
```


## Image Previews
![Screenshot 2025-05-27 18-35-40](https://github.com/user-attachments/assets/65c62cc6-075a-4997-9599-d7e50f00a3f7)

![vertical_preview](https://github.com/user-attachments/assets/f2dcc8d3-f28a-4ef9-a7ea-5d1db9e5d3bf)

![horizontal_preview](https://github.com/user-attachments/assets/f4aee5c2-08bb-4a60-977e-6306f89fff99)
