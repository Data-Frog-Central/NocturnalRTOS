import sys
import os
from PIL import Image

def png_to_rgb565(image_path, output_path=None):
    try:
        img = Image.open(image_path)
    except Exception as e:
        print(f"Error opening image: {e}")
        return

    img = img.convert('RGB')
    width, height = img.size

    if not output_path:
        output_path = os.path.splitext(image_path)[0] + ".h"

    array_name = os.path.basename(os.path.splitext(image_path)[0]).lower() + "_data"

    print(f"Converting {image_path} ({width}x{height}) to {output_path}...")

    with open(output_path, 'w') as f:
        f.write("#ifndef BOOT_IMAGE_H\n")
        f.write("#define BOOT_IMAGE_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define BOOT_IMAGE_WIDTH  {width}\n")
        f.write(f"#define BOOT_IMAGE_HEIGHT {height}\n\n")
        f.write(f"const uint16_t {array_name}[{width * height}] = {{\n")

        pixels = list(img.getdata())
        line_count = 0
        
        for i, (r, g, b) in enumerate(pixels):
            r_565 = (r & 0xF8) << 8
            g_565 = (g & 0xFC) << 3
            b_565 = (b & 0xF8) >> 3
            
            rgb565 = r_565 | g_565 | b_565

            f.write(f"0x{rgb565:04X}, ")

            line_count += 1
            if line_count >= 12:
                f.write("\n")
                line_count = 0

        f.write("\n};\n\n")
        f.write("#endif // BOOT_IMAGE_H\n")

    print("Done!")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python png_to_rgb565.py <path_to_image.png>")
    else:
        png_to_rgb565(sys.argv[1])