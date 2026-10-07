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
// Setting each property on each object allocates a private style per object;
// these are created once and only referenced. Rows themselves come from
// Theme::row (hairline on top, flex row).
static lv_style_t styleIconBox;  // Holder for the zoomed list icon
static lv_style_t styleTimeCol;  // Hourly: time, white like the daily day names
static lv_style_t styleRainCol;  // Hourly: rain %, blue, right-aligned
static lv_style_t styleTempCol;  // Hourly: temperature, fills the rest, right
static lv_style_t styleDayCol;   // Daily: day + rain stacked
static lv_style_t styleLowCol;   // Daily: low, right-aligned
static lv_style_t styleHighCol;  // Daily: high, right-aligned
static lv_style_t styleTrack;    // Daily: low..high bar track
static lv_style_t styleFill;     // Daily: bar fill (colour set per row)

static void initListStyles() {
  static bool done = false;
  if (done)
    return;
  done = true;

  // Meteocons fill ~70% of their box: 64px zoomed to 44px, clipped to the
  // 36px row height only through its transparent margin
  lv_style_init(&styleIconBox);
  lv_style_set_width(&styleIconBox, 40);
  lv_style_set_height(&styleIconBox, 36);

  lv_style_init(&styleTimeCol);
  lv_style_set_width(&styleTimeCol, 50);
  lv_style_set_text_font(&styleTimeCol, &Theme::body);
  lv_style_set_text_color(&styleTimeCol, lv_color_hex(Theme::TEXT));

  lv_style_init(&styleRainCol);
  lv_style_set_width(&styleRainCol, 44);
  lv_style_set_text_align(&styleRainCol, LV_TEXT_ALIGN_RIGHT);
  lv_style_set_text_font(&styleRainCol, &Theme::small);
  lv_style_set_text_color(&styleRainCol, lv_color_hex(Theme::RAIN));

  lv_style_init(&styleTempCol);
  lv_style_set_flex_grow(&styleTempCol, 1);
  lv_style_set_text_align(&styleTempCol, LV_TEXT_ALIGN_RIGHT);
  lv_style_set_text_font(&styleTempCol, &Theme::body);
  lv_style_set_text_color(&styleTempCol, lv_color_hex(Theme::TEXT));

  lv_style_init(&styleDayCol);
  lv_style_set_width(&styleDayCol, 52);
  lv_style_set_layout(&styleDayCol, LV_LAYOUT_FLEX);
  lv_style_set_flex_flow(&styleDayCol, LV_FLEX_FLOW_COLUMN);

  lv_style_init(&styleLowCol);
  lv_style_set_width(&styleLowCol, 30);
  lv_style_set_text_align(&styleLowCol, LV_TEXT_ALIGN_RIGHT);
  lv_style_set_text_font(&styleLowCol, &Theme::body);
  lv_style_set_text_color(&styleLowCol, lv_color_hex(Theme::TEXT));

  lv_style_init(&styleHighCol);
  lv_style_set_width(&styleHighCol, 32);
  lv_style_set_text_align(&styleHighCol, LV_TEXT_ALIGN_RIGHT);
  lv_style_set_text_font(&styleHighCol, &Theme::body);
  lv_style_set_text_color(&styleHighCol, lv_color_hex(Theme::TEXT));

  lv_style_init(&styleTrack);
  lv_style_set_flex_grow(&styleTrack, 1);
  lv_style_set_height(&styleTrack, 4);
  lv_style_set_radius(&styleTrack, 2);
  lv_style_set_bg_opa(&styleTrack, LV_OPA_COVER);
  lv_style_set_bg_color(&styleTrack, lv_color_hex(Theme::DIVIDER));

  lv_style_init(&styleFill);
  lv_style_set_height(&styleFill, 4);
  lv_style_set_radius(&styleFill, 2);
  lv_style_set_bg_opa(&styleFill, LV_OPA_COVER);
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

  // === HEADER === (city name; Hourly / Daily say so in the subtitle)
  lv_obj_t *header_row = GuiController::createHeader(
      bg_grad, data.cityName.length() > 0 ? data.cityName.c_str() : "Unknown",
      GuiController::cityCount, GuiController::getCityIndex());
  // Long press: body = refresh now, header = device info
  GuiController::attachLongPress(bg_grad, header_row);

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

    // Hero: icon + text column (temperature, description, H/L) as one group,
    // centred on the screen, with the icon centred on the column's height
    auto plainBox = [](lv_obj_t *parent) {
      lv_obj_t *o = lv_obj_create(parent);
      lv_obj_remove_style_all(o); // No padding, border or background
      lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      return o;
    };

    lv_obj_t *hero = plainBox(bg_grad);
    lv_obj_set_size(hero, LV_PCT(100), 104);
    lv_obj_set_pos(hero, 0, 40);
    lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hero, 10, 0);

    // Meteocons fill only ~45 of their 64px; scale 1.25x so the icon holds
    // its own next to the big digits
    lv_obj_t *icon_wrap = plainBox(hero);
    lv_obj_set_size(icon_wrap, 84, 84);
    lv_obj_t *hero_icon =
        createWeatherIcon(icon_wrap, data.currentWeatherCode, data.isNight);
    lv_img_set_zoom(hero_icon, 320);

    lv_obj_t *col = plainBox(hero);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 4, 0);

    // Temperature (whole degrees; the decimal is false precision) with a
    // small arrow if tomorrow is >= 1 degree warmer / cooler
    lv_obj_t *temp_row = plainBox(col);
    lv_obj_set_size(temp_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(temp_row, 4, 0);

    snprintf(buf, sizeof(buf), "%d\xC2\xB0", (int)lroundf(data.currentTemp));
    Theme::label(temp_row, buf, &Theme::digits,
                 Theme::tempColor(data.currentTemp));

    float diff = data.daily[1].maxTemp - data.daily[0].maxTemp;
    if (data.daily[1].date.length() > 0 && fabsf(diff) >= 1.0f)
      Theme::label(temp_row, diff > 0 ? LV_SYMBOL_UP : LV_SYMBOL_DOWN,
                   &lv_font_montserrat_14, diff > 0 ? 0xFF7755 : 0x6688FF);

    Theme::label(col, getWeatherDesc(data.currentWeatherCode), &Theme::body,
                 Theme::TEXT);

    snprintf(buf, sizeof(buf), "H %.0f\xC2\xB0  \xC2\xB7  L %.0f\xC2\xB0",
             data.daily[0].maxTemp, data.daily[0].minTemp);
    Theme::label(col, buf, &Fonts::text14, Theme::TEXT);

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
    // === LIST VIEWS === hairline rows under a subtitle
    bool isHourly = (forecastMode == 1);
    initListStyles();

    // Hourly shows 12 rows (heap): OWM's 3-hour slots cover 36h; Open-Meteo's
    // 1-hour slots are shown every 2nd hour to still cover 24h.
    const int hourStride = (data.hourlyStepHours == 1) ? 2 : 1;
    int days = 0;
    while (days < 7 && data.daily[days].date.length() > 0)
      days++;

    if (isHourly) {
      snprintf(buf, sizeof(buf), "Next %d hours",
               12 * hourStride * data.hourlyStepHours);
    } else {
      snprintf(buf, sizeof(buf), "%d days", days);
    }
    Theme::subtitle(bg_grad, buf);

    lv_obj_t *list = Theme::list(bg_grad, Theme::CONTENT_Y);

    // Week range for the daily low..high bars (shared scale)
    float weekMin = 100, weekMax = -100;
    for (int i = 0; i < days; i++) {
      weekMin = min(weekMin, data.daily[i].minTemp);
      weekMax = max(weekMax, data.daily[i].maxTemp);
    }
    float weekRange = max(weekMax - weekMin, 1.0f);

    auto addIcon = [](lv_obj_t *row, int code, bool night) {
      lv_obj_t *box = Theme::plainBox(row);
      lv_obj_add_style(box, &styleIconBox, 0);
      lv_obj_t *icon = createWeatherIcon(box, code, night);
      lv_img_set_zoom(icon, 176); // 64px -> 44px
    };

    int count = isHourly ? 12 : days;
    for (int i = 0; i < count; i++) {
      lv_obj_t *row = Theme::row(list, 36);

      if (isHourly) {
        int h = i * hourStride; // Index into data.hourly
        if (h >= 24 || data.hourly[h].time.isEmpty()) {
          lv_obj_del(row);
          break;
        }
        const HourlyForecast &hf = data.hourly[h];

        lv_obj_t *t = lv_label_create(row);
        lv_obj_add_style(t, &styleTimeCol, 0);
        lv_label_set_text(t, hf.time.length() > 10
                                 ? hf.time.substring(11, 16).c_str()
                                 : "--:--");

        addIcon(row, hf.weatherCode, hf.isNight);

        lv_obj_t *rain = lv_label_create(row);
        lv_obj_add_style(rain, &styleRainCol, 0);
        if (hf.pop >= 0.1f) { // Only when likely enough to matter
          snprintf(buf, sizeof(buf), "%.0f%%", hf.pop * 100.0f);
          lv_label_set_text(rain, buf);
        } else {
          lv_label_set_text(rain, "");
        }

        lv_obj_t *temp = lv_label_create(row);
        lv_obj_add_style(temp, &styleTempCol, 0);
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", hf.temp);
        lv_label_set_text(temp, buf);
      } else {
        const DailyForecast &df = data.daily[i];

        // Day name with the rain chance underneath (no room for its own
        // column next to the bar)
        lv_obj_t *dayCol = Theme::plainBox(row);
        lv_obj_add_style(dayCol, &styleDayCol, 0);
        lv_obj_set_height(dayCol, LV_SIZE_CONTENT);
        char dateBuf[16];
        if (i == 0)
          snprintf(dateBuf, sizeof(dateBuf), "Today");
        else
          formatDate(df.date.c_str(), dateBuf); // "Wed 8"
        Theme::label(dayCol, dateBuf, &Theme::body, Theme::TEXT);
        if (df.pop >= 0.1f) {
          snprintf(buf, sizeof(buf), "%.0f%%", df.pop * 100.0f);
          Theme::label(dayCol, buf, &Theme::small, Theme::RAIN);
        }

        addIcon(row, df.weatherCode, false);

        lv_obj_t *lo = lv_label_create(row);
        lv_obj_add_style(lo, &styleLowCol, 0);
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", df.minTemp);
        lv_label_set_text(lo, buf);

        // Low..high on the week's scale
        lv_obj_t *track = Theme::plainBox(row);
        lv_obj_add_style(track, &styleTrack, 0);
        lv_obj_t *fill = Theme::plainBox(track);
        lv_obj_add_style(fill, &styleFill, 0);
        lv_obj_set_style_bg_color(fill, lv_color_hex(Theme::tempColor(df.maxTemp)),
                                  0);
        int x = (int)((df.minTemp - weekMin) * 100.0f / weekRange);
        int w = (int)((df.maxTemp - df.minTemp) * 100.0f / weekRange);
        lv_obj_set_x(fill, lv_pct(constrain(x, 0, 96)));
        lv_obj_set_width(fill, lv_pct(constrain(w, 4, 100 - x)));

        lv_obj_t *hi = lv_label_create(row);
        lv_obj_add_style(hi, &styleHighCol, 0);
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", df.maxTemp);
        lv_label_set_text(hi, buf);
      }
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
