#include "WeatherView.h"
#include "DataManager.h"
#include "GuiController.h"
#include "Theme.h"
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

lv_obj_t *WeatherView::createWeatherIcon(lv_obj_t *parent, int code,
                                         bool isNight) {
  // Full-colour Meteocons (alpha images), so no recolouring. Drizzle and
  // showers have no night variant.
  const void *src = &weather_icon_cloud; // Unknown codes (-1)
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
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    src = isNight ? (const void *)&weather_icon_snow_night
                  : (const void *)&weather_icon_snow;
  } else if (code >= 80 && code <= 82) {
    src = &weather_icon_showers;
  } else if (code >= 95) {
    src = isNight ? (const void *)&weather_icon_thunder_night
                  : (const void *)&weather_icon_thunder;
  }

  lv_obj_t *img = lv_img_create(parent);
  lv_img_set_src(img, src);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
  return img;
}

// --- Shared styles for the hourly / daily list rows ---
// The hourly list has 12 rows of 5-6 objects. Setting each property on each
// object allocates a private style per object (about 1.5KB per row in
// practice); these styles are created once and only referenced by the rows.
static lv_style_t styleRow;      // Border + background opacity
static lv_style_t styleRowEven;  // Background colour, alternating
static lv_style_t styleRowOdd;
static lv_style_t styleIconBox;  // 40x40 holder for the zoomed icon
static lv_style_t styleTimeCol;  // White, fixed width
static lv_style_t styleRainCol;  // Blue, small, centred, fixed width
static lv_style_t styleTrendCol; // Centred, fixed width
static lv_style_t styleTrendUp;
static lv_style_t styleTrendDown;
static lv_style_t styleTempCol; // White

static void initListStyles() {
  static bool done = false;
  if (done)
    return;
  done = true;

  lv_style_init(&styleRow);
  lv_style_set_bg_opa(&styleRow, LV_OPA_80);
  lv_style_set_border_width(&styleRow, 2);
  lv_style_set_border_color(&styleRow, lv_color_hex(0xAAAAAA));
  lv_style_set_border_opa(&styleRow, LV_OPA_70);

  lv_style_init(&styleRowEven);
  lv_style_set_bg_color(&styleRowEven, lv_color_hex(0x181818));
  lv_style_init(&styleRowOdd);
  lv_style_set_bg_color(&styleRowOdd, lv_color_hex(0x2A2A2A));

  lv_style_init(&styleIconBox);
  lv_style_set_width(&styleIconBox, 40);
  lv_style_set_height(&styleIconBox, 40);

  lv_style_init(&styleTimeCol);
  lv_style_set_width(&styleTimeCol, 60);
  lv_style_set_text_color(&styleTimeCol, lv_color_hex(0xFFFFFF));

  lv_style_init(&styleRainCol);
  lv_style_set_width(&styleRainCol, 40);
  lv_style_set_text_align(&styleRainCol, LV_TEXT_ALIGN_CENTER);
  lv_style_set_text_color(&styleRainCol, lv_color_hex(0x00BFFF));
  lv_style_set_text_font(&styleRainCol, &lv_font_montserrat_14);

  lv_style_init(&styleTrendCol);
  lv_style_set_width(&styleTrendCol, 20);
  lv_style_set_text_align(&styleTrendCol, LV_TEXT_ALIGN_CENTER);
  lv_style_init(&styleTrendUp);
  lv_style_set_text_color(&styleTrendUp, lv_color_hex(0xFF5555));
  lv_style_init(&styleTrendDown);
  lv_style_set_text_color(&styleTrendDown, lv_color_hex(0x5555FF));

  lv_style_init(&styleTempCol);
  lv_style_set_text_color(&styleTempCol, lv_color_hex(0xFFFFFF));
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

  // Flat theme background (the screen and the full-size body container,
  // which carries the tap / long-press handlers)
  lv_obj_set_style_bg_color(new_scr, lv_color_hex(Theme::BG), 0);
  lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);

  lv_obj_t *bg_grad = lv_obj_create(new_scr);
  lv_obj_set_size(bg_grad, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(bg_grad, lv_color_hex(Theme::BG), 0);
  lv_obj_set_style_bg_opa(bg_grad, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bg_grad, 0, 0);
  lv_obj_set_style_pad_all(bg_grad, 0, 0); // Fix: Remove default padding
  lv_obj_clear_flag(bg_grad, LV_OBJ_FLAG_SCROLLABLE);

  // Click & Gesture Handlers
  lv_obj_add_flag(bg_grad, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(bg_grad, GuiController::handleScreenClick,
                      LV_EVENT_SHORT_CLICKED, NULL); // Not after a long press
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
  // Long press: body = refresh now, header = device info
  GuiController::attachLongPress(bg_grad, header_row);

  lv_obj_t *city_lbl = lv_label_create(header_row);
  lv_obj_set_width(city_lbl, 160); // Reduced to 160 as per user request
  lv_label_set_long_mode(city_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(city_lbl, lv_color_hex(0xFFFFFF), 0);
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

  // Green fresh / yellow updating / red stale; recoloured in place later
  lv_obj_set_style_bg_color(dot, lv_color_hex(GuiController::statusDotColor()),
                            0);
  GuiController::setStatusDot(dot);

  if (noData) {
    // === NO DATA YET === (keeps gestures working while we wait)
    lv_obj_t *wait_lbl = lv_label_create(bg_grad);
    lv_label_set_text(wait_lbl, DataManager::isWeatherUpdating(
                                    GuiController::getCityIndex())
                                    ? "Fetching weather..."
                                    : "No weather data yet.\nRetrying soon.");
    lv_obj_set_style_text_color(wait_lbl, lv_color_hex(Theme::TEXT_DIM), 0);
    lv_obj_set_style_text_font(wait_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(wait_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wait_lbl, LV_ALIGN_CENTER, 0, 0);
  } else if (forecastMode == 0) {
    // === CURRENT WEATHER: hero (icon + big temperature) over a details grid

    // Hero icon at native 64px
    lv_obj_t *icon_wrap = lv_obj_create(bg_grad);
    lv_obj_remove_style_all(icon_wrap);
    lv_obj_set_size(icon_wrap, 72, 72);
    lv_obj_set_pos(icon_wrap, 14, 50);
    lv_obj_clear_flag(icon_wrap, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    createWeatherIcon(icon_wrap, data.currentWeatherCode, data.isNight);

    // Big temperature in whole degrees (the decimal is false precision)
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", (int)lroundf(data.currentTemp));
    lv_obj_t *temp_lbl = Theme::label(bg_grad, buf, &Theme::digits,
                                      Theme::tempColor(data.currentTemp));
    lv_obj_set_pos(temp_lbl, 100, 40);

    // Tomorrow warmer / cooler (by >= 1 degree); nothing when about the same
    float diff = data.daily[1].maxTemp - data.daily[0].maxTemp;
    if (data.daily[1].date.length() > 0 && fabsf(diff) >= 1.0f) {
      lv_obj_t *arrow =
          Theme::label(bg_grad, diff > 0 ? LV_SYMBOL_UP : LV_SYMBOL_DOWN,
                       &lv_font_montserrat_14, diff > 0 ? 0xFF7755 : 0x6688FF);
      lv_obj_align_to(arrow, temp_lbl, LV_ALIGN_OUT_RIGHT_TOP, 4, 10);
    }

    lv_obj_t *desc = Theme::label(bg_grad, getWeatherDesc(data.currentWeatherCode),
                                  &Theme::body, Theme::TEXT);
    lv_obj_set_width(desc, 132);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(desc, 102, 98);

    snprintf(buf, sizeof(buf), "H %.0f\xC2\xB0  \xC2\xB7  L %.0f\xC2\xB0",
             data.daily[0].maxTemp, data.daily[0].minTemp);
    lv_obj_t *hl = Theme::label(bg_grad, buf, &Fonts::text14, Theme::TEXT_DIM);
    lv_obj_set_pos(hl, 102, 120);

    Theme::divider(bg_grad, 10, 148, 220);

    // Details grid: 2 columns x 4 rows of label / value
    const lv_coord_t colA = 14, colB = 124;
    auto rowY = [](int r) -> lv_coord_t { return 156 + r * 40; };

    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", data.currentFeelsLike);
    Theme::cell(bg_grad, colA, rowY(0), "Feels like", buf);

    snprintf(buf, sizeof(buf), "%.0f%%", data.currentRainProb * 100.0f);
    Theme::cell(bg_grad, colB, rowY(0), "Rain", buf, Theme::RAIN);

    snprintf(buf, sizeof(buf), "%d%%", data.currentHumidity);
    Theme::cell(bg_grad, colA, rowY(1), "Humidity", buf);

    // Wind: coloured only when it matters
    snprintf(buf, sizeof(buf), "%.0f km/h %s", data.windSpeed,
             getWindDir(data.windDirection));
    uint32_t windColor = Theme::TEXT;
    if (data.windSpeed >= 60)
      windColor = Theme::ALERT;
    else if (data.windSpeed >= 40)
      windColor = 0xFF9900;
    Theme::cell(bg_grad, colB, rowY(1), "Wind", buf, windColor);

    // UV index with its WHO level
    uint32_t uvColor = Theme::TEXT_DIM;
    if (data.uvIndex >= 0) {
      const char *level = "low";
      uvColor = Theme::GOOD;
      if (data.uvIndex >= 11) {
        level = "extreme";
        uvColor = 0xFF00FF;
      } else if (data.uvIndex >= 8) {
        level = "very high";
        uvColor = Theme::ALERT;
      } else if (data.uvIndex >= 6) {
        level = "high";
        uvColor = 0xFF8800;
      } else if (data.uvIndex >= 3) {
        level = "moderate";
        uvColor = Theme::WARN;
      }
      snprintf(buf, sizeof(buf), "%.0f %s", data.uvIndex, level);
    } else {
      snprintf(buf, sizeof(buf), "--");
    }
    Theme::cell(bg_grad, colA, rowY(2), "UV index", buf, uvColor);

    // Air quality, OWM scale 1..5 as words
    const char *aqiText = "--"; // No OWM key or fetch failed
    uint32_t aqiColor = Theme::TEXT_DIM;
    if (data.currentAQI >= 1) {
      aqiText = WeatherService::getAQIDesc(data.currentAQI);
      static const uint32_t aqiColors[] = {Theme::GOOD, 0xAADD33, Theme::WARN,
                                           0xFF9900, Theme::ALERT};
      aqiColor = aqiColors[min(data.currentAQI, 5) - 1];
    }
    Theme::cell(bg_grad, colB, rowY(2), "Air quality", aqiText, aqiColor);

    Theme::cell(bg_grad, colA, rowY(3), "Sunrise",
                data.sunrise.length() ? data.sunrise.c_str() : "--:--");
    Theme::cell(bg_grad, colB, rowY(3), "Sunset",
                data.sunset.length() ? data.sunset.c_str() : "--:--");

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

    initListStyles();

    // Hourly shows 12 rows (each row costs ~1.3KB of heap): OWM's 3-hour
    // slots cover 36h; Open-Meteo's 1-hour slots are shown every 2nd hour
    // to still cover 24h.
    const int hourStride = (data.hourlyStepHours == 1) ? 2 : 1;
    int count = isHourly ? 12 : 7;
    for (int i = 0; i < count; i++) {
      int h = isHourly ? i * hourStride : 0; // Index into data.hourly
      if (h >= 24)
        break;
      // Providers fill fewer slots than we have room for (OWM: ~6 days)
      if (isHourly ? data.hourly[h].time.isEmpty()
                   : data.daily[i].date.isEmpty())
        break;

      lv_obj_t *row = lv_obj_create(list);
      lv_obj_set_size(row, LV_PCT(100), 45);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_add_style(row, &styleRow, 0);
      lv_obj_add_style(row, (i % 2) ? &styleRowOdd : &styleRowEven, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

      // Time/Day
      lv_obj_t *time_lbl = lv_label_create(row);
      lv_obj_add_style(time_lbl, &styleTimeCol, 0);
      if (isHourly) {
        if (data.hourly[h].time.length() > 10)
          lv_label_set_text(time_lbl,
                            data.hourly[h].time.substring(11, 16).c_str());
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

      // Icon: 64px image centred in a 40x40 box and zoomed to fit. (Sizing
      // the image object itself to 40x40 makes LVGL tile and offset it.)
      lv_obj_t *icon_box = lv_obj_create(row);
      lv_obj_remove_style_all(icon_box); // Transparent, no border/padding
      lv_obj_add_style(icon_box, &styleIconBox, 0);
      lv_obj_clear_flag(icon_box,
                        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      lv_obj_t *icon = createWeatherIcon(
          icon_box,
          isHourly ? data.hourly[h].weatherCode : data.daily[i].weatherCode,
          isHourly && data.hourly[h].isNight); // Daily: day icon
      lv_img_set_zoom(icon, 160);

      // Rain Prob (List)
      float pop = isHourly ? data.hourly[h].pop : data.daily[i].pop;
      lv_obj_t *rain_lbl = lv_label_create(row);
      lv_obj_add_style(rain_lbl, &styleRainCol, 0);
      if (pop >= 0.1) { // Show if > 10%
        char rainBuf[16];
        snprintf(rainBuf, sizeof(rainBuf), "%.0f%%", pop * 100.0);
        lv_label_set_text(rain_lbl, rainBuf);
      } else {
        lv_label_set_text(rain_lbl, "");
      }

      // Trend
      if (!isHourly) {
        lv_obj_t *trend_lbl = lv_label_create(row);
        lv_obj_add_style(trend_lbl, &styleTrendCol, 0);
        if (i > 0) {
          float diff = data.daily[i].maxTemp - data.daily[i - 1].maxTemp;
          if (diff >= 1.0) {
            lv_label_set_text(trend_lbl, LV_SYMBOL_UP);
            lv_obj_add_style(trend_lbl, &styleTrendUp, 0);
          } else if (diff <= -1.0) {
            lv_label_set_text(trend_lbl, LV_SYMBOL_DOWN);
            lv_obj_add_style(trend_lbl, &styleTrendDown, 0);
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
        snprintf(buf, sizeof(buf), "%.1f°", data.hourly[h].temp);
      else
        snprintf(buf, sizeof(buf), "%.0f°/%.0f°", data.daily[i].minTemp,
                 data.daily[i].maxTemp);
      lv_label_set_text(temp_lbl, buf);
      lv_obj_add_style(temp_lbl, &styleTempCol, 0);
    }

    // Keep the scroll position when this same list is rebuilt with new data
    GuiController::trackListScroll(
        list, 100 + GuiController::getCityIndex() * 10 + forecastMode);
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
