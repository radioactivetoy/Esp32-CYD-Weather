#pragma once

#include <lvgl.h>

// Montserrat with a fallback font for accented Latin-1 characters (à é ç ñ ·
// ...) and the euro sign, generated from the same Montserrat-Medium.ttf as
// the LVGL built-ins (montserrat_latin1_*.c, made with lv_font_conv).
// Use these for any text that can contain place names or currency symbols.
namespace Fonts {

void init(); // Call once after lv_init()

extern lv_font_t text14;
extern lv_font_t text16;
extern lv_font_t text20;

} // namespace Fonts
