"""Convert Meteocons (https://github.com/basmilius/weather-icons, MIT) into
LVGL images for lib/GuiController/weather_icons.{c,h}.

    uv run --with pillow python tools/convert_meteocons.py

- Downloads the 512px "fill" PNGs.
- Crops every icon to the same square: the union of all icons' visible
  artwork. At 64px the drawings then fill the image instead of leaving ~20%
  empty margin, so the main screen can show them 1:1 (sharp) and they keep
  their relative sizes.
- A few night icons are drawn noticeably smaller in Meteocons than their
  day counterparts; they get a small scale-up (NIGHT_SCALE).
- Resizes in premultiplied alpha (no dark fringes) and Floyd-Steinberg
  dithers to RGB565, which keeps the gradients smooth on the 16-bit panel.
- Writes LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 little-endian + alpha), matching
  LV_COLOR_16_SWAP 0.
"""

import io
import os
import urllib.request

from PIL import Image

SIZE = 64
ALPHA_VISIBLE = 16  # Alpha above this counts as artwork when cropping

# LVGL image name -> Meteocons name
ICONS = {
    "weather_icon_sun": "clear-day",
    "weather_icon_moon": "clear-night",
    "weather_icon_part_cloud": "partly-cloudy-day",
    "weather_icon_night_part_cloud": "partly-cloudy-night",
    "weather_icon_cloud": "overcast",
    "weather_icon_fog": "fog",
    "weather_icon_drizzle": "drizzle",
    "weather_icon_rain": "rain",
    "weather_icon_showers": "overcast-day-rain",
    "weather_icon_snow": "snow",
    "weather_icon_thunder": "thunderstorms",
    "weather_icon_cloud_night": "overcast-night",
    "weather_icon_fog_night": "fog-night",
    "weather_icon_rain_night": "overcast-night-rain",
    "weather_icon_snow_night": "overcast-night-snow",
    "weather_icon_thunder_night": "thunderstorms-night",
    "weather_icon_drizzle_night": "overcast-night-drizzle",
    "weather_icon_showers_night": "partly-cloudy-night-rain",
    "weather_icon_sleet": "sleet",
    "weather_icon_hail": "hail",
    "weather_icon_extreme_rain": "extreme-rain",
}

# Night icons drawn much smaller than their day counterparts (moon vs sun
# with rays, fog-night vs fog): scale the artwork up around its centre
NIGHT_SCALE = {
    "clear-night": 1.25,
    "fog-night": 1.2,
}

BASE_URLS = [
    "https://raw.githubusercontent.com/basmilius/weather-icons/dev/production/fill/png/512/",
    "https://raw.githubusercontent.com/basmilius/weather-icons/main/production/fill/png/512/",
]

OUTPUT_C = "lib/GuiController/weather_icons.c"
OUTPUT_H = "lib/GuiController/weather_icons.h"


def download(name):
    for base in BASE_URLS:
        url = base + name + ".png"
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=20) as resp:
                print(f"  OK {url}")
                return Image.open(io.BytesIO(resp.read())).convert("RGBA")
        except Exception as e:  # noqa: BLE001 - try the next mirror
            print(f"  FAIL {url}: {e}")
    raise SystemExit(f"Could not download {name}")


def visible_bbox(img):
    alpha = img.getchannel("A").point(lambda a: 255 if a > ALPHA_VISIBLE else 0)
    return alpha.getbbox()


def union_square(bboxes, width, height):
    x0 = min(b[0] for b in bboxes)
    y0 = min(b[1] for b in bboxes)
    x1 = max(b[2] for b in bboxes)
    y1 = max(b[3] for b in bboxes)
    side = max(x1 - x0, y1 - y0)
    side = int(side * 1.04)  # Tiny margin so antialiased edges aren't cut
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    left = int(round(cx - side / 2))
    top = int(round(cy - side / 2))
    left = max(0, min(left, width - side))
    top = max(0, min(top, height - side))
    return (left, top, left + side, top + side)


def scale_about_centre(img, factor):
    w, h = img.size
    big = img.resize((int(w * factor), int(h * factor)), Image.LANCZOS)
    left = (big.width - w) // 2
    top = (big.height - h) // 2
    return big.crop((left, top, left + w, top + h))


def resize_premultiplied(img, size):
    # Premultiplied resampling avoids dark halos around antialiased edges
    return img.convert("RGBa").resize((size, size), Image.LANCZOS).convert("RGBA")


def to_rgb565_dithered(img):
    """Floyd-Steinberg dither RGB to 5/6/5 bits; returns LVGL bytes."""
    w, h = img.size
    px = [[list(img.getpixel((x, y))) for x in range(w)] for y in range(h)]
    out = bytearray()
    levels = (31, 63, 31)
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[y][x]
            q = []
            for c, (v, n) in enumerate(zip((r, g, b), levels)):
                v = max(0.0, min(255.0, v))
                lvl = round(v * n / 255)
                q.append(lvl)
                err = v - lvl * 255 / n
                if a > 0:  # Only diffuse within visible artwork
                    for dx, dy, wgt in ((1, 0, 7), (-1, 1, 3), (0, 1, 5), (1, 1, 1)):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < w and 0 <= ny < h and px[ny][nx][3] > 0:
                            px[ny][nx][c] += err * wgt / 16
            rgb565 = (q[0] << 11) | (q[1] << 5) | q[2]
            out += bytes((rgb565 & 0xFF, rgb565 >> 8, int(a)))
    return out


def main():
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

    print("Downloading...")
    sources = {name: download(meteo) for name, meteo in ICONS.items()}

    first = next(iter(sources.values()))
    crop = union_square([visible_bbox(i) for i in sources.values()],
                        first.width, first.height)
    print(f"Shared crop: {crop} of {first.size}")

    with open(OUTPUT_C, "w", newline="\n") as fc, \
         open(OUTPUT_H, "w", newline="\n") as fh:
        fc.write("// Generated by tools/convert_meteocons.py from Meteocons\n"
                 "// (https://github.com/basmilius/weather-icons, MIT License)\n"
                 '#include "lvgl.h"\n\n')
        fh.write("#pragma once\n// Generated by tools/convert_meteocons.py\n"
                 '#include "lvgl.h"\n\n')

        for name, img in sources.items():
            art = img.crop(crop)
            factor = NIGHT_SCALE.get(ICONS[name])
            if factor:
                art = scale_about_centre(art, factor)
            data = to_rgb565_dithered(resize_premultiplied(art, SIZE))

            fc.write(f"const uint8_t {name}_map[] = {{\n")
            for i in range(0, len(data), 16):
                fc.write("  " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",\n")
            fc.write("};\n\n")
            fc.write(f"const lv_img_dsc_t {name} = {{\n"
                     "  .header.always_zero = 0,\n"
                     f"  .header.w = {SIZE},\n"
                     f"  .header.h = {SIZE},\n"
                     f"  .data_size = {len(data)},\n"
                     "  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n"
                     f"  .data = {name}_map,\n"
                     "};\n\n")
            fh.write(f"LV_IMG_DECLARE({name});\n")
            print(f"  {name}: {len(data)} bytes")

    print("Done.")


if __name__ == "__main__":
    main()
