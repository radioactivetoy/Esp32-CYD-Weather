#include "BusView.h"
#include "DataManager.h"
#include "GuiController.h"
#include "NetworkManager.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);

// --- Live ETA countdown ---
// Arrival times are fetched every ~60s; in between, tick() counts the visible
// labels down from the fetch time. The pointers belong to liveScreen and are
// dropped when LVGL deletes that screen (auto_del after a screen change).
static const int MAX_ROWS = 6;
static lv_obj_t *liveLabels[MAX_ROWS];
static int liveSeconds[MAX_ROWS];
static int liveCount = 0;
static lv_obj_t *liveScreen = nullptr;
static uint32_t liveFetchedAt = 0;

static void onBusScreenDeleted(lv_event_t *e) {
  if (lv_event_get_target(e) == liveScreen) {
    liveScreen = nullptr;
    liveCount = 0;
  }
}

// Seconds left for an arrival, accounting for time since the fetch
static int remainingSeconds(int secondsAtFetch) {
  int elapsed = liveFetchedAt ? (int)((millis() - liveFetchedAt) / 1000) : 0;
  int s = secondsAtFetch - elapsed;
  return s < 0 ? 0 : s;
}

static void setEtaLabel(lv_obj_t *label, int seconds) {
  char buf[16];
  if (seconds < 60)
    snprintf(buf, sizeof(buf), "Prop");
  else
    snprintf(buf, sizeof(buf), "%d min", seconds / 60);
  lv_label_set_text(label, buf);

  int mins = seconds / 60;
  uint32_t col = 0x00FF00;
  if (mins <= 2)
    col = 0xFF4500;
  else if (mins <= 5)
    col = 0xFFFF00;
  lv_obj_set_style_text_color(label, lv_color_hex(col), 0);
}

void BusView::forgetLiveLabels() {
  liveScreen = nullptr;
  liveCount = 0;
}

void BusView::tick() {
  if (!liveScreen)
    return;
  for (int i = 0; i < liveCount; i++)
    setEtaLabel(liveLabels[i], remainingSeconds(liveSeconds[i]));
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
  Serial.println("BusView: Start Show");

  lv_obj_t *new_scr = lv_obj_create(NULL);
  if (!new_scr) {
    Serial.println("BusView: Screen create failed (out of mem)");
    return;
  }
  Serial.println("BusView: Screen Created");
  lv_obj_clean(new_scr);

  // Take over the live countdown; the previous screen's labels are dropped
  liveScreen = new_scr;
  liveCount = 0;
  liveFetchedAt = data.lastUpdate;
  lv_obj_add_event_cb(new_scr, onBusScreenDeleted, LV_EVENT_DELETE, NULL);

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
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0); // Transparent like others
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 5, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(header, LV_OBJ_FLAG_EVENT_BUBBLE); // Bubble clicks up

  lv_obj_t *title = lv_label_create(header);
  if (data.stopName.length() > 0) {
    lv_label_set_text(title, GuiController::sanitize(data.stopName).c_str());
  } else {
    lv_label_set_text_fmt(title, "Stop: %s", data.stopCode.c_str());
  }

  lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(title, 160);
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), 0); // Cyan
  lv_obj_set_style_text_font(title, &Fonts::text20, 0); // Accents: "Plaça"
  Serial.println("BusView: Title Set");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0); // Left Aligned (No Icon)

  // Which stop of several (tap to switch)
  GuiController::createPageDots(header, GuiController::busStopCount,
                                GuiController::getBusIndex());

  // Time
  struct tm timeinfo;
  lv_obj_t *time_lb = lv_label_create(header);
  if (getLocalTime(&timeinfo, 10)) {
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lv_label_set_text(time_lb, timeStr);
  } else {
    lv_label_set_text(time_lb, "--:--");
  }
  lv_obj_set_style_text_color(time_lb, lv_color_hex(0xAAAAAA), 0); // Grey
  lv_obj_set_style_text_font(time_lb, &lv_font_montserrat_20, 0);
  lv_obj_align(time_lb, LV_ALIGN_TOP_RIGHT, 0, 0); // Top aligned
  GuiController::setActiveTimeLabel(time_lb);

  // Status Dot
  lv_obj_t *dot = lv_obj_create(header);
  lv_obj_set_size(dot, 10, 8); // Wider (was 8x8)
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_align_to(dot, time_lb, LV_ALIGN_OUT_LEFT_MID, -7, 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

  uint32_t dotColor = 0x00AA00; // Dark Green
  if (DataManager::isBusUpdating(GuiController::getBusIndex())) {
    dotColor = 0xFFFF00; // Yellow
  } else if (data.lastUpdate == 0 ||
             (millis() - data.lastUpdate > 60000)) { // 60s Stale
    dotColor = 0xFF0000; // Red
  }
  lv_obj_set_style_bg_color(dot, lv_color_hex(dotColor), 0);
  Serial.println("BusView: Time & Dot Created");

  // List
  lv_obj_t *list = lv_obj_create(new_scr);
  Serial.println("BusView: List Created");
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
    const char *msg;
    if (data.lastUpdate != 0)
      msg = "No buses right now.";
    else if (DataManager::isBusUpdating(GuiController::getBusIndex()))
      msg = "Fetching arrivals...";
    else if (NetworkManager::getAppId().isEmpty() ||
             NetworkManager::getAppKey().isEmpty())
      msg = "TMB App ID / Key not set.\nAdd them in the web settings.";
    else
      msg = "Can't reach TMB.\nCheck the App ID / Key.\nRetrying soon.";

    lv_obj_t *lbl = lv_label_create(list);
    lv_label_set_text(lbl, msg);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_top(lbl, 80, 0);
    Serial.println("BusView: Empty List");
  } else {
    int idx = 0;
    int limit = MAX_ROWS;
    for (const auto &arr : data.arrivals) {
      if (idx >= limit)
        break;

      lv_obj_t *row = lv_obj_create(list);
      if (!row)
        break;
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
      lv_label_set_text(dest, GuiController::sanitize(arr.destination).c_str());
      lv_obj_set_flex_grow(dest, 1);
      lv_label_set_long_mode(dest, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_obj_set_style_text_color(dest, lv_color_hex(0xDDDDDD), 0);
      lv_obj_set_style_text_font(dest, &Fonts::text14, 0); // Accents

      lv_obj_t *timeLbl = lv_label_create(row);
      lv_obj_set_width(timeLbl, 55);
      lv_obj_set_style_text_align(timeLbl, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_style_text_font(timeLbl, &lv_font_montserrat_14, 0);
      // No opacity pulse here: an LV_ANIM_REPEAT_INFINITE anim on a child
      // can fire after auto_del frees the old screen.
      setEtaLabel(timeLbl, remainingSeconds(arr.seconds));
      liveLabels[liveCount] = timeLbl;
      liveSeconds[liveCount] = arr.seconds;
      liveCount++;

      idx++;
    }
  }

  lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
  if (anim == 2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  else if (anim == -2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_TOP;

  lv_scr_load_anim(new_scr, anim_type, (anim == 0) ? 0 : 300, 0, true);
}
