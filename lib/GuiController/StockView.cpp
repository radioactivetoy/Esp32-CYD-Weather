#include "StockView.h"
#include "GuiController.h"
#include "NetworkManager.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_24);

void StockView::show(const std::vector<StockItem> &data, int anim) {
  GuiController::currentApp = GuiController::APP_STOCK;

  lv_obj_t *new_scr = lv_obj_create(NULL);

  lv_obj_add_event_cb(new_scr, GuiController::handleGesture, LV_EVENT_GESTURE,
                      NULL);
  lv_obj_add_event_cb(new_scr, GuiController::handleScreenClick,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_set_style_bg_color(new_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  // Header
  lv_obj_t *header = lv_obj_create(new_scr);
  lv_obj_set_size(header, LV_PCT(100), 40);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 5, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(header,
                  LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Market Ticker");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFD700), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20,
                             0); // Title 20px
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  // Time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lv_obj_t *time_lb = lv_label_create(header);
    lv_label_set_text(time_lb, timeStr);
    lv_obj_set_style_text_color(time_lb, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(time_lb, &lv_font_montserrat_20,
                               0); // Upgrade 14->20
    lv_obj_align(time_lb, LV_ALIGN_TOP_RIGHT, 0, 0);
    GuiController::setActiveTimeLabel(time_lb);
  }

  // List
  lv_obj_t *list = lv_obj_create(new_scr);
  lv_obj_set_size(list, LV_PCT(100), 280);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 0, 0); // Fix: Remove default padding
  lv_obj_add_flag(list, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

  if (data.empty()) {
    lv_obj_t *lbl = lv_label_create(list);

    String msg = "Loading...";
    if (NetworkManager::getStockSymbols().length() == 0) {
      msg = "No Symbols Configured";
    } else {
      msg = "No Data Received.\nCheck Network";
    }

    lv_label_set_text(lbl, msg.c_str());
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
  } else {
    for (const auto &item : data) {
      lv_obj_t *row = lv_obj_create(list);
      lv_obj_set_size(row, LV_PCT(100), 70); // 70px Height
      lv_obj_set_style_bg_color(row, lv_color_hex(0x202020), 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_80, 0);
      lv_obj_set_style_border_color(row, lv_color_hex(0x777777), 0);
      lv_obj_set_style_border_width(row, 2, 0);
      lv_obj_set_style_border_opa(row, LV_OPA_70, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row,
                      LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

      // Symbol (Company Name) - Size 20, Left Mid
      lv_obj_t *sym = lv_label_create(row);
      lv_label_set_text(sym, item.symbol.c_str());
      lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_text_font(sym, &lv_font_montserrat_20,
                                 0); // 20px
      lv_obj_align(sym, LV_ALIGN_LEFT_MID, 5, 0);

      // Right Container (Price + Change)
      lv_obj_t *right_box = lv_obj_create(row);
      lv_obj_set_size(right_box, LV_SIZE_CONTENT, LV_PCT(100)); // Auto width
      lv_obj_align(right_box, LV_ALIGN_RIGHT_MID, -10,
                   0); // Added margin from right edge
      lv_obj_set_flex_flow(right_box, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(right_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_bg_opa(right_box, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(right_box, 0, 0);
      lv_obj_set_style_pad_all(right_box, 0, 0);
      lv_obj_set_style_pad_row(right_box, 0, 0); // Remove gap to save space
      lv_obj_clear_flag(right_box, LV_OBJ_FLAG_CLICKABLE);

      // Price - Size 20
      lv_obj_t *price = lv_label_create(right_box);
      char buf[32];
      if (item.price < 1.0)
        snprintf(buf, sizeof(buf), "$%.4f", item.price);
      else
        snprintf(buf, sizeof(buf), "$%.2f", item.price);
      lv_label_set_text(price, buf);
      lv_obj_set_style_text_color(price, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_text_font(price, &lv_font_montserrat_20,
                                 0); // 20px
      lv_obj_set_style_text_align(price, LV_TEXT_ALIGN_RIGHT, 0);

      // Change % - Size 16
      lv_obj_t *change = lv_label_create(right_box);
      snprintf(buf, sizeof(buf), "%+.2f%%", item.changePercent);
      lv_label_set_text(change, buf);
      lv_color_t cColor = (item.changePercent >= 0) ? lv_color_hex(0x00FF00)
                                                    : lv_color_hex(0xFF4444);
      lv_obj_set_style_text_color(change, cColor, 0);
      lv_obj_set_style_text_font(change, &lv_font_montserrat_14,
                                 0); // 14px
      lv_obj_set_style_text_align(change, LV_TEXT_ALIGN_RIGHT, 0);
    }
  }

  lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
  if (anim == 2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  else if (anim == -2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_TOP;

  lv_scr_load_anim(new_scr, anim_type, (anim == 0) ? 0 : 300, 0, true);
}
