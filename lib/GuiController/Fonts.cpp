#include "Fonts.h"

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(montserrat_latin1_14);
LV_FONT_DECLARE(montserrat_latin1_16);
LV_FONT_DECLARE(montserrat_latin1_20);

namespace Fonts {

lv_font_t text14;
lv_font_t text16;
lv_font_t text20;

// RAM copy of a built-in font with the fallback attached. Line metrics are
// widened to the fallback's so accented capitals (À, É) aren't clipped.
static void makeFont(lv_font_t &out, const lv_font_t &base,
                     const lv_font_t &fallback) {
  out = base;
  out.fallback = &fallback;
  if (fallback.line_height > out.line_height)
    out.line_height = fallback.line_height;
  if (fallback.base_line > out.base_line)
    out.base_line = fallback.base_line;
}

void init() {
  makeFont(text14, lv_font_montserrat_14, montserrat_latin1_14);
  makeFont(text16, lv_font_montserrat_16, montserrat_latin1_16);
  makeFont(text20, lv_font_montserrat_20, montserrat_latin1_20);
}

} // namespace Fonts
