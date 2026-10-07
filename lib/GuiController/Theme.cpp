#include "Theme.h"

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(montserrat_digits_48);

namespace Theme {

const lv_font_t &small = lv_font_montserrat_14;
const lv_font_t &body = lv_font_montserrat_16;
const lv_font_t &digits = montserrat_digits_48;

uint32_t tempColor(float c) {
  if (c < 0)
    return 0x88AAFF;
  if (c < 10)
    return 0xAADDFF;
  if (c >= 28)
    return 0xFF5533;
  if (c >= 20)
    return 0xFFCC44;
  return TEXT;
}

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                uint32_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}

lv_obj_t *divider(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                  lv_coord_t w) {
  lv_obj_t *d = lv_obj_create(parent);
  lv_obj_remove_style_all(d); // No theme padding/border/radius
  lv_obj_set_size(d, w, 1);
  lv_obj_set_pos(d, x, y);
  lv_obj_set_style_bg_color(d, lv_color_hex(DIVIDER), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return d;
}

void cell(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *labelText,
          const char *value, uint32_t valueColor) {
  lv_obj_t *l = label(parent, labelText, &small, TEXT_DIM);
  lv_obj_set_pos(l, x, y);
  // Values are numbers, units and ASCII words ("°" is in the built-in font)
  lv_obj_t *v = label(parent, value, &body, valueColor);
  lv_obj_set_pos(v, x, y + 16);
}

} // namespace Theme
