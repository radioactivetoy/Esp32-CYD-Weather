
import os
import requests
import cairosvg
from io import BytesIO
from PIL import Image

# CONFIGURATION
WIDTH = 64
HEIGHT = 64
OUTPUT_C = "lib/GuiController/weather_icons.c"
OUTPUT_H = "lib/GuiController/weather_icons.h"
TRANSPARENT_KEY = (0, 255, 0)  # Standard Green for chroma key

# URL MAP
ICON_URLS = {
    "weather_icon_sun": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-day-sunny.svg',
    "weather_icon_part_cloud": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-day-cloudy.svg',
    "weather_icon_cloud": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-cloud.svg',
    "weather_icon_fog": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-fog.svg',
    "weather_icon_drizzle": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-sprinkle.svg',
    "weather_icon_showers": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-showers.svg',
    "weather_icon_rain": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-rain.svg',
    "weather_icon_snow": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-snow.svg',
    "weather_icon_thunder": 'https://raw.githubusercontent.com/erikflowers/weather-icons/master/svg/wi-thunderstorm.svg',
    "bus_icon": 'https://icons.getbootstrap.com/assets/icons/bus-front.svg'
}

def rgb888_to_rgb565(r, g, b):
    val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    low = val & 0xFF
    high = (val >> 8) & 0xFF
    return bytes([low, high])

def fetch_and_convert():
    print("Generating C files with Cairosvg...")
    
    with open(OUTPUT_C, "w") as f, open(OUTPUT_H, "w") as fh:
        # Header includes
        f.write('#include "lvgl.h"\n\n')
        fh.write('#pragma once\n#include "lvgl.h"\n\n')

        for name, url in ICON_URLS.items():
            print(f"Processing {name} from {url}...")
            try:
                # 1. Fetch SVG
                resp = requests.get(url)
                resp.raise_for_status()
                svg_data = resp.content

                # 2. Convert to PNG using Cairosvg
                # Note: We want white icons usually, but SVGs might be black.
                # Erik flowers icons are usually black.
                # We can inject a style or just process the pixels later.
                # Let's render as is (Black) and map pixels.
                
                png_data = cairosvg.svg2png(bytestring=svg_data, output_width=WIDTH, output_height=HEIGHT)
                
                # 3. Process with PIL
                img = Image.open(BytesIO(png_data)).convert("RGBA")
                result_bytes = bytearray()
                
                # Create a 64x64 buffer
                # Note: cairosvg might preserve aspect ratio, so we paste into center
                canvas = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
                
                # Center the image
                w, h = img.size
                x = (WIDTH - w) // 2
                y = (HEIGHT - h) // 2
                canvas.paste(img, (x, y), img) # Use alpha channel as mask
                
                pixels = canvas.load()
                
                for y in range(HEIGHT):
                    for x in range(WIDTH):
                        r, g, b, a = pixels[x, y]
                        
                        if a < 100:
                            # Transparent -> Green
                            result_bytes.extend(rgb888_to_rgb565(*TRANSPARENT_KEY))
                        else:
                            # Opaque
                            if name == "bus_icon":
                                # Bus Logic: Keep colors
                                # But actually boost visibility
                                result_bytes.extend(rgb888_to_rgb565(r, g, b))
                            else:
                                # Weather Logic: Force White
                                result_bytes.extend(rgb888_to_rgb565(255, 255, 255))

                # Write C array
                f.write(f'const uint8_t {name}_map[] = {{\n')
                for i, b in enumerate(result_bytes):
                    if i % 16 == 0:
                        f.write("  ")
                    f.write(f"0x{b:02x}, ")
                    if (i + 1) % 16 == 0:
                        f.write("\n")
                f.write("\n};\n\n")

                # Write Descriptor
                f.write(f'const lv_img_dsc_t {name} = {{\n')
                f.write('  .header.always_zero = 0,\n')
                f.write(f'  .header.w = {WIDTH},\n')
                f.write(f'  .header.h = {HEIGHT},\n')
                f.write('  .data_size = ' + str(len(result_bytes)) + ',\n')
                f.write('  .header.cf = LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED,\n')
                f.write(f'  .data = {name}_map,\n')
                f.write('};\n\n')

                # Header decl
                fh.write(f'LV_IMG_DECLARE({name});\n')

            except Exception as e:
                print(f"Error processing {name}: {e}")

    print(f"Success: Generated {OUTPUT_C} and {OUTPUT_H}")

if __name__ == "__main__":
    fetch_and_convert()
