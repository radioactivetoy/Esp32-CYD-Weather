#include "GuiController.h"
#include "BusService.h"
#include "WeatherService.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstdio> // For sprintf
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_24);

// Screen dimensions
// Screen dimensions (Portrait)
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t
    buf[screenWidth * 30]; // Increased buffer (30 lines) - Fit in DRAM

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  static uint32_t flush_count = 0;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  if (flush_count++ % 100 == 0) {
    // Serial.printf("HEARTBEAT: Flush #%u\n", flush_count);
  }

  lv_disp_flush_ready(disp);
}

// Animation helper for pulsing effect
static void opa_anim_cb(void *obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)obj, v, 0);
}

bool GuiController::busScreenActive = false;
bool GuiController::isBusScreenActive() { return busScreenActive; }
void GuiController::updateBusCache(const BusData &data) { cachedBus = data; }

void GuiController::init() {
  lv_init();

  tft.begin();        /* TFT init */
  tft.setRotation(0); /* Portrait orientation (0 or 2) */

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 30);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  Serial.println("GuiController: LVGL and Display initialized.");
}

// Queue State
String GuiController::pendingMsg = "";
bool GuiController::needsUpdate = false;
static lv_obj_t *activeTimeLabel = NULL; // Track the time label

// Data Cache
WeatherData GuiController::cachedWeather;
BusData GuiController::cachedBus;

void GuiController::showLoadingScreen(const char *msg) {
  // Just verify thread safety for string copy?
  // We are protected by `dataMutex` when calling this from Main/Network
  // usually.
  if (msg)
    pendingMsg = String(msg);
  else
    pendingMsg = "Loading...";
  needsUpdate = true;
}

void GuiController::update() {
  if (needsUpdate) {
    needsUpdate = false;
    drawLoadingScreen(pendingMsg.c_str());
  }
  lv_timer_handler();
}

void GuiController::drawLoadingScreen(const char *msg) {
  // Make sure we are on the main thread (called from loop -> update)
  // Serial.printf("DEBUG: Drawing Loading Screen: %s\n", msg ? msg : "NULL");

  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);

  // Clear tracking
  activeTimeLabel = NULL;

  // Use a distinct background color for loading
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0000AA), 0); // Dark Blue
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, msg ? msg : "Loading...");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);

  // No force handler here. update() calls lv_timer_handler() immediately after.
}

void GuiController::handle(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    lv_timer_handler();
    delay(5);
  }
}

void GuiController::createWeatherIcon(lv_obj_t *parent, int code) {
  // Bitmap Icon Generator
  lv_obj_clean(parent);

  // Choose Icon Source based on WMO code
  const void *src = &weather_icon_cloud;     // Default
  lv_color_t color = lv_color_hex(0xFFFFFF); // Default White

  if (code == 0 || code == 1) {
    src = &weather_icon_sun;
    color = lv_color_hex(0xFFD700); // Gold
  } else if (code == 2 || code == 3) {
    src = &weather_icon_cloud;
    color = lv_color_hex(0xEEEEEE); // Light Grey
  } else if (code >= 45 && code <= 48) {
    src = &weather_icon_cloud;
    color = lv_color_hex(0xAAAAAA); // Fog Grey
  } else if (code >= 51 && code <= 67) {
    src = &weather_icon_rain;       // Drizzle/Rain
    color = lv_color_hex(0x00BFFF); // Blue
  } else if (code >= 71 && code <= 77) {
    src = &weather_icon_snow;
    color = lv_color_hex(0xE0FFFF); // Cyan
  } else if (code >= 80 && code <= 82) {
    src = &weather_icon_rain; // Showers
    color = lv_color_hex(0x00BFFF);
  } else if (code >= 85 && code <= 86) {
    src = &weather_icon_snow;
    color = lv_color_hex(0xE0FFFF);
  } else if (code >= 95) {
    src = &weather_icon_thunder;
    color = lv_color_hex(0x9370DB); // Purple
  }

  // Create Image
  lv_obj_t *img = lv_img_create(parent);
  lv_img_set_src(img, src);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

  // Apply Color
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(img, color, 0);
}

// Helper for Month Names
const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void formatDate(const char *input, char *output) {
  int y, m, d;
  if (sscanf(input, "%d-%d-%d", &y, &m, &d) == 3) {
    if (m >= 1 && m <= 12) {
      sprintf(output, "%d %s", d, months[m - 1]); // Format: "DD Month"
    } else {
      strcpy(output, input); // Fallback
    }
  } else {
    strcpy(output, input);
  }
}

// Toggle State: 0: Current, 1: Hourly, 2: Daily, 3: Trends Graph
// Toggle State: 0: Current, 1: Hourly, 2: Daily, 3: Trends Graph
static int forecastMode = 0;

static void toggle_forecast_cb(lv_event_t *e) {
  void *ptr = lv_event_get_user_data(e);
  if (!ptr) {
    Serial.println("FATAL ERROR: toggle_forecast_cb user_data is NULL!");
    return;
  }
  forecastMode = (forecastMode + 1) % 4;
  const WeatherData *data = (const WeatherData *)ptr;
  GuiController::showWeatherScreen(*data);
}

void GuiController::handleGesture(lv_event_t *e) {
  lv_obj_t *screen = lv_event_get_target(e);
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  // Serial.printf("DEBUG: Gesture Detected! Dir=%d, on Object=0x%p\n", dir,
  // screen);

  if (dir == LV_DIR_LEFT) {
    if (forecastMode < 3) {
      forecastMode++;
      showWeatherScreen(cachedWeather, 1); // Animate Left
    }
  } else if (dir == LV_DIR_RIGHT) {
    if (forecastMode > 0) {
      forecastMode--;
      showWeatherScreen(cachedWeather, -1); // Animate Right
    }
  } else if (dir == LV_DIR_TOP) {
    // Swipe UP: Show Bus Screen
    // Serial.println("DEBUG: Swipe UP -> Bus Screen");
    showBusScreen(cachedBus, 2);
  } else if (dir == LV_DIR_BOTTOM) {
    // Swipe DOWN: Refresh
    // Serial.println("DEBUG: Swipe DOWN -> Refresh");
    // Force Weather Screen via main logic? Or just switch back
    showWeatherScreen(cachedWeather, -2);
  }
}

void GuiController::showWeatherScreen(const WeatherData &data, int anim) {
  busScreenActive = false;
  uint32_t tStart = millis();
  // Serial.println("DEBUG: GuiController::showWeatherScreen START");

  // Update Cache (if not self)
  if (&data != &cachedWeather) {
    cachedWeather = data;
  }

  // Fix Memory Leak: Delete the previous screen after loading the new one?
  // LVGL `lv_scr_load_anim` with `auto_del=true` handles this.
  // But `lv_scr_load` does not.
  // We can track the old screen and delete it, or just use anim with 0 time.

  lv_obj_t *act_scr = lv_scr_act();

  // Create NEW screen
  lv_obj_t *new_scr = lv_obj_create(NULL);
  lv_obj_clear_flag(new_scr, LV_OBJ_FLAG_SCROLLABLE);

  char buf[128];

  // Base Black Background
  lv_obj_set_style_bg_color(new_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  // Dynamic Weather Glow (Applies to all modes for consistency/atmosphere)
  uint32_t glow_color = 0x111111; // Default
  int code = data.currentWeatherCode;
  if (code == 0 || code == 1)
    glow_color = 0x001F3F; // Navy (Clear)
  else if (code == 2 || code == 3)
    glow_color = 0x222222; // Grey (Cloudy)
  else if (code >= 51 && code <= 67)
    glow_color = 0x0C192C; // Steel (Rain)
  else if (code >= 95)
    glow_color = 0x1A0033; // Purple (Thunder)

  // Apply a subtle gradient to the whole screen
  lv_obj_t *bg_grad = lv_obj_create(new_scr);
  lv_obj_set_size(bg_grad, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(bg_grad, lv_color_hex(glow_color), 0);
  lv_obj_set_style_bg_grad_color(bg_grad, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_grad_dir(bg_grad, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(bg_grad, LV_OPA_COVER, 0); // ENSURE OPACITY
  lv_obj_set_style_border_width(bg_grad, 0, 0);
  lv_obj_clear_flag(bg_grad, LV_OBJ_FLAG_SCROLLABLE);

  // Make screen clickable to toggle view
  lv_obj_add_flag(bg_grad, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(bg_grad, toggle_forecast_cb, LV_EVENT_CLICKED,
                      (void *)&data);

  // --- ATTACH GESTURE HANDLER ---
  lv_obj_add_event_cb(new_scr, handleGesture, LV_EVENT_GESTURE, NULL);

  // --- MODE SPECIFIC CONTENT ---

  // Helper for Wind Direction (Low overhead lambda)
  auto getWindDir = [](int deg) -> const char * {
    const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[((deg + 22) % 360) / 45];
  };

  if (forecastMode == 0) {
    // === VIEW 0: CURRENT WEATHER MAIN SCREEN (Redesigned with Pills) ===

    // Header (City + Time)
    lv_obj_t *header_row = lv_obj_create(bg_grad);
    lv_obj_set_size(header_row, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_style_pad_all(header_row, 5, 0);

    lv_obj_t *city_lbl = lv_label_create(header_row);
    lv_label_set_text(city_lbl, data.cityName.length() > 0
                                    ? data.cityName.c_str()
                                    : "Unknown");
    lv_obj_set_style_text_color(city_lbl, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(city_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(city_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    struct tm timeinfo;
    // Use 10ms timeout to prevent blocking
    if (getLocalTime(&timeinfo, 10)) {
      char timeStr[16];
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
      lv_obj_t *time_lbl = lv_label_create(header_row);
      lv_label_set_text(time_lbl, timeStr);
      lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xAAAAAA), 0);
      lv_obj_align(time_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

      activeTimeLabel = time_lbl; // Track it
    }

    // === GLASS CARD (Main Info) ===
    lv_obj_t *glass_card = lv_obj_create(bg_grad);
    lv_obj_set_size(glass_card, 180, 150);             // Reduced Height
    lv_obj_align(glass_card, LV_ALIGN_CENTER, 0, -45); // Shifted WAY UP
    lv_obj_set_style_bg_color(glass_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(glass_card, LV_OPA_40, 0);
    lv_obj_set_style_radius(glass_card, 15, 0);
    lv_obj_set_style_border_width(glass_card, 1, 0);
    lv_obj_set_style_border_color(glass_card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(glass_card, LV_OPA_20, 0);
    lv_obj_set_flex_flow(glass_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(glass_card, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(glass_card, 5, 0);
    lv_obj_clear_flag(glass_card, LV_OBJ_FLAG_SCROLLABLE);

    // Icon (Fixed Scrollbars)
    lv_obj_t *icon_wrap = lv_obj_create(glass_card);
    lv_obj_set_size(icon_wrap, 60, 60);
    lv_obj_set_style_bg_opa(icon_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_wrap, 0, 0);
    lv_obj_clear_flag(icon_wrap, LV_OBJ_FLAG_SCROLLABLE); // FIX SCROLLBARS
    createWeatherIcon(icon_wrap, data.currentWeatherCode);
    if (lv_obj_get_child(icon_wrap, 0))
      lv_img_set_zoom(lv_obj_get_child(icon_wrap, 0), 256);

    // Temp Row (with Trend Arrows)
    // Temp Row (with Trend Arrows)
    lv_obj_t *temp_row = lv_obj_create(glass_card);
    lv_obj_set_size(temp_row, LV_PCT(100), LV_SIZE_CONTENT); // Autosize height
    lv_obj_set_style_bg_opa(temp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(temp_row, 0, 0);
    lv_obj_set_flex_flow(temp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(temp_row, 0, 0);
    lv_obj_set_style_pad_column(temp_row, 8, 0); // Reduced spacing
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE);

    // Left Arrow (Today vs Yesterday)
    lv_obj_t *arrow_l = lv_label_create(temp_row);
    float diffL = data.daily[0].maxTemp - data.yesterdayMaxTemp;
    if (diffL >= 1.0) {
      lv_label_set_text(arrow_l, LV_SYMBOL_UP);
      lv_obj_set_style_text_color(arrow_l, lv_color_hex(0xFF5555), 0); // Red
    } else if (diffL <= -1.0) {
      lv_label_set_text(arrow_l, LV_SYMBOL_DOWN);
      lv_obj_set_style_text_color(arrow_l, lv_color_hex(0x5555FF), 0); // Blue
    } else {
      lv_label_set_text(arrow_l, "-"); // Stable
      lv_obj_set_style_text_color(arrow_l, lv_color_hex(0x888888), 0);
    }

    // Temp
    lv_obj_t *temp_lbl = lv_label_create(temp_row);
    snprintf(buf, sizeof(buf), "%.1f°C", data.currentTemp);
    lv_label_set_text(temp_lbl, buf);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0xFFFFFF), 0);

    // Right Arrow (Tomorrow vs Today)
    lv_obj_t *arrow_r = lv_label_create(temp_row);
    float diffR = data.daily[1].maxTemp - data.daily[0].maxTemp;
    if (diffR >= 1.0) {
      lv_label_set_text(arrow_r, LV_SYMBOL_UP);
      lv_obj_set_style_text_color(arrow_r, lv_color_hex(0xFF5555), 0);
    } else if (diffR <= -1.0) {
      lv_label_set_text(arrow_r, LV_SYMBOL_DOWN);
      lv_obj_set_style_text_color(arrow_r, lv_color_hex(0x5555FF), 0);
    } else {
      lv_label_set_text(arrow_r, "-");
      lv_obj_set_style_text_color(arrow_r, lv_color_hex(0x888888), 0);
    }

    // High / Low Temp (NEW)
    lv_obj_t *hl_lbl = lv_label_create(glass_card);
    snprintf(buf, sizeof(buf), "H:%.0f° L:%.0f°", data.daily[0].maxTemp,
             data.daily[0].minTemp);
    lv_label_set_text(hl_lbl, buf);
    lv_obj_set_style_text_font(hl_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hl_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_top(hl_lbl, 0, 0); // Remove padding

    // Desc
    lv_obj_t *desc_lbl = lv_label_create(glass_card);
    lv_label_set_text(desc_lbl, getWeatherDesc(data.currentWeatherCode));
    lv_obj_set_style_text_color(desc_lbl, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_pad_top(desc_lbl, 2, 0); // Reduced padding

    // === DETAILS PILLS (Bottom Area) ===
    lv_obj_t *details_cont = lv_obj_create(bg_grad);
    lv_obj_set_size(details_cont, 220, 90);
    lv_obj_align(details_cont, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_opa(details_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(details_cont, 0, 0);
    lv_obj_set_flex_flow(details_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(details_cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(details_cont, 0, 0);
    lv_obj_clear_flag(details_cont, LV_OBJ_FLAG_SCROLLABLE); // Fix scrollbar

    // Pill Macro (High Contrast)
    auto add_pill = [&](const char *label, const char *value, uint32_t color) {
      lv_obj_t *pill = lv_obj_create(details_cont);
      lv_obj_set_size(pill, 105, 40);
      lv_obj_set_style_bg_color(pill, lv_color_hex(0x202020),
                                0);                // Darker Background
      lv_obj_set_style_bg_opa(pill, LV_OPA_60, 0); // Higher Opacity
      lv_obj_set_style_radius(pill, 10, 0);
      lv_obj_set_style_border_width(pill, 1, 0);
      lv_obj_set_style_border_color(pill, lv_color_hex(0x555555),
                                    0); // Subtle border
      lv_obj_set_style_border_opa(pill, LV_OPA_50, 0);
      lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_all(pill, 0, 0);
      lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t *v = lv_label_create(pill);
      lv_label_set_text(v, value);
      lv_obj_set_style_text_color(v, lv_color_hex(color), 0);
      lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);

      lv_obj_t *l = lv_label_create(pill);
      lv_label_set_text(l, label);
      lv_obj_set_style_text_color(l, lv_color_hex(0xAAAAAA), 0);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    };

    snprintf(buf, sizeof(buf), "%d%%", data.currentHumidity);
    add_pill("Humidity", buf, 0xFFFFFF);

    snprintf(buf, sizeof(buf), "%.1f km/h", data.windSpeed);
    // Wind Dir integration
    char windLabel[16];
    snprintf(windLabel, sizeof(windLabel), "Wind %s",
             getWindDir(data.windDirection));
    add_pill(windLabel, buf, 0x90EE90);

    snprintf(buf, sizeof(buf), "%.0f hPa", data.currentPressure);
    add_pill("Pressure", buf, 0xFFFFFF);

    snprintf(buf, sizeof(buf), "AQI: %d", data.currentAQI);
    add_pill("Quality", buf, data.currentAQI > 50 ? 0xFFA500 : 0x00FF00);

  } else if (forecastMode == 1 || forecastMode == 2) {
    // === VIEW 1 & 2: HOURLY / DAILY LIST ===
    bool isHourly = (forecastMode == 1);

    // Header
    lv_obj_t *header = lv_label_create(bg_grad);
    lv_label_set_text(header, isHourly ? "Hourly Forecast" : "7-Day Outlook");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);

    // List Container
    lv_obj_t *list = lv_obj_create(bg_grad);
    lv_obj_set_size(list, LV_PCT(100),
                    260); // FIX: Reduced Height (was 280) to clear header
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    // Add event to list to allow scrolling BUT capture clicks if not scrolling
    lv_obj_add_event_cb(list, toggle_forecast_cb, LV_EVENT_CLICKED,
                        (void *)&data);

    int count = isHourly ? 24 : 7;
    for (int i = 0; i < count; i++) {
      lv_obj_t *row = lv_obj_create(list);
      lv_obj_set_size(row, LV_PCT(100), 45); // Taller rows
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_bg_color(row,
                                lv_color_hex((i % 2) ? 0x222222 : 0x000000), 0);
      lv_obj_set_style_border_width(row, 0, 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      // Pass clicks through rows to the list container
      lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);

      // Time/Day
      lv_obj_t *time_lbl = lv_label_create(row);
      lv_obj_set_width(time_lbl, 60);
      if (isHourly) {
        if (data.hourly[i].time.length() > 10)
          lv_label_set_text(time_lbl,
                            data.hourly[i].time.substring(11, 16).c_str());
        else
          lv_label_set_text(time_lbl, "--:--");
      } else {
        // Just show date or day name if possible (we only have date string)
        if (data.daily[i].date.length() > 0) {
          char dateBuf[32];
          formatDate(data.daily[i].date.c_str(), dateBuf);
          lv_label_set_text(time_lbl, dateBuf);
        } else
          lv_label_set_text(time_lbl, "Day");
      }
      lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xFFFFFF), 0);

      // Icon
      lv_obj_t *icon_box = lv_obj_create(row);
      lv_obj_set_size(icon_box, 40, 40);
      lv_obj_set_style_bg_opa(icon_box, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(icon_box, 0, 0);
      lv_obj_set_style_pad_all(icon_box, 0, 0);
      lv_obj_clear_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE); // FIX SCROLLBARS
      createWeatherIcon(icon_box, isHourly ? data.hourly[i].weatherCode
                                           : data.daily[i].weatherCode);
      if (lv_obj_get_child(icon_box, 0))
        lv_img_set_zoom(lv_obj_get_child(icon_box, 0), 160);

      // Trend Indicator (Daily Only)
      if (!isHourly) {
        lv_obj_t *trend_lbl = lv_label_create(row);
        lv_obj_set_width(trend_lbl, 20); // Fixed width for alignment
        lv_obj_set_style_text_align(trend_lbl, LV_TEXT_ALIGN_CENTER, 0);

        if (i > 0) {
          float diff = data.daily[i].maxTemp - data.daily[i - 1].maxTemp;
          if (diff >= 1.0) {
            lv_label_set_text(trend_lbl, LV_SYMBOL_UP);
            lv_obj_set_style_text_color(trend_lbl, lv_color_hex(0xFF5555),
                                        0); // Red
          } else if (diff <= -1.0) {
            lv_label_set_text(trend_lbl, LV_SYMBOL_DOWN);
            lv_obj_set_style_text_color(trend_lbl, lv_color_hex(0x5555FF),
                                        0); // Blue
          } else {
            lv_label_set_text(trend_lbl, ""); // Flat
          }
        } else {
          lv_label_set_text(trend_lbl, "");
        }
      }

      // Temp
      lv_obj_t *temp_lbl = lv_label_create(row);
      if (isHourly)
        snprintf(buf, sizeof(buf), "%.1f°", data.hourly[i].temp);
      else
        snprintf(buf, sizeof(buf), "%.0f°/%.0f°", data.daily[i].minTemp,
                 data.daily[i].maxTemp);
      lv_label_set_text(temp_lbl, buf);
      lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0xFFFFFF), 0);
    }
  } else if (forecastMode == 3) {
    // === VIEW 3: TRENDS GRAPH ===

    // Add click to background for this mode specifically if needed,
    // or rely on the global bg_grad click (already set at top)

    lv_obj_t *title = lv_label_create(bg_grad);
    lv_label_set_text(title, "24h Temperature");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *chart = lv_chart_create(bg_grad);
    lv_obj_set_size(chart, LV_PCT(90), 240);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 10);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_50, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_add_event_cb(chart, toggle_forecast_cb, LV_EVENT_CLICKED,
                        (void *)&data);

    // Axis
    lv_obj_set_style_pad_left(chart, 40, 0);
    lv_obj_set_style_pad_bottom(chart, 20, 0);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 5, 2, true,
                           40);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -5, 45);

    lv_chart_series_t *ser = lv_chart_add_series(chart, lv_color_hex(0xFFA500),
                                                 LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_point_count(chart, 24);

    for (int i = 0; i < 24; i++) {
      lv_chart_set_next_value(chart, ser, (int)data.hourly[i].temp);
    }
    lv_chart_refresh(chart);
  }

  // ANIMATION & LOAD
  lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
  if (anim == 1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
  else if (anim == -1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
  else if (anim == 2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  else if (anim == -2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_TOP;

  // Always use load_anim to ensure auto_del acts on the OLD screen
  int time = (anim == 0) ? 0 : 300;
  lv_scr_load_anim(new_scr, anim_type, time, 0, true);

  // Serial.printf("DEBUG: GuiController::showWeatherScreen LOAD ANIM (Render
  // Time: %lu ms)\n", millis() - tStart);
}

const char *GuiController::getWeatherDesc(int code) {
  // Simplified WMO codes
  switch (code) {
  case 0:
    return "Clear sky";
  case 1:
    return "Mainly clear";
  case 2:
    return "Partly cloudy";
  case 3:
    return "Overcast";
  case 45:
  case 48:
    return "Fog";
  case 51:
  case 53:
  case 55:
    return "Drizzle";
  case 61:
  case 63:
  case 65:
    return "Rain";
  case 71:
  case 73:
  case 75:
    return "Snow";
  case 80:
  case 81:
  case 82:
    return "Rain Showers";
  case 95:
  case 96:
  case 99:
    return "Thunderstorm";
  default:
    return "Unknown";
  }
}

// Helper to get color for bus line
lv_color_t GuiController::getBusLineColor(const String &line,
                                          lv_color_t &textColor) {
  textColor = lv_color_hex(0xFFFFFF); // Default white text

  if (line.startsWith("H")) {
    return lv_color_hex(0x002E6E); // TMB Blue
  } else if (line.startsWith("V")) {
    return lv_color_hex(0x6FA628); // TMB Green
  } else if (line.startsWith("D")) {
    return lv_color_hex(0x8956A0); // TMB Purple
  } else if (line.length() <= 3 && line.toInt() != 0) {
    // Standard numerical lines
    return lv_color_hex(0xCC0000); // TMB Red
  }

  // Default / Night / Other
  return lv_color_hex(0xCC0000); // Fallback to Red
}

void GuiController::showBusScreen(const BusData &data, int anim) {
  uint32_t tStart = millis();
  // Serial.printf("DEBUG: GuiController::showBusScreen START (Data: %d
  // buses)\n", data.arrivals.size()); Serial.printf("DEBUG: GUI StopName:
  // '%s'\n", data.stopName.c_str());

  busScreenActive = true;

  // Update Cache
  if (&data != &cachedBus) {
    cachedBus = data;
  }

  // Create NEW screen for animation
  lv_obj_t *new_scr = lv_obj_create(NULL);
  lv_obj_clean(new_scr); // Good practice even if new

  // Attach Gesture Handler
  lv_obj_add_event_cb(new_scr, handleGesture, LV_EVENT_GESTURE, NULL);

  char buf[128]; // Reuse buffer
  lv_obj_set_style_bg_color(new_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  // 1. STATUS BAR (Consistent with Weather)
  lv_obj_t *status_bar = lv_obj_create(new_scr);
  lv_obj_set_size(status_bar, LV_PCT(100), 20);
  lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x1A1A1A), 0);
  lv_obj_set_style_border_width(status_bar, 0, 0);
  lv_obj_set_style_pad_all(status_bar, 2, 0);
  lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

  struct tm timeinfo;
  // Use 10ms timeout to prevent blocking
  if (getLocalTime(&timeinfo, 10)) {
    char timeStr[16];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lv_obj_t *time_lb = lv_label_create(status_bar);
    lv_label_set_text(time_lb, timeStr);
    lv_obj_set_style_text_color(time_lb, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(time_lb, &lv_font_montserrat_14, 0);
    lv_obj_align(time_lb, LV_ALIGN_RIGHT_MID, -5, 0);
  }

  // 2. HEADER: Bus Stop (Now below status bar)
  lv_obj_t *header = lv_obj_create(new_scr);
  lv_obj_set_size(header, LV_PCT(100), 40);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_bg_color(header, lv_color_hex(0xCC0000), 0); // TMB Red
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Bus Icon
  lv_obj_t *icon = lv_img_create(header);
  lv_img_set_src(icon, &bus_icon);
  lv_img_set_zoom(icon, 128); // Scale 64px -> 32px
  lv_obj_align(icon, LV_ALIGN_LEFT_MID, -10, 0);

  lv_obj_t *title = lv_label_create(header);
  if (data.stopName.length() > 0) {
    lv_label_set_text(title, data.stopName.c_str());
    lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(title, 180);
    lv_obj_align(title, LV_ALIGN_CENTER, 15, 0);
  } else {
    lv_label_set_text_fmt(title, "Stop: %s", data.stopCode.c_str());
    lv_obj_align(title, LV_ALIGN_CENTER, 15, 0);
  }
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

  // List of Arrivals
  lv_obj_t *list = lv_obj_create(new_scr);
  lv_obj_set_size(list, LV_PCT(100), 260); // Space below header
  lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_row(list, 0, 0);

  // Attach Gesture to List so swipes work even over the list
  // IMPORTANT: Allow bubbling so the screen-level handler (in main.cpp) sees
  // it!
  lv_obj_clear_flag(list, LV_OBJ_FLAG_GESTURE_BUBBLE); // Default is bubble? No.
  lv_obj_add_flag(list, LV_OBJ_FLAG_GESTURE_BUBBLE);

  if (data.arrivals.empty()) {
    lv_obj_t *lbl = lv_label_create(list);
    lv_label_set_text(lbl, "No buses found or API Error.");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
  } else {
    int idx = 0;
    // LIMIT TO MAX 6 ITEMS FOR PERFORMANCE
    int limit = 6;
    for (const auto &arr : data.arrivals) {
      if (idx >= limit)
        break; // Optimization limit

      lv_obj_t *row = lv_obj_create(list);
      lv_obj_set_size(row, LV_PCT(100), 44);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);

      // Zebra striping
      uint32_t bg_col = (idx % 2 == 0) ? 0x000000 : 0x333333;
      lv_obj_set_style_bg_color(row, lv_color_hex(bg_col), 0);
      lv_obj_set_style_border_width(row, 0, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_pad_all(row, 5, 0);
      lv_obj_set_style_pad_column(row, 8, 0);

      // Professional Line Badge
      lv_color_t txtCol;
      lv_color_t badgeCol = getBusLineColor(arr.line, txtCol);

      lv_obj_t *lineBox = lv_obj_create(row);
      lv_obj_set_size(lineBox, 40, 28);
      lv_obj_set_style_bg_color(lineBox, badgeCol, 0);
      lv_obj_set_style_radius(lineBox, 4, 0);
      lv_obj_set_style_border_width(lineBox, 0, 0);
      lv_obj_clear_flag(lineBox, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t *lineLbl = lv_label_create(lineBox);
      lv_label_set_text(lineLbl, arr.line.c_str());
      lv_obj_center(lineLbl);
      lv_obj_set_style_text_color(lineLbl, txtCol, 0);
      lv_obj_set_style_text_font(lineLbl, &lv_font_montserrat_14, 0);

      // Destination
      lv_obj_t *dest = lv_label_create(row);
      lv_label_set_text(dest, arr.destination.c_str());
      lv_obj_set_flex_grow(dest, 1);
      lv_label_set_long_mode(dest, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_obj_set_style_text_color(dest, lv_color_hex(0xDDDDDD), 0);
      lv_obj_set_style_text_font(dest, &lv_font_montserrat_14, 0);

      // Pro ETA Color Coding
      lv_obj_t *timeLbl = lv_label_create(row);
      lv_label_set_text(timeLbl, arr.text.c_str());
      lv_obj_set_width(timeLbl, 55);
      lv_obj_set_style_text_align(timeLbl, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_style_text_font(timeLbl, &lv_font_montserrat_14, 0);

      uint32_t eta_col = 0x00FF00; // Default Lime
      int mins = arr.seconds / 60;
      if (mins <= 2) {
        eta_col = 0xFF4500; // Arriving (Orange-Red)
        // Add Pulse Animation
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
        eta_col = 0xFFFF00; // Soon (Yellow)
      lv_obj_set_style_text_color(timeLbl, lv_color_hex(eta_col), 0);

      idx++;
    }
  }

  // ANIMATION & LOAD
  lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
  if (anim == 1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
  else if (anim == -1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
  else if (anim == 2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  else if (anim == -2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_TOP;

  if (anim == 0) {
    lv_scr_load(new_scr);
  } else {
    lv_scr_load_anim(new_scr, anim_type, 300, 0, true);
  }
  /* Serial.printf(
      "DEBUG: GuiController::showBusScreen LOAD ANIM (Render Time: %lu ms)\n",
      millis() - tStart); */
}

void GuiController::updateTime() {
  if (activeTimeLabel == NULL)
    return;

  // Verify object validity (simple check)
  if (!lv_obj_is_valid(activeTimeLabel)) {
    activeTimeLabel = NULL;
    return;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    char timeStr[16];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lv_label_set_text(activeTimeLabel, timeStr);
  }
}
