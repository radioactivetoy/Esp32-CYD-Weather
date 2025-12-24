#include "BusView.h"
#include "GuiController.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_20);

static void opa_anim_cb(void *obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)obj, v, 0);
}

lv_color_t BusView::getBusLineColor(const String &line, lv_color_t &textColor) {
  textColor = lv_color_hex(0xFFFFFF);

  if (line.startsWith("H"))
    return lv_color_hex(0x002E6E);
  else if (line.startsWith("V"))
    return lv_color_hex(0x6FA628);
  else if (line.startsWith("D"))
    return lv_color_hex(0x8956A0);
  else if (line.startsWith("X"))
    return lv_color_hex(0x000000);
  else if (line.startsWith("N"))
    return lv_color_hex(0x002E6E);
  else if (line.length() <= 3 && line.toInt() != 0)
    return lv_color_hex(0xCC0000);

  return lv_color_hex(0xCC0000);
}

void BusView::show(const BusData &data, int anim) {
  GuiController::currentApp = GuiController::APP_BUS;

  lv_obj_t *new_scr = lv_obj_create(NULL);
  lv_obj_clean(new_scr);

  lv_obj_add_event_cb(new_scr, GuiController::handleGesture, LV_EVENT_GESTURE,
                      NULL);
  lv_obj_add_event_cb(new_scr, GuiController::handleScreenClick,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_set_style_bg_color(new_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  // HEADER
  lv_obj_t *header = lv_obj_create(new_scr);
  lv_obj_set_size(header, LV_PCT(100), 40);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0xCC0000), 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 5, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(header, LV_OBJ_FLAG_EVENT_BUBBLE); // Bubble clicks up

  lv_obj_t *icon = lv_img_create(header);
  lv_img_set_src(icon, &bus_icon);
  lv_img_set_zoom(icon, 128);
  lv_obj_align(icon, LV_ALIGN_LEFT_MID, 5, 0);

  lv_obj_t *title = lv_label_create(header);
  if (data.stopName.length() > 0) {
    lv_label_set_text(title, GuiController::sanitize(data.stopName));
  } else {
    lv_label_set_text_fmt(title, "Stop: %s", data.stopCode.c_str());
  }
  lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(title, 190);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

  // Time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lv_obj_t *time_lb = lv_label_create(header);
    lv_label_set_text(time_lb, timeStr);
    lv_obj_set_style_text_color(time_lb, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(time_lb, &lv_font_montserrat_20, 0);
    lv_obj_align(time_lb, LV_ALIGN_RIGHT_MID, 0, 0);
    GuiController::setActiveTimeLabel(time_lb);
  }

  // List
  lv_obj_t *list = lv_obj_create(new_scr);
  lv_obj_set_size(list, LV_PCT(100), 280);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_row(list, 0, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_EVENT_BUBBLE); // Bubble clicks up

  if (data.arrivals.empty()) {
    lv_obj_t *lbl = lv_label_create(list);
    lv_label_set_text(lbl, "No buses found or API Error.");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
  } else {
    int idx = 0;
    int limit = 6;
    for (const auto &arr : data.arrivals) {
      if (idx >= limit)
        break;

      lv_obj_t *row = lv_obj_create(list);
      lv_obj_set_size(row, LV_PCT(100), 44);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE); // Bubble clicks from row

      uint32_t bg_col =
          (idx % 2 == 0) ? 0x101010 : 0x202020; // 333333 -> 202020
      lv_obj_set_style_bg_color(row, lv_color_hex(bg_col), 0);
      lv_obj_set_style_border_width(row, 2, 0); // Increased 1->2
      lv_obj_set_style_border_color(row, lv_color_hex(0x777777), 0);
      lv_obj_set_style_border_opa(row, LV_OPA_70, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_pad_all(row, 5, 0);
      lv_obj_set_style_pad_column(row, 8, 0);

      lv_color_t txtCol;
      lv_color_t badgeCol = getBusLineColor(arr.line, txtCol);

      lv_obj_t *lineBox = lv_obj_create(row);
      lv_obj_set_size(lineBox, 40, 28);
      lv_obj_set_style_bg_color(lineBox, badgeCol, 0);
      lv_obj_set_style_radius(lineBox, 4, 0);
      lv_obj_set_style_border_width(lineBox, 0, 0);
      lv_obj_clear_flag(lineBox,
                        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

      lv_obj_t *lineLbl = lv_label_create(lineBox);
      lv_label_set_text(lineLbl, arr.line.c_str());
      lv_obj_center(lineLbl);
      lv_obj_set_style_text_color(lineLbl, txtCol, 0);
      lv_obj_set_style_text_font(lineLbl, &lv_font_montserrat_14, 0);

      lv_obj_t *dest = lv_label_create(row);
      lv_label_set_text(dest, GuiController::sanitize(arr.destination));
      lv_obj_set_flex_grow(dest, 1);
      lv_label_set_long_mode(dest, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_obj_set_style_text_color(dest, lv_color_hex(0xDDDDDD), 0);
      lv_obj_set_style_text_font(dest, &lv_font_montserrat_14, 0);

      lv_obj_t *timeLbl = lv_label_create(row);
      lv_label_set_text(timeLbl, arr.text.c_str());
      lv_obj_set_width(timeLbl, 55);
      lv_obj_set_style_text_align(timeLbl, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_style_text_font(timeLbl, &lv_font_montserrat_14, 0);

      uint32_t eta_col = 0x00FF00;
      int mins = arr.seconds / 60;
      if (mins <= 2) {
        eta_col = 0xFF4500;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, timeLbl);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_40);
        lv_anim_set_time(&a, 800);
        lv_anim_set_playback_delay(&a, 100);
        lv_anim_set_playback_time(&a, 500);
        lv_anim_set_repeat_delay(&a, 100);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, opa_anim_cb);
        lv_anim_start(&a);
      } else if (mins <= 5)
        eta_col = 0xFFFF00;
      lv_obj_set_style_text_color(timeLbl, lv_color_hex(eta_col), 0);

      idx++;
    }
  }

  lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
  if (anim == 1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
  else if (anim == -1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
  else if (anim == 2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  else if (anim == -2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_TOP;

  int time = (anim == 0) ? 0 : 300;
  lv_scr_load_anim(new_scr, anim_type, time, 0, true);
}
