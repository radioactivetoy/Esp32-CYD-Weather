#include "WeatherView.h"
#include "DataManager.h"
#include "GuiController.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_32);

static const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed",
                                 "Thu", "Fri", "Sat"};

// Day of week for a Gregorian date, 0 = Sunday (Sakamoto's method)
static int dayOfWeek(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3)
    y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// "YYYY-MM-DD" -> "Wed 8" (output must hold at least 16 chars)
void WeatherView::formatDate(const char *input, char *output) {
  int y, m, d;
  if (sscanf(input, "%d-%d-%d", &y, &m, &d) == 3 && m >= 1 && m <= 12 &&
      d >= 1 && d <= 31) {
    snprintf(output, 16, "%s %d", weekdays[dayOfWeek(y, m, d)], d);
  } else {
    snprintf(output, 16, "%s", input);
  }
}

const char *WeatherView::getWeatherDesc(int code) {
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

void WeatherView::createWeatherIcon(lv_obj_t *parent, int code, bool isNight) {
  lv_obj_clean(parent);
  const void *src = &weather_icon_cloud;
  lv_color_t color = lv_color_hex(0xFFFFFF);

  if (code == 0) {
    if (isNight) {
      src = &weather_icon_moon;
      color = lv_color_hex(0xEEEEEE); // Moon color
    } else {
      src = &weather_icon_sun;
      color = lv_color_hex(0xFFD700);
    }
  } else if (code == 1 || code == 2) {
    if (isNight) {
      src = &weather_icon_night_part_cloud;
      color = lv_color_hex(0xDDDDDD); // Night cloud
    } else {
      src = &weather_icon_part_cloud;
      color = lv_color_hex(0xFFEEAA);
    }
  } else if (code == 3) {
    src = &weather_icon_cloud;
    color = lv_color_hex(0xEEEEEE);
  } else if (code == 45 || code == 48) {
    src = &weather_icon_fog;
    color = lv_color_hex(0xAAAAAA);
  } else if (code >= 51 && code <= 55) {
    src = &weather_icon_drizzle;
    color = lv_color_hex(0xADD8E6);
  } else if (code >= 61 && code <= 67) {
    src = &weather_icon_rain;
    color = lv_color_hex(0x00BFFF);
  } else if (code >= 71 && code <= 77) {
    src = &weather_icon_snow;
    color = lv_color_hex(0xE0FFFF);
  } else if (code >= 80 && code <= 82) {
    src = &weather_icon_showers;
    color = lv_color_hex(0x1E90FF);
  } else if (code >= 85 && code <= 86) {
    src = &weather_icon_snow;
    color = lv_color_hex(0xE0FFFF);
  } else if (code >= 95) {
    src = &weather_icon_thunder;
    color = lv_color_hex(0x9370DB);
  }

  lv_obj_t *img = lv_img_create(parent);
  lv_img_set_src(img, src);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(img, color, 0);
}

void WeatherView::show(const WeatherData &data, int anim, int forecastMode) {
  GuiController::currentApp = GuiController::APP_WEATHER;

  // Note: We create a NEW screen, so auto_del of the previous one is handled by
  // LVGL if anim is used correctly or we rely on the caller to manage
  // transitions. GuiController logic used lv_scr_load_anim(..., auto_del=true).

  lv_obj_t *new_scr = lv_obj_create(NULL);
  if (!new_scr) {
    Serial.println("WeatherView: Screen create failed (out of mem)");
    return;
  }
  lv_obj_clear_flag(new_scr, LV_OBJ_FLAG_SCROLLABLE);

  char buf[128];

  // Base Background
  lv_obj_set_style_bg_color(new_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  // Dynamic Glow
  uint32_t glow_color = 0x111111;
  int code = data.currentWeatherCode;
  if (code == 0 || code == 1)
    glow_color = 0x001F3F;
  else if (code == 2 || code == 3)
    glow_color = 0x222222;
  else if (code >= 51 && code <= 67)
    glow_color = 0x0C192C;
  else if (code >= 95)
    glow_color = 0x1A0033;

  lv_obj_t *bg_grad = lv_obj_create(new_scr);
  lv_obj_set_size(bg_grad, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(bg_grad, lv_color_hex(glow_color), 0);
  lv_obj_set_style_bg_grad_color(bg_grad, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_grad_dir(bg_grad, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(bg_grad, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bg_grad, 0, 0);
  lv_obj_set_style_pad_all(bg_grad, 0, 0); // Fix: Remove default padding
  lv_obj_clear_flag(bg_grad, LV_OBJ_FLAG_SCROLLABLE);

  // Click & Gesture Handlers
  lv_obj_add_flag(bg_grad, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(bg_grad, GuiController::handleScreenClick,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(new_scr, GuiController::handleGesture, LV_EVENT_GESTURE,
                      NULL);

  auto getWindDir = [](int deg) -> const char * {
    static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int d = ((deg % 360) + 360) % 360; // Normalize, also for negative input
    return dirs[((d + 22) / 45) % 8];
  };

  // lastUpdate == 0: no successful fetch for this city yet (placeholder)
  bool noData = (data.lastUpdate == 0);

  // === COMMON HEADER ===
  lv_obj_t *header_row = lv_obj_create(bg_grad);
  lv_obj_set_size(header_row, LV_PCT(100), 40);
  lv_obj_align(header_row, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(header_row, 0, 0);
  lv_obj_set_style_pad_all(header_row, 5, 0);
  lv_obj_clear_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(header_row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE |
                                  LV_OBJ_FLAG_GESTURE_BUBBLE);

  lv_obj_t *city_lbl = lv_label_create(header_row);
  lv_obj_set_width(city_lbl, 160); // Reduced to 160 as per user request
  lv_label_set_long_mode(city_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(city_lbl, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_text_font(city_lbl, &Fonts::text20, 0); // Accents: "Vallès"
  lv_obj_align(city_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  // Which city of several (swipe left/right)
  GuiController::createPageDots(header_row, GuiController::cityCount,
                                GuiController::getCityIndex());

  String titleText = String(data.cityName.length() > 0
                                ? GuiController::sanitize(data.cityName).c_str()
                                : "Unknown");
  if (forecastMode == 1)
    titleText += " - Hourly";
  else if (forecastMode == 2)
    titleText += " - Daily";
  lv_label_set_text(city_lbl, titleText.c_str());

  struct tm timeinfo;
  lv_obj_t *time_lbl = lv_label_create(header_row);
  if (getLocalTime(&timeinfo, 10)) {
    char timeStr[16];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lv_label_set_text(time_lbl, timeStr);
  } else {
    lv_label_set_text(time_lbl, "--:--");
  }
  lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_20, 0);
  lv_obj_align(time_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);
  GuiController::setActiveTimeLabel(time_lbl);

  // Status Dot
  lv_obj_t *dot = lv_obj_create(header_row);
  lv_obj_set_size(dot, 10, 8); // Wider
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_align_to(dot, time_lbl, LV_ALIGN_OUT_LEFT_MID, -7, 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

  uint32_t dotColor = 0x00AA00; // Dark Green (Fresh)
  if (DataManager::isWeatherUpdating(GuiController::getCityIndex())) {
    dotColor = 0xFFFF00; // Yellow (Refreshing)
  } else if (data.lastUpdate == 0 ||
             (millis() - data.lastUpdate > 900000)) {
    dotColor = 0xFF0000; // Red (Stale)
  }
  lv_obj_set_style_bg_color(dot, lv_color_hex(dotColor), 0);

  if (noData) {
    // === NO DATA YET === (keeps gestures working while we wait)
    lv_obj_t *wait_lbl = lv_label_create(bg_grad);
    lv_label_set_text(wait_lbl, DataManager::isWeatherUpdating(
                                    GuiController::getCityIndex())
                                    ? "Fetching weather..."
                                    : "No weather data yet.\nRetrying soon.");
    lv_obj_set_style_text_color(wait_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(wait_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(wait_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wait_lbl, LV_ALIGN_CENTER, 0, 0);
  } else if (forecastMode == 0) {
    // === CURRENT WEATHER ===

    // Glass Card
    lv_obj_t *glass_card = lv_obj_create(bg_grad);
    lv_obj_set_size(glass_card, 180, 172); // 155 + "Feels like" line
    lv_obj_align(glass_card, LV_ALIGN_TOP_MID, 0,
                 38); // Align below header (moved up 45->38)
    lv_obj_set_style_bg_color(glass_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(glass_card, LV_OPA_70, 0);
    lv_obj_set_style_radius(glass_card, 15, 0);
    lv_obj_set_style_border_width(glass_card, 2, 0); // Increased 1->2
    lv_obj_set_style_border_color(glass_card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(glass_card, LV_OPA_50, 0);
    lv_obj_set_flex_flow(glass_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(glass_card, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(glass_card, 5, 0);
    lv_obj_set_style_pad_row(glass_card, 2,
                             0); // Minimize internal vertical gap
    lv_obj_clear_flag(glass_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(glass_card, LV_OBJ_FLAG_CLICKABLE |
                                    LV_OBJ_FLAG_EVENT_BUBBLE |
                                    LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *icon_wrap = lv_obj_create(glass_card);
    lv_obj_set_size(icon_wrap, 50, 50); // Reduced 60->50 to save vertical space
    lv_obj_set_style_bg_opa(icon_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_wrap, 0, 0);
    lv_obj_clear_flag(icon_wrap,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    createWeatherIcon(icon_wrap, data.currentWeatherCode, data.isNight);
    if (lv_obj_get_child(icon_wrap, 0))
      lv_img_set_zoom(lv_obj_get_child(icon_wrap, 0),
                      220); // Zoom 256->220 (approx 0.85x)

    // Temp Row
    lv_obj_t *temp_row = lv_obj_create(glass_card);
    lv_obj_set_size(temp_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(temp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(temp_row, 0, 0);
    lv_obj_set_flex_flow(temp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(temp_row, 0, 0);
    lv_obj_set_style_pad_column(temp_row, 8, 0);
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Temp
    lv_obj_t *temp_lbl = lv_label_create(temp_row);
    snprintf(buf, sizeof(buf), "%.1f°C", data.currentTemp);
    lv_label_set_text(temp_lbl, buf);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_32,
                               0); // Upgrade 24->32
    lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0xFFFFFF), 0);

    // Right Arrow - Floating to keep Temp centered
    lv_obj_t *arrow_r = lv_label_create(temp_row);
    lv_obj_add_flag(arrow_r, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(arrow_r, LV_ALIGN_RIGHT_MID, -10,
                 0); // Add 10px padding from right edge
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

    // Feels like
    lv_obj_t *feels_lbl = lv_label_create(glass_card);
    snprintf(buf, sizeof(buf), "Feels like %.0f°", data.currentFeelsLike);
    lv_label_set_text(feels_lbl, buf);
    lv_obj_set_style_text_font(feels_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(feels_lbl, lv_color_hex(0x999999), 0);

    // H/L
    lv_obj_t *hl_lbl = lv_label_create(glass_card);
    snprintf(buf, sizeof(buf), "H:%.0f° L:%.0f°", data.daily[0].maxTemp,
             data.daily[0].minTemp);
    lv_label_set_text(hl_lbl, buf); // Restored!
    lv_obj_set_style_text_font(hl_lbl, &lv_font_montserrat_16,
                               0); // Upgrade H/L 14->16
    lv_obj_set_style_text_color(hl_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_top(hl_lbl, 0, 0);

    // Desc
    // Desc Container (Desc + Rain%)
    lv_obj_t *desc_row = lv_obj_create(glass_card);
    lv_obj_set_size(desc_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(desc_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(desc_row, 0, 0);
    lv_obj_set_flex_flow(desc_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(desc_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(desc_row, 0, 0);
    lv_obj_set_style_pad_top(desc_row, 2, 0);
    lv_obj_clear_flag(desc_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Weather Description
    lv_obj_t *desc_lbl = lv_label_create(desc_row);
    lv_label_set_text(desc_lbl, getWeatherDesc(data.currentWeatherCode));
    lv_obj_set_style_text_color(desc_lbl, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(desc_lbl, &lv_font_montserrat_16, 0);

    // Rain % (Appended)
    if (data.currentRainProb > 0.0) {
      lv_obj_t *rain_appended = lv_label_create(desc_row);
      char rBuf[16];
      snprintf(rBuf, sizeof(rBuf), " %.0f%%",
               data.currentRainProb * 100.0); // Space prefix
      lv_label_set_text(rain_appended, rBuf);
      lv_obj_set_style_text_color(rain_appended, lv_color_hex(0x00BFFF),
                                  0); // Blue
      lv_obj_set_style_text_font(rain_appended, &lv_font_montserrat_16, 0);
    }

    // Pills
    lv_obj_t *details_cont = lv_obj_create(bg_grad);
    lv_obj_set_size(details_cont, 220, 90);
    lv_obj_align(details_cont, LV_ALIGN_BOTTOM_MID, 0,
                 -2); // Moved lower -15 -> -2
    lv_obj_set_style_bg_opa(details_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(details_cont, 0, 0);
    lv_obj_set_flex_flow(details_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(details_cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(details_cont, 0, 0);
    lv_obj_clear_flag(details_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(details_cont, LV_OBJ_FLAG_CLICKABLE |
                                      LV_OBJ_FLAG_EVENT_BUBBLE |
                                      LV_OBJ_FLAG_GESTURE_BUBBLE);

    auto add_pill = [&](const char *label, const char *value, uint32_t color) {
      lv_obj_t *pill = lv_obj_create(details_cont);
      lv_obj_set_size(pill, 105, 40);
      lv_obj_set_style_bg_color(pill, lv_color_hex(0x202020), 0);
      lv_obj_set_style_bg_opa(pill, LV_OPA_80, 0);
      lv_obj_set_style_radius(pill, 10, 0);
      lv_obj_set_style_border_width(pill, 2, 0); // Increased 1->2
      lv_obj_set_style_border_color(pill, lv_color_hex(0x777777),
                                    0); // Lighter 55->77
      lv_obj_set_style_border_opa(pill, LV_OPA_70, 0);
      lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_all(pill, 0, 0);
      lv_obj_set_style_pad_row(pill, 0, 0); // Added: Remove gap between lines
      lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

      lv_obj_t *v = lv_label_create(pill);
      lv_label_set_text(v, value);
      lv_obj_set_style_text_color(v, lv_color_hex(color), 0);
      lv_obj_set_style_text_font(v, &lv_font_montserrat_16,
                                 0); // Upgrade 14->16

      lv_obj_t *l = lv_label_create(pill);
      lv_label_set_text(l, label);
      lv_obj_set_style_text_color(l, lv_color_hex(0xDDDDDD),
                                  0); // Brighter Grey
      lv_obj_set_style_text_font(l, &lv_font_montserrat_16,
                                 0); // Upgrade 14->16
    };

    snprintf(buf, sizeof(buf), "%d%%", data.currentHumidity);
    add_pill("Humidity", buf, 0xFFFFFF);

    snprintf(buf, sizeof(buf), "%.1f km/h", data.windSpeed);
    char windLabel[16];
    snprintf(windLabel, sizeof(windLabel), "Wind %s",
             getWindDir(data.windDirection));
    add_pill(windLabel, buf, 0x90EE90);

    snprintf(buf, sizeof(buf), "%.0f hPa", data.currentPressure);
    add_pill("Pressure", buf, 0xFFFFFF);

    // OWM scale 1..5 shown as words ("Good", "Fair", ...); "--" if unknown
    const char *aqiText = "--"; // No OWM key or fetch failed
    uint32_t aqiColor = 0x888888;
    if (data.currentAQI >= 1) {
      aqiText = WeatherService::getAQIDesc(data.currentAQI);
      aqiColor = 0x00FF00; // Good (1)
    }
    if (data.currentAQI == 2)
      aqiColor = 0xADFF2F; // Fair (GreenYellow)
    else if (data.currentAQI == 3)
      aqiColor = 0xFFFF00; // Moderate (Yellow)
    else if (data.currentAQI == 4)
      aqiColor = 0xFFA500; // Poor (Orange)
    else if (data.currentAQI >= 5)
      aqiColor = 0xFF4500; // Very Poor (OrangeRed)
    add_pill("Air Quality", aqiText, aqiColor);

  } else if (forecastMode == 1 || forecastMode == 2) {
    // === LIST VIEWS ===
    bool isHourly = (forecastMode == 1);

    lv_obj_t *list = lv_obj_create(bg_grad);
    lv_obj_set_size(list, LV_PCT(100), 260);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_EVENT_BUBBLE);

    int count = isHourly ? 24 : 7;
    for (int i = 0; i < count; i++) {
      // Providers fill fewer slots than we have room for (OWM: ~6 days)
      if (isHourly ? data.hourly[i].time.isEmpty()
                   : data.daily[i].date.isEmpty())
        break;

      lv_obj_t *row = lv_obj_create(list);
      lv_obj_set_size(row, LV_PCT(100), 45);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_bg_color(row,
                                lv_color_hex((i % 2) ? 0x202020 : 0x101010),
                                0);               // Darker alternating
      lv_obj_set_style_bg_opa(row, LV_OPA_80, 0); // High Opacity
      lv_obj_set_style_border_width(row, 2, 0);   // 1->2
      lv_obj_set_style_border_color(row, lv_color_hex(0x777777), 0);
      lv_obj_set_style_border_opa(row, LV_OPA_70, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

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
        char dateBuf[16];
        if (i == 0)
          snprintf(dateBuf, sizeof(dateBuf), "Today");
        else
          formatDate(data.daily[i].date.c_str(), dateBuf); // "Wed 8"
        lv_label_set_text(time_lbl, dateBuf);
      }
      lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xFFFFFF), 0);

      // Icon
      lv_obj_t *icon_box = lv_obj_create(row);
      lv_obj_set_size(icon_box, 40, 40);
      lv_obj_set_style_bg_opa(icon_box, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(icon_box, 0, 0);
      lv_obj_set_style_pad_all(icon_box, 0, 0);
      lv_obj_clear_flag(icon_box,
                        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      createWeatherIcon(icon_box,
                        isHourly ? data.hourly[i].weatherCode
                                 : data.daily[i].weatherCode,
                        isHourly && data.hourly[i].isNight); // Daily: day icon
      if (lv_obj_get_child(icon_box, 0))
        lv_img_set_zoom(lv_obj_get_child(icon_box, 0), 160);

      // Rain Prob (List)
      float pop = isHourly ? data.hourly[i].pop : data.daily[i].pop;
      lv_obj_t *rain_lbl = lv_label_create(row);
      lv_obj_set_width(rain_lbl, 40);
      lv_obj_set_style_text_align(rain_lbl, LV_TEXT_ALIGN_CENTER, 0);

      if (pop >= 0.1) { // Show if > 10%
        char rainBuf[16];
        snprintf(rainBuf, sizeof(rainBuf), "%.0f%%", pop * 100.0);
        lv_label_set_text(rain_lbl, rainBuf);
        lv_obj_set_style_text_color(rain_lbl, lv_color_hex(0x00BFFF), 0);
        lv_obj_set_style_text_font(rain_lbl, &lv_font_montserrat_14,
                                   0); // Small font
      } else {
        lv_label_set_text(rain_lbl, "");
      }

      // Trend
      if (!isHourly) {
        lv_obj_t *trend_lbl = lv_label_create(row);
        lv_obj_set_width(trend_lbl, 20);
        lv_obj_set_style_text_align(trend_lbl, LV_TEXT_ALIGN_CENTER, 0);
        if (i > 0) {
          float diff = data.daily[i].maxTemp - data.daily[i - 1].maxTemp;
          if (diff >= 1.0) {
            lv_label_set_text(trend_lbl, LV_SYMBOL_UP);
            lv_obj_set_style_text_color(trend_lbl, lv_color_hex(0xFF5555), 0);
          } else if (diff <= -1.0) {
            lv_label_set_text(trend_lbl, LV_SYMBOL_DOWN);
            lv_obj_set_style_text_color(trend_lbl, lv_color_hex(0x5555FF), 0);
          } else {
            lv_label_set_text(trend_lbl, "");
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
  }

  // Animation
  lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
  if (anim == 1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
  else if (anim == -1)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
  else if (anim == 2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  else if (anim == -2)
    anim_type = LV_SCR_LOAD_ANIM_MOVE_TOP;
  else if (anim == 3)
    anim_type = LV_SCR_LOAD_ANIM_FADE_ON;

  int time = (anim == 0) ? 0 : 300;
  lv_scr_load_anim(new_scr, anim_type, time, 0, true);
}
