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

// Shared by every list row (one style instead of per-row local properties)
static lv_style_t styleRow;

void init() {
  lv_style_init(&styleRow);
  lv_style_set_width(&styleRow, LV_PCT(100));
  lv_style_set_border_side(&styleRow, LV_BORDER_SIDE_TOP);
  lv_style_set_border_width(&styleRow, 1);
  lv_style_set_border_color(&styleRow, lv_color_hex(DIVIDER));
  lv_style_set_pad_column(&styleRow, 8);
  lv_style_set_layout(&styleRow, LV_LAYOUT_FLEX);
  lv_style_set_flex_flow(&styleRow, LV_FLEX_FLOW_ROW);
  lv_style_set_flex_cross_place(&styleRow, LV_FLEX_ALIGN_CENTER);
  lv_style_set_flex_track_place(&styleRow, LV_FLEX_ALIGN_CENTER);
}

lv_obj_t *plainBox(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t *subtitle(lv_obj_t *parent, const char *text) {
  lv_obj_t *l = label(parent, text, &small, TEXT_DIM);
  lv_obj_set_pos(l, SIDE, SUBTITLE_Y);
  return l;
}

lv_obj_t *list(lv_obj_t *parent, lv_coord_t y) {
  lv_obj_t *l = lv_obj_create(parent);
  lv_obj_remove_style_all(l);
  lv_obj_set_pos(l, SIDE, y);
  lv_obj_set_size(l, 240 - 2 * SIDE, 320 - y);
  lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scrollbar_mode(l, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  return l;
}

lv_obj_t *row(lv_obj_t *list, lv_coord_t h) {
  lv_obj_t *r = plainBox(list);
  lv_obj_add_style(r, &styleRow, 0);
  lv_obj_set_height(r, h);
  lv_obj_add_flag(r, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  return r;
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
