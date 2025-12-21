
import os
import requests
import cairosvg
import PIL.Image
import io

OUTPUT_C = "lib/GuiController/weather_icons.c"
OUTPUT_H = "lib/GuiController/weather_icons.h"
SIZE = 64

BASE_URL = "https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/"

ICON_MAP = {
    "weather_icon_sun": "wi-day-sunny.svg",
    "weather_icon_cloud": "wi-cloud.svg",
    "weather_icon_rain": "wi-rain.svg",
    "weather_icon_snow": "wi-snow.svg",
    "weather_icon_thunder": "wi-thunderstorm.svg",
    # Moon Phases
    "moon_new": "wi-moon-new.svg",
    "moon_wax_cresc": "wi-moon-waxing-crescent-3.svg",
    "moon_first_q": "wi-moon-first-quarter.svg",
    "moon_wax_gibb": "wi-moon-waxing-gibbous-3.svg",
    "moon_full": "wi-moon-full.svg",
    "moon_wan_gibb": "wi-moon-waning-gibbous-3.svg",
    "moon_third_q": "wi-moon-third-quarter.svg",
    "moon_wan_cresc": "wi-moon-waning-crescent-3.svg"
}

def download_and_convert(svg_filename):
    url = BASE_URL + svg_filename
    print(f"Downloading {url}...")
    try:
        r = requests.get(url)
        r.raise_for_status()
        svg_data = r.content
        
        # Convert to PNG using cairosvg
        # We render it slightly larger to ensure quality then resize, 
        # or render directly to target size.
        # SVGs might have intrinsic size. We can force output width/height.
        png_data = cairosvg.svg2png(bytestring=svg_data, output_width=SIZE, output_height=SIZE)
        
        return png_data
    except Exception as e:
        print(f"Error processing {svg_filename}: {e}")
        return None

def generate():
    with open(OUTPUT_C, "w") as f, open(OUTPUT_H, "w") as fh:
        # Header includes
        f.write('#include "lvgl.h"\n\n')
        fh.write('#pragma once\n#include "lvgl.h"\n\n')

        for name, filename in ICON_MAP.items():
            png_bytes = download_and_convert(filename)
            if not png_bytes:
                continue
                
            img = PIL.Image.open(io.BytesIO(png_bytes))
            img = img.convert("RGBA")
            
            # Ensure it is 64x64
            if img.size != (SIZE, SIZE):
                img = img.resize((SIZE, SIZE), PIL.Image.LANCZOS)
            
            byte_data = bytearray()
            
            # Convert to White Mask (Alpha as intensity)
            for y in range(SIZE):
                for x in range(SIZE):
                    r, g, b, a = img.getpixel((x, y))
                    
                    # Original icons are black. 
                    # If pixel is black (0,0,0) and alpha is high, we want it to be user-colored.
                    # We always output White (0xFFFF) so LVGL tinting works.
                    # We use the alpha from the image as the alpha for the mask.
                    # BUT: if the icon is physically black, does that affect things?
                    # The alpha channel is usually the shape. 
                    # Let's check if the SVG renders as black on transparent.
                    # Yes, standard icons usually black.
                    # So we take 'a' as our alpha.
                    
                    # Little Endian RGB565 for White: 0xFF, 0xFF
                    byte_data.append(0xFF)
                    byte_data.append(0xFF)
                    byte_data.append(a)
            
            # Write C array
            f.write(f'const uint8_t {name}_map[] = {{\n')
            
            for i, b in enumerate(byte_data):
                if i % 12 == 0: 
                    f.write("  ")
                f.write(f"0x{b:02x}, ")
                if (i + 1) % 12 == 0:
                    f.write("\n")
            
            f.write("\n};\n\n")
            
            # Descriptor
            f.write(f'const lv_img_dsc_t {name} = {{\n')
            f.write('  .header.always_zero = 0,\n')
            f.write(f'  .header.w = {SIZE},\n')
            f.write(f'  .header.h = {SIZE},\n')
            f.write('  .data_size = ' + str(len(byte_data)) + ',\n')
            f.write('  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n')
            f.write(f'  .data = {name}_map,\n')
            f.write('};\n\n')

            # Header decl
            fh.write(f'LV_IMG_DECLARE({name});\n')
        
        print(f"Generated {OUTPUT_C} and {OUTPUT_H}")

if __name__ == "__main__":
    generate()
