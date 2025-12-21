
import PIL.Image
import PIL.ImageDraw
import math

OUTPUT_C = "lib/GuiController/weather_icons.c"
OUTPUT_H = "lib/GuiController/weather_icons.h"
SIZE = 64 # Output size - Icons will be drawn to fill this 64x64 frame

def create_base_image():
    # Draw at 4x resolution for high-quality anti-aliasing
    return PIL.Image.new("RGBA", (SIZE * 4, SIZE * 4), (0, 0, 0, 0))

def draw_sun(draw, w, h):
    # Reference: wi-day-sunny
    # Center Circle (detached)
    cx, cy = w/2, h/2
    r_core = w * 0.18
    draw.ellipse((cx-r_core, cy-r_core, cx+r_core, cy+r_core), fill=(255, 255, 255))
    
    # Rays (detached)
    ray_start = w * 0.28
    ray_end = w * 0.42
    ray_width = int(w * 0.06)
    
    for i in range(8):
        angle = (math.pi * 2 / 8) * i
        sx = cx + math.cos(angle) * ray_start
        sy = cy + math.sin(angle) * ray_start
        ex = cx + math.cos(angle) * ray_end
        ey = cy + math.sin(angle) * ray_end
        draw.line((sx, sy, ex, ey), fill=(255, 255, 255), width=ray_width)

def draw_cloud_shape(draw, w, h, offset_y=0):
    # Reference: wi-cloud
    # Flat bottom, fluffy top.
    # Coordinates for smooth curve
    # Bottom Bar
    bx1, by1 = w*0.2, h*(0.55 + offset_y)
    bx2, by2 = w*0.8, h*(0.75 + offset_y)
    
    # Left Puff
    lx, ly, lr = w*0.25, h*(0.55+offset_y), w*0.15
    # Right Puff
    rx, ry, rr = w*0.75, h*(0.55+offset_y), w*0.15
    # Top Puff (Main)
    tx, ty, tr = w*0.4, h*(0.4+offset_y), w*0.22
    
    # Draw Puffs (Circles)
    fill = (255, 255, 255)
    
    # Left
    draw.ellipse((w*0.15, h*(0.45+offset_y), w*0.45, h*(0.75+offset_y)), fill=fill)
    # Right
    draw.ellipse((w*0.55, h*(0.45+offset_y), w*0.85, h*(0.75+offset_y)), fill=fill)
    # Top Center
    draw.ellipse((w*0.28, h*(0.25+offset_y), w*0.72, h*(0.70+offset_y)), fill=fill)
    
    # Bottom Flat fill
    draw.rectangle((w*0.2, h*(0.6+offset_y), w*0.8, h*(0.75+offset_y)), fill=fill)

def draw_cloud(draw, w, h):
    draw_cloud_shape(draw, w, h, 0)

def draw_rain(draw, w, h):
    draw_cloud_shape(draw, w, h, -0.1) # Shift cloud up
    
    # Rain lines (vertical tilted?)
    # Reference: wi-rain
    color = (255, 255, 255)
    line_w = int(w * 0.05)
    
    # 3 Drops
    # Left
    draw.line((w*0.35, h*0.7, w*0.30, h*0.9), fill=color, width=line_w)
    # Center
    draw.line((w*0.50, h*0.7, w*0.45, h*0.9), fill=color, width=line_w)
    # Right
    draw.line((w*0.65, h*0.7, w*0.60, h*0.9), fill=color, width=line_w)

def draw_snow(draw, w, h):
    draw_cloud_shape(draw, w, h, -0.1)
    
    # Snowflakes (dots)
    # Reference: wi-snow
    color = (255, 255, 255)
    r = w * 0.03
    
    # 3 dots
    centers = [
        (w*0.35, h*0.75),
        (w*0.50, h*0.85),
        (w*0.65, h*0.75)
    ]
    for cx, cy in centers:
        draw.ellipse((cx-r, cy-r, cx+r, cy+r), fill=color)

def draw_thunder(draw, w, h):
    draw_cloud_shape(draw, w, h, -0.1)
    
    # Bolt
    # Reference: wi-thunderstorm
    # Polygon
    points = [
        (w*0.50, h*0.65), # Top Mid (under cloud)
        (w*0.40, h*0.80), # Left Zig
        (w*0.48, h*0.80), # Right Zag inner
        (w*0.42, h*0.95)  # Bottom Tip
    ]
    
    # But needs thickness. Draw line segments or polygon.
    draw.line((w*0.52, h*0.65, w*0.42, h*0.80), fill=(255,255,255), width=int(w*0.05))
    draw.line((w*0.42, h*0.80, w*0.52, h*0.80), fill=(255,255,255), width=int(w*0.04))
    draw.line((w*0.52, h*0.80, w*0.45, h*0.95), fill=(255,255,255), width=int(w*0.05))


ICONS = {
    "weather_icon_sun": draw_sun,
    "weather_icon_cloud": draw_cloud,
    "weather_icon_rain": draw_rain,
    "weather_icon_snow": draw_snow,
    "weather_icon_thunder": draw_thunder
}

def generate():
    with open(OUTPUT_C, "w") as f, open(OUTPUT_H, "w") as fh:
        # Header includes
        f.write('#include "lvgl.h"\n\n')
        fh.write('#pragma once\n#include "lvgl.h"\n\n')

        for name, func in ICONS.items():
            img = create_base_image()
            draw = PIL.ImageDraw.Draw(img)
            w, h = img.size
            
            # Draw
            func(draw, w, h)
            
            # Resize down to Target Size with LANCZOS for best quality
            img = img.resize((SIZE, SIZE), PIL.Image.LANCZOS)
            
            # Convert to Data
            byte_data = bytearray()
            
            # LVGL CF_TRUE_COLOR_ALPHA: pixel is 3 bytes usually (BGR + alpha)? 
            # Or 2 bytes (RGB565) + 8bit Alpha?
            # Actually LV_IMG_CF_TRUE_COLOR_ALPHA depends on LV_COLOR_DEPTH.
            # If 16-bit: 16bit color + 8bit alpha = 3 bytes total.
            # Byte order: Low, High, Alpha.
            # We output WHITE (0xFFFF) + Alpha.
            
            for y in range(SIZE):
                for x in range(SIZE):
                    r, g, b, a = img.getpixel((x, y))
                    
                    # RGB565 white is 0xFFFF. Little Endian: 0xFF, 0xFF.
                    byte_data.append(0xFF)
                    byte_data.append(0xFF)
                    byte_data.append(a)
            
            # Write C array
            f.write(f'const uint8_t {name}_map[] = {{\n')
            
            # Format bytes nicely
            for i, b in enumerate(byte_data):
                if i % 12 == 0: 
                    f.write("  ")
                f.write(f"0x{b:02x}, ")
                if (i + 1) % 12 == 0:
                    f.write("\n")
            
            f.write("\n};\n\n")
            
            # Write Descriptor
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
