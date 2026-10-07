#include "BusView.h"
#include "DataManager.h"
#include "GuiController.h"
#include "NetworkManager.h"
#include "Theme.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);

// --- Live ETA countdown ---
// Arrival times are fetched every ~60s; in between, tick() counts the visible
// labels down from the fetch time. The pointers belong to liveScreen and are
// dropped when LVGL deletes that screen (auto_del after a screen change).
static const int MAX_ROWS = BusView::MAX_ROWS;
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
    snprintf(buf, sizeof(buf), "Due");
  else
    snprintf(buf, sizeof(buf), "%d min", seconds / 60);
  lv_label_set_text(label, buf);

  int mins = seconds / 60;
  // Colour only when it matters: due now / within 2 / within 5 minutes
  uint32_t col = Theme::TEXT;
  if (seconds < 60)
    col = Theme::ALERT;
  else if (mins <= 2)
    col = 0xFF9900;
  else if (mins <= 5)
    col = Theme::WARN;
  lv_obj_set_style_text_color(label, lv_color_hex(col), 0);
}

void BusView::updateEtas(const BusData &data) {
  if (!liveScreen)
    return;
  // Rows match data.arrivals in order (GuiController checked the layout)
  int n = min(liveCount, (int)data.arrivals.size());
  for (int i = 0; i < n; i++)
    liveSeconds[i] = data.arrivals[i].seconds;
  liveFetchedAt = data.lastUpdate;
  tick();
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
                      LV_EVENT_SHORT_CLICKED, NULL); // Not after a long press

  lv_obj_set_style_bg_color(new_scr, lv_color_hex(Theme::BG), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  // Header: stop name (or code until the name is known), dots per stop
  String title = data.stopName.length() > 0 ? data.stopName
                                            : String("Stop ") + data.stopCode;
  lv_obj_t *header =
      GuiController::createHeader(new_scr, title.c_str(),
                                  GuiController::busStopCount,
                                  GuiController::getBusIndex());
  // Long press: body = refresh now, header = device info
  GuiController::attachLongPress(new_scr, header);

  char buf[48];
  if (GuiController::busStopCount > 1)
    snprintf(buf, sizeof(buf), "Stop %s \xC2\xB7 tap for next stop",
             data.stopCode.c_str());
  else
    snprintf(buf, sizeof(buf), "Stop %s", data.stopCode.c_str());
  lv_obj_t *sub = Theme::subtitle(new_scr, buf);
  lv_obj_set_style_text_font(sub, &Fonts::text14, 0); // "·"

  lv_obj_t *list = Theme::list(new_scr, Theme::CONTENT_Y);

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

    lv_obj_t *lbl = Theme::label(list, msg, &Theme::body, Theme::TEXT_DIM);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(lbl, 70, 0);
  } else {
    int idx = 0;
    for (const auto &arr : data.arrivals) {
      if (idx >= MAX_ROWS)
        break;

      lv_obj_t *row = Theme::row(list, 40);

      // Line badge in TMB's colours (the colour means something here)
      lv_color_t txtCol;
      lv_color_t badgeCol = getBusLineColor(arr.line, txtCol);
      lv_obj_t *badge = Theme::plainBox(row);
      lv_obj_set_size(badge, 40, 24);
      lv_obj_set_style_radius(badge, 4, 0);
      lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(badge, badgeCol, 0);
      lv_obj_t *lineLbl = lv_label_create(badge);
      lv_label_set_text(lineLbl, arr.line.c_str());
      lv_obj_set_style_text_color(lineLbl, txtCol, 0);
      lv_obj_set_style_text_font(lineLbl, &lv_font_montserrat_14, 0);
      lv_obj_center(lineLbl);

      lv_obj_t *dest = Theme::label(
          row, GuiController::sanitize(arr.destination).c_str(),
          &Fonts::text16, Theme::TEXT); // Accents: "Plaça"
      lv_obj_set_flex_grow(dest, 1);
      lv_label_set_long_mode(dest, LV_LABEL_LONG_SCROLL_CIRCULAR);

      lv_obj_t *timeLbl = lv_label_create(row);
      lv_obj_set_width(timeLbl, 58);
      lv_obj_set_style_text_align(timeLbl, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_style_text_font(timeLbl, &Theme::body, 0);
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
