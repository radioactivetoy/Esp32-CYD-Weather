#include "StockView.h"
#include "DataManager.h"
#include "GuiController.h"
#include "NetworkManager.h"
#include "Theme.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);

// Price with the quote's currency: "$187.20", "€24.31", "1234.00 CHF".
static void formatPrice(char *buf, size_t len, float price,
                        const String &currency) {
  const char *symbol = nullptr;
  if (currency == "USD")
    symbol = "$";
  else if (currency == "EUR")
    symbol = "\xE2\x82\xAC"; // €
  else if (currency == "GBP")
    symbol = "\xC2\xA3"; // £
  else if (currency == "JPY" || currency == "CNY")
    symbol = "\xC2\xA5"; // ¥

  const char *fmt = (price < 1.0f) ? "%.4f" : "%.2f";
  char num[24];
  snprintf(num, sizeof(num), fmt, price);

  if (symbol)
    snprintf(buf, len, "%s%s", symbol, num);
  else if (currency.length() > 0) // e.g. "GBp" (pence), "CHF"
    snprintf(buf, len, "%s %s", num, currency.c_str());
  else
    snprintf(buf, len, "%s", num);
}

void StockView::show(const std::vector<StockItem> &data, int anim) {
  GuiController::currentApp = GuiController::APP_STOCK;

  lv_obj_t *new_scr = lv_obj_create(NULL);
  if (!new_scr) {
    Serial.println("StockView: Screen create failed (out of mem)");
    return;
  }

  lv_obj_add_event_cb(new_scr, GuiController::handleGesture, LV_EVENT_GESTURE,
                      NULL);
  lv_obj_add_event_cb(new_scr, GuiController::handleScreenClick,
                      LV_EVENT_SHORT_CLICKED, NULL); // Not after a long press

  lv_obj_set_style_bg_color(new_scr, lv_color_hex(Theme::BG), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  lv_obj_t *header = GuiController::createHeader(new_scr, "Markets", 0, 0);
  // Long press: body = refresh now, header = device info
  GuiController::attachLongPress(new_scr, header);

  // Subtitle: how many symbols and when they were last updated (local time,
  // so it stays correct without being redrawn)
  char buf[48];
  uint32_t last = DataManager::getStockLastUpdate();
  time_t nowT = time(nullptr);
  if (last != 0 && nowT > 1700000000) {
    time_t at = nowT - (time_t)((millis() - last) / 1000);
    struct tm tmAt;
    localtime_r(&at, &tmAt);
    char hm[8];
    strftime(hm, sizeof(hm), "%H:%M", &tmAt);
    snprintf(buf, sizeof(buf), "%u symbols \xC2\xB7 updated %s",
             (unsigned)data.size(), hm);
  } else {
    snprintf(buf, sizeof(buf), "%u symbols", (unsigned)data.size());
  }
  lv_obj_t *sub = Theme::subtitle(new_scr, buf);
  lv_obj_set_style_text_font(sub, &Fonts::text14, 0); // "·"

  lv_obj_t *list = Theme::list(new_scr, Theme::CONTENT_Y);

  if (data.empty()) {
    const char *msg = NetworkManager::getStockSymbols().length() == 0
                          ? "No symbols configured.\nAdd them in the web "
                            "settings."
                          : "No quotes received.\nRetrying soon.";
    lv_obj_t *lbl = Theme::label(list, msg, &Theme::body, Theme::TEXT_DIM);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(lbl, 70, 0);
  } else {
    for (const auto &item : data) {
      lv_obj_t *row = Theme::row(list, 46);

      // Left: symbol over its currency
      lv_obj_t *left = Theme::plainBox(row);
      lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
      lv_obj_set_flex_grow(left, 1);
      lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
      Theme::label(left, item.symbol.c_str(), &Theme::body, Theme::TEXT);
      if (item.currency.length() > 0)
        Theme::label(left, item.currency.c_str(), &Theme::small,
                     Theme::TEXT_DIM);

      // Right: price over the day's change (the only coloured value)
      lv_obj_t *right = Theme::plainBox(row);
      lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                            LV_FLEX_ALIGN_END);

      formatPrice(buf, sizeof(buf), item.price, item.currency);
      Theme::label(right, buf, &Fonts::text16, Theme::TEXT); // Has € £ ¥

      snprintf(buf, sizeof(buf), "%+.2f%%", item.changePercent);
      Theme::label(right, buf, &Theme::small,
                   item.changePercent >= 0 ? Theme::GOOD : Theme::BAD);
    }

    // Keep the scroll position when new quotes rebuild the list
    GuiController::trackListScroll(list, 1);
  }

  lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
  if (anim == 2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  else if (anim == -2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_TOP;

  lv_scr_load_anim(new_scr, anim_type, (anim == 0) ? 0 : 300, 0, true);
}
