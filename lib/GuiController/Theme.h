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
constexpr uint32_t DIVIDER = 0x666666;   // Hairlines (darker greys vanish on the panel)
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

// --- Layout (240x320 screen) ---
constexpr lv_coord_t SUBTITLE_Y = 42; // Below the 40px header
constexpr lv_coord_t CONTENT_Y = 62;  // Lists start here when there's a subtitle
constexpr lv_coord_t SIDE = 10;       // Left / right margin

void init(); // Call once after lv_init() (creates the shared styles)

// --- Building blocks ---
// Box with no padding, border or background; not clickable or scrollable.
lv_obj_t *plainBox(lv_obj_t *parent);

// Dim one-line caption under the header ("Next 36 hours", "Stop 378 ...").
lv_obj_t *subtitle(lv_obj_t *parent, const char *text);

// Vertical list from y to the bottom of the screen, SIDE margins. Scrolls
// when its rows don't fit; taps, long presses and gestures bubble up.
lv_obj_t *list(lv_obj_t *parent, lv_coord_t y);

// Full-width list row, h tall, hairline on top, children laid out left to
// right and vertically centred.
lv_obj_t *row(lv_obj_t *list, lv_coord_t h);

// Plain label, no theme styles beyond font and colour.
lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                uint32_t color);

// 1px horizontal hairline at (x, y), w wide.
lv_obj_t *divider(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w);

// Small dim label above a bright value, top-left at (x, y).
void cell(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *label,
          const char *value, uint32_t valueColor = TEXT);

} // namespace Theme
