import urllib.request
import io
import sys

try:
    from PIL import Image
except ImportError:
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
    from PIL import Image

SIZE = 64

ICONS = {
    "weather_icon_sun":               "clear-day",
    "weather_icon_moon":              "clear-night",
    "weather_icon_part_cloud":        "partly-cloudy-day",
    "weather_icon_night_part_cloud":  "partly-cloudy-night",
    "weather_icon_cloud":             "overcast",
    "weather_icon_fog":               "fog",
    "weather_icon_drizzle":           "drizzle",
    "weather_icon_rain":              "rain",
    "weather_icon_showers":           "overcast-day-rain",
    "weather_icon_snow":              "snow",
    "weather_icon_thunder":           "thunderstorms",
}

# Try both known base URLs
BASE_URLS = [
    "https://raw.githubusercontent.com/basmilius/weather-icons/dev/production/fill/png/512/",
    "https://raw.githubusercontent.com/basmilius/weather-icons/main/production/fill/png/512/",
]

OUTPUT_C = "lib/GuiController/weather_icons.c"
OUTPUT_H = "lib/GuiController/weather_icons.h"

def try_download(meteocon_name):
    for base in BASE_URLS:
        url = base + meteocon_name + ".png"
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=15) as resp:
                data = resp.read()
            print(f"  OK: {url}")
            return data
        except Exception as e:
            print(f"  FAIL {url}: {e}")
    return None

def convert_to_lvgl(img_data):
    img = Image.open(io.BytesIO(img_data)).convert("RGBA")
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    byte_data = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            r, g, b, a = img.getpixel((x, y))
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            byte_data.append(rgb565 & 0xFF)
            byte_data.append((rgb565 >> 8) & 0xFF)
            byte_data.append(a)
    return byte_data

import os
os.chdir(os.path.join(os.path.dirname(__file__), ".."))

with open(OUTPUT_C, "w") as fc, open(OUTPUT_H, "w") as fh:
    fc.write('#include "lvgl.h"\n\n')
    fh.write('#pragma once\n#include "lvgl.h"\n\n')

    for name, meteocon_name in ICONS.items():
        print(f"Downloading {meteocon_name}...")
        raw = try_download(meteocon_name)
        if raw is None:
            print(f"  ERROR: could not download {meteocon_name}, skipping")
            continue

        byte_data = convert_to_lvgl(raw)

        fc.write(f'const uint8_t {name}_map[] = {{\n')
        for i, b in enumerate(byte_data):
            if i % 16 == 0:
                fc.write("  ")
            fc.write(f"0x{b:02x}, ")
            if (i + 1) % 16 == 0:
                fc.write("\n")
        fc.write("\n};\n\n")

        fc.write(f'const lv_img_dsc_t {name} = {{\n')
        fc.write('  .header.always_zero = 0,\n')
        fc.write(f'  .header.w = {SIZE},\n')
        fc.write(f'  .header.h = {SIZE},\n')
        fc.write(f'  .data_size = {len(byte_data)},\n')
        fc.write('  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n')
        fc.write(f'  .data = {name}_map,\n')
        fc.write('};\n\n')

        fh.write(f'LV_IMG_DECLARE({name});\n')
        print(f"  Converted {name} ({len(byte_data)} bytes)")

print("Done.")
