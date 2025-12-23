#include "WeatherView.h"
#include "GuiController.h"
#include <cstdio>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_24);

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

void WeatherView::createWeatherIcon(lv_obj_t *parent, int code) {
  lv_obj_clean(parent);
  const void *src = &weather_icon_cloud;
  lv_color_t color = lv_color_hex(0xFFFFFF);

  if (code == 0) {
    src = &weather_icon_sun;
    color = lv_color_hex(0xFFD700);
  } else if (code == 1 || code == 2) {
    src = &weather_icon_part_cloud;
    color = lv_color_hex(0xFFEEAA);
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
  lv_obj_clear_flag(bg_grad, LV_OBJ_FLAG_SCROLLABLE);

  // Click & Gesture Handlers
  lv_obj_add_flag(bg_grad, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(bg_grad, GuiController::handleScreenClick,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(new_scr, GuiController::handleGesture, LV_EVENT_GESTURE,
                      NULL);

  auto getWindDir = [](int deg) -> const char * {
    const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[((deg + 22) % 360) / 45];
  };

  if (forecastMode == 0) {
    // === CURRENT WEATHER ===
    lv_obj_t *header_row = lv_obj_create(bg_grad);
    lv_obj_set_size(header_row, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_style_pad_all(header_row, 5, 0);

    lv_obj_t *city_lbl = lv_label_create(header_row);
    lv_label_set_text(city_lbl,
                      data.cityName.length() > 0
                          ? GuiController::sanitize(data.cityName).c_str()
                          : "Unknown");
    lv_obj_set_style_text_color(city_lbl, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(city_lbl, &GuiController::safe_font_16, 0);
    lv_obj_align(city_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      char timeStr[16];
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
      lv_obj_t *time_lbl = lv_label_create(header_row);
      lv_label_set_text(time_lbl, timeStr);
      lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xAAAAAA), 0);
      lv_obj_align(time_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);
      GuiController::setActiveTimeLabel(time_lbl); // Register
    }

    // Glass Card
    lv_obj_t *glass_card = lv_obj_create(bg_grad);
    lv_obj_set_size(glass_card, 180, 150);
    lv_obj_align(glass_card, LV_ALIGN_CENTER, 0, -45);
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
    lv_obj_clear_flag(glass_card,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon_wrap = lv_obj_create(glass_card);
    lv_obj_set_size(icon_wrap, 60, 60);
    lv_obj_set_style_bg_opa(icon_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_wrap, 0, 0);
    lv_obj_clear_flag(icon_wrap, LV_OBJ_FLAG_SCROLLABLE);
    createWeatherIcon(icon_wrap, data.currentWeatherCode);
    if (lv_obj_get_child(icon_wrap, 0))
      lv_img_set_zoom(lv_obj_get_child(icon_wrap, 0), 256);

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
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE);

    // Left Arrow (Trend vs Yesterday)
    lv_obj_t *arrow_l = lv_label_create(temp_row);
    float diffL = data.daily[0].maxTemp - data.yesterdayMaxTemp;
    Serial.printf("TREND DEBUG: Yest: %.1f, Today: %.1f, Diff: %.1f\n",
                  data.yesterdayMaxTemp, data.daily[0].maxTemp, diffL);

    if (diffL >= 0.5) { // Threshold 0.5
      lv_label_set_text(arrow_l, LV_SYMBOL_UP);
      lv_obj_set_style_text_color(arrow_l, lv_color_hex(0xFF5555), 0);
    } else if (diffL <= -0.5) {
      lv_label_set_text(arrow_l, LV_SYMBOL_DOWN);
      lv_obj_set_style_text_color(arrow_l, lv_color_hex(0x5555FF), 0);
    } else {
      lv_label_set_text(arrow_l, "-"); // Stable
      lv_obj_set_style_text_color(arrow_l, lv_color_hex(0x888888), 0);
    }

    // Temp
    lv_obj_t *temp_lbl = lv_label_create(temp_row);
    snprintf(buf, sizeof(buf), "%.1f°C", data.currentTemp);
    lv_label_set_text(temp_lbl, buf);
    lv_obj_set_style_text_font(temp_lbl, &GuiController::safe_font_24, 0);
    lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0xFFFFFF), 0);

    // Right Arrow
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

    // H/L
    lv_obj_t *hl_lbl = lv_label_create(glass_card);
    snprintf(buf, sizeof(buf), "H:%.0f° L:%.0f°", data.daily[0].maxTemp,
             data.daily[0].minTemp);
    lv_label_set_text(hl_lbl, buf);
    lv_obj_set_style_text_font(hl_lbl, &GuiController::safe_font_14, 0);
    lv_obj_set_style_text_color(hl_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_top(hl_lbl, 0, 0);

    // Desc
    lv_obj_t *desc_lbl = lv_label_create(glass_card);
    lv_label_set_text(desc_lbl, getWeatherDesc(data.currentWeatherCode));
    lv_obj_set_style_text_color(desc_lbl, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_pad_top(desc_lbl, 2, 0);

    // Pills
    lv_obj_t *details_cont = lv_obj_create(bg_grad);
    lv_obj_set_size(details_cont, 220, 90);
    lv_obj_align(details_cont, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_opa(details_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(details_cont, 0, 0);
    lv_obj_set_flex_flow(details_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(details_cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(details_cont, 0, 0);
    lv_obj_clear_flag(details_cont,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    auto add_pill = [&](const char *label, const char *value, uint32_t color) {
      lv_obj_t *pill = lv_obj_create(details_cont);
      lv_obj_set_size(pill, 105, 40);
      lv_obj_set_style_bg_color(pill, lv_color_hex(0x202020), 0);
      lv_obj_set_style_bg_opa(pill, LV_OPA_60, 0);
      lv_obj_set_style_radius(pill, 10, 0);
      lv_obj_set_style_border_width(pill, 1, 0);
      lv_obj_set_style_border_color(pill, lv_color_hex(0x555555), 0);
      lv_obj_set_style_border_opa(pill, LV_OPA_50, 0);
      lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_all(pill, 0, 0);
      lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t *v = lv_label_create(pill);
      lv_label_set_text(v, value);
      lv_obj_set_style_text_color(v, lv_color_hex(color), 0);
      lv_obj_set_style_text_font(v, &GuiController::safe_font_14, 0);

      lv_obj_t *l = lv_label_create(pill);
      lv_label_set_text(l, label);
      lv_obj_set_style_text_color(l, lv_color_hex(0xAAAAAA), 0);
      lv_obj_set_style_text_font(l, &GuiController::safe_font_14, 0);
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

    snprintf(buf, sizeof(buf), "AQI: %d", data.currentAQI);
    add_pill("Quality", buf, data.currentAQI > 50 ? 0xFFA500 : 0x00FF00);

  } else if (forecastMode == 1 || forecastMode == 2) {
    // === LIST VIEWS ===
    bool isHourly = (forecastMode == 1);

    lv_obj_t *header = lv_label_create(bg_grad);
    String headerText = GuiController::sanitize(data.cityName) +
                        (isHourly ? " - Hourly" : " - 7 Days");
    lv_label_set_text(header, headerText.c_str());
    lv_obj_set_style_text_font(header, &GuiController::safe_font_16, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);

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
                                lv_color_hex((i % 2) ? 0x222222 : 0x000000), 0);
      lv_obj_set_style_border_width(row, 0, 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
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
      createWeatherIcon(icon_box, isHourly ? data.hourly[i].weatherCode
                                           : data.daily[i].weatherCode);
      if (lv_obj_get_child(icon_box, 0))
        lv_img_set_zoom(lv_obj_get_child(icon_box, 0), 160);

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
  } else if (forecastMode == 3) {
    // === CHART VIEW ===
    lv_obj_t *title = lv_label_create(bg_grad);
    String titleText = GuiController::sanitize(data.cityName) + " - 24h Temp";
    lv_label_set_text(title, titleText.c_str());
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *chart = lv_chart_create(bg_grad);
    lv_obj_set_size(chart, LV_PCT(90), 240);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 10);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_50, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_add_flag(chart, LV_OBJ_FLAG_EVENT_BUBBLE);

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
