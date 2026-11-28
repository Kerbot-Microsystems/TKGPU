"""Regenerate the Windows icon from assets/tkgpu-logo.svg.

Development-only dependencies: resvg-py 0.5.0 and Pillow.
The resulting DIB icon needs no SVG renderer at runtime.
"""
from io import BytesIO
from pathlib import Path

from PIL import Image
import resvg_py

root = Path(__file__).resolve().parents[1]
png = resvg_py.svg_to_bytes(svg_path=str(root / 'assets/tkgpu-logo.svg'),
                          width=1024, skip_system_fonts=True)
logo = Image.open(BytesIO(png)).convert('RGBA')
surface = Image.new('RGBA', (1024, 1024))
surface.alpha_composite(logo, ((1024 - logo.width) // 2, (1024 - logo.height) // 2))
surface.save(root / 'assets/tkgpu-logo.ico', format='ICO', bitmap_format='bmp',
             sizes=[(size, size) for size in (16, 20, 24, 32, 48, 64, 128, 256)])
print('Generated assets/tkgpu-logo.ico from the repository SVG')
