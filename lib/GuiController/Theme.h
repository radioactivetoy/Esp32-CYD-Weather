#pragma once

#include <lvgl.h>

// App-wide look: black background, hairline dividers instead of boxes,
// dim small labels with bright values, colour only where it means something.
namespace Theme {

// --- Colours ---
// Pure black and neutral greys: on this TN panel dark tinted colours (e.g.
// navy) show up as bright blue, especially viewed from below.
constexpr uint32_t BG = 0x000000;        // Screen background
constexpr uint32_t TEXT = 0xFFFFFF;      // Values, titles
constexpr uint32_t TEXT_SOFT = 0xDDDDDD; // Header clock
constexpr uint32_t TEXT_DIM = 0xA8A8A8;  // Labels, secondary info
constexpr uint32_t DIVIDER = 0x333333;   // Hairlines, empty bar tracks
constexpr uint32_t RAIN = 0x44BBFF;      // Precipitation
constexpr uint32_t GOOD = 0x55DD55;      // Positive change, low UV, ...
constexpr uint32_t BAD = 0xFF6666;       // Negative change
constexpr uint32_t WARN = 0xFFDD33;      // Moderate
constexpr uint32_t ALERT = 0xFF4444;     // Due now, very high

// Temperature as colour: icy blue .. white .. amber .. red
uint32_t tempColor(float celsius);

// --- Fonts (built-in Montserrat + generated) ---
extern const lv_font_t &small;  // 14px: labels (12px was unreadable)
extern const lv_font_t &body;   // 16px: values
extern const lv_font_t &digits; // 48px: big temperature (digits, -, ° only)

// --- Building blocks ---
// Plain label, no theme styles beyond font and colour.
lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                uint32_t color);

// 1px horizontal hairline at (x, y), w wide.
lv_obj_t *divider(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w);

// Small dim label above a bright value, top-left at (x, y).
void cell(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *label,
          const char *value, uint32_t valueColor = TEXT);

} // namespace Theme
