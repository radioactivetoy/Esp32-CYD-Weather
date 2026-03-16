#include "WeatherView.h"
#include "DataManager.h"
#include "GuiController.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);

// Helper for Month Names
static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void WeatherView::formatDate(const char *input, char *output) {
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

  if (code == 0) {
    src = isNight ? (const void *)&weather_icon_moon
                 : (const void *)&weather_icon_sun;
  } else if (code == 1 || code == 2) {
    src = isNight ? (const void *)&weather_icon_night_part_cloud
                 : (const void *)&weather_icon_part_cloud;
  } else if (code == 3) {
    src = isNight ? (const void *)&weather_icon_cloud_night
                 : (const void *)&weather_icon_cloud;
  } else if (code == 45 || code == 48) {
    src = isNight ? (const void *)&weather_icon_fog_night
                 : (const void *)&weather_icon_fog;
  } else if (code >= 51 && code <= 55) {
    src = &weather_icon_drizzle;
  } else if (code >= 61 && code <= 67) {
    src = isNight ? (const void *)&weather_icon_rain_night
                 : (const void *)&weather_icon_rain;
  } else if (code >= 71 && code <= 77) {
    src = isNight ? (const void *)&weather_icon_snow_night
                 : (const void *)&weather_icon_snow;
  } else if (code >= 80 && code <= 82) {
    src = &weather_icon_showers;
  } else if (code >= 85 && code <= 86) {
    src = isNight ? (const void *)&weather_icon_snow_night
                 : (const void *)&weather_icon_snow;
  } else if (code >= 95) {
    src = isNight ? (const void *)&weather_icon_thunder_night
                 : (const void *)&weather_icon_thunder;
  }

  lv_obj_t *img = lv_img_create(parent);
  lv_img_set_src(img, src);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
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
  lv_obj_add_event_cb(bg_grad, GuiController::handleLongPress,
                      LV_EVENT_LONG_PRESSED, NULL);
  lv_obj_add_event_cb(new_scr, GuiController::handleGesture, LV_EVENT_GESTURE,
                      NULL);

  auto getWindDir = [](int deg) -> const char * {
    const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[((deg + 22) % 360) / 45];
  };

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
  lv_obj_add_event_cb(header_row, GuiController::showInfoOverlay,
                      LV_EVENT_LONG_PRESSED, NULL);

  lv_obj_t *city_lbl = lv_label_create(header_row);
  lv_obj_set_width(city_lbl, 160); // Reduced to 160 as per user request
  lv_label_set_long_mode(city_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(city_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(city_lbl, &lv_font_montserrat_20, 0);
  lv_obj_align(city_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  String titleText = String(data.cityName.length() > 0
                                ? GuiController::sanitize(data.cityName).c_str()
                                : "Unknown");
  if (forecastMode == 1)
    titleText += " - Hourly";
  else if (forecastMode == 2)
    titleText += " - 7 Days";
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
  lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xDDDDDD), 0);
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

  if (forecastMode == 0) {
    // === CURRENT WEATHER ===

    // Glass Card
    lv_obj_t *glass_card = lv_obj_create(bg_grad);
    lv_obj_set_size(glass_card, 180, 175); // Expanded for feels like + sunrise/sunset
    lv_obj_align(glass_card, LV_ALIGN_TOP_MID, 0,
                 38); // Align below header (moved up 45->38)
    lv_obj_set_style_bg_color(glass_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(glass_card, LV_OPA_60, 0);
    lv_obj_set_style_radius(glass_card, 15, 0);
    lv_obj_set_style_border_width(glass_card, 2, 0); // Increased 1->2
    lv_obj_set_style_border_color(glass_card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(glass_card, LV_OPA_70, 0);
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
    lv_obj_set_size(icon_wrap, 64, 64);
    lv_obj_set_style_bg_opa(icon_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_wrap, 0, 0);
    lv_obj_clear_flag(icon_wrap,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    createWeatherIcon(icon_wrap, data.currentWeatherCode, data.isNight);
    if (lv_obj_get_child(icon_wrap, 0))
      lv_img_set_zoom(lv_obj_get_child(icon_wrap, 0),
                      256); // 1:1 native size

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
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_32, 0);
    {
      uint32_t tc;
      if (data.currentTemp < 0)        tc = 0x88AAFF;
      else if (data.currentTemp < 10)  tc = 0xAADDFF;
      else if (data.currentTemp < 20)  tc = 0xFFFFFF;
      else if (data.currentTemp < 28)  tc = 0xFFCC44;
      else                             tc = 0xFF5533;
      lv_obj_set_style_text_color(temp_lbl, lv_color_hex(tc), 0);
    }

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
      lv_label_set_text(arrow_r, "=");
      lv_obj_set_style_text_color(arrow_r, lv_color_hex(0x00CC00), 0);
    }

    // H/L/F (combined single line, F: color-coded by delta)
    lv_obj_t *hl_lbl = lv_label_create(glass_card);
    lv_label_set_recolor(hl_lbl, true);
    {
      float fd = data.currentFeelsLike - data.currentTemp;
      uint32_t fc = (fd <= -3.0f) ? 0x88AAFF : (fd >= 3.0f) ? 0xFF7744 : 0xEEEEEE;
      snprintf(buf, sizeof(buf), "H:%.0f\xC2\xB0 L:%.0f\xC2\xB0  #%06X F:%.0f\xC2\xB0#",
               data.daily[0].maxTemp, data.daily[0].minTemp,
               (unsigned int)fc, data.currentFeelsLike);
    }
    lv_label_set_text(hl_lbl, buf);
    lv_obj_set_style_text_font(hl_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hl_lbl, lv_color_hex(0xEEEEEE), 0);
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


    // Sunrise / Sunset
    if (data.sunrise.length() > 0 && data.sunset.length() > 0) {
      lv_obj_t *sun_lbl = lv_label_create(glass_card);
      char sunBuf[32];
      snprintf(sunBuf, sizeof(sunBuf), LV_SYMBOL_UP " %s  " LV_SYMBOL_DOWN " %s",
               data.sunrise.c_str(), data.sunset.c_str());
      lv_label_set_text(sun_lbl, sunBuf);
      lv_obj_set_style_text_font(sun_lbl, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(sun_lbl, lv_color_hex(0xFFDD66), 0);
      lv_obj_set_style_pad_top(sun_lbl, 2, 0);
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
      lv_obj_set_style_border_color(pill, lv_color_hex(0xAAAAAA), 0);
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
      lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_16,
                                 0); // Upgrade 14->16
    };

    snprintf(buf, sizeof(buf), "%d%%", data.currentHumidity);
    add_pill("Humidity", buf, 0xFFFFFF);

    snprintf(buf, sizeof(buf), "%.1f km/h", data.windSpeed);
    char windLabel[16];
    snprintf(windLabel, sizeof(windLabel), "Wind %s",
             getWindDir(data.windDirection));
    {
      uint32_t wc = 0x90EE90;
      if (data.windSpeed >= 20) wc = 0xFFFF00;
      if (data.windSpeed >= 40) wc = 0xFF9900;
      if (data.windSpeed >= 60) wc = 0xFF4444;
      add_pill(windLabel, buf, wc);
    }

    {
      snprintf(buf, sizeof(buf), "UV %.0f", data.currentUVIndex);
      uint32_t uvColor = 0x00FF00;   // Low (0-2)
      if (data.currentUVIndex >= 3)  uvColor = 0xFFFF00;  // Moderate
      if (data.currentUVIndex >= 6)  uvColor = 0xFF8800;  // High
      if (data.currentUVIndex >= 8)  uvColor = 0xFF4444;  // Very high
      if (data.currentUVIndex >= 11) uvColor = 0xFF00FF;  // Extreme
      add_pill("UV Index", buf, uvColor);
    }

    char aqiBuf[32];
    snprintf(aqiBuf, sizeof(aqiBuf), "AQI: %d", data.currentAQI);
    uint32_t aqiColor = 0x00FF00; // Good (1)
    if (data.currentAQI == 2)
      aqiColor = 0xADFF2F; // Fair (GreenYellow)
    else if (data.currentAQI == 3)
      aqiColor = 0xFFFF00; // Moderate (Yellow)
    else if (data.currentAQI == 4)
      aqiColor = 0xFFA500; // Poor (Orange)
    else if (data.currentAQI >= 5)
      aqiColor = 0xFF4500; // Very Poor (OrangeRed)
    add_pill("Quality", aqiBuf, aqiColor);

    // City page indicator dots
    if (GuiController::cityCount > 1) {
      lv_obj_t *dot_row = lv_obj_create(bg_grad);
      lv_obj_set_size(dot_row, LV_SIZE_CONTENT, 8);
      lv_obj_align_to(dot_row, glass_card, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
      lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(dot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_bg_opa(dot_row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(dot_row, 0, 0);
      lv_obj_set_style_pad_all(dot_row, 0, 0);
      lv_obj_set_style_pad_column(dot_row, 5, 0);
      lv_obj_clear_flag(dot_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      int cur = GuiController::getCityIndex();
      int tot = GuiController::cityCount;
      for (int i = 0; i < tot; i++) {
        lv_obj_t *d = lv_obj_create(dot_row);
        bool active = (i == cur);
        lv_obj_set_size(d, active ? 10 : 6, 6);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(active ? 0xFFFFFF : 0x666666), 0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      }
    }

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
      lv_obj_t *row = lv_obj_create(list);
      lv_obj_set_size(row, LV_PCT(100), 45);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_bg_color(row,
                                lv_color_hex((i % 2) ? 0x2A2A2A : 0x181818),
                                0);
      lv_obj_set_style_bg_opa(row, LV_OPA_80, 0); // High Opacity
      lv_obj_set_style_border_width(row, 2, 0);   // 1->2
      lv_obj_set_style_border_color(row, lv_color_hex(0xAAAAAA), 0);
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
      lv_obj_clear_flag(icon_box,
                        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      bool slotIsNight = false;
      if (isHourly && data.hourly[i].time.length() > 12) {
        int h = data.hourly[i].time.substring(11, 13).toInt();
        slotIsNight = (h >= 20 || h < 7);
      }
      createWeatherIcon(icon_box,
                        isHourly ? data.hourly[i].weatherCode
                                 : data.daily[i].weatherCode,
                        slotIsNight);
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

  int time = (anim == 0) ? 0 : 300;
  lv_scr_load_anim(new_scr, anim_type, time, 0, true);
}
