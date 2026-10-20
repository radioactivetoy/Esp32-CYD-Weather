# ESP32 Weather & Bus Clock (CYD)

A smart desktop display for Barcelona, built on the **ESP32-2432S024C** (Cheap Yellow Display). It shows real-time weather from OpenWeatherMap (or keyless Open-Meteo), TMB (Transports Metropolitans de Barcelona) bus arrival times, and Stock/Crypto prices.

## 🎮 Controls & Navigation

The interface relies on intuitive **Touch Gestures**:

| Action | Gesture | Function |
| :--- | :--- | :--- |
| **Switch App** | **Swipe UP / DOWN** | Cycle between **Weather** ↔ **Bus** ↔ **Crypto/Stocks** |
| **Switch Page** | **Swipe LEFT / RIGHT** | **Weather**: Next/Prev City |
| **Switch Station**| **Tap Screen (Bus)** | **Bus**: Next Bus Stop |
| **Toggle View** | **Tap Screen (Weather)** | **Weather**: Cycle Views (Current → Hourly → Daily) |
| **Refresh Now** | **Long-press screen** | Refreshes the current app immediately |
| **Device Info** | **Long-press header** | WiFi, IP, uptime, memory; tap to close |
| **Refresh Data** | **Auto / Swipe / Tap** | **Bus**: Auto-refreshes on entry, on tap, & every 60s <br> **Stock**: 5 min <br> **Weather**: 15 min (10 min when switching city) |

## ✨ Features (Polished)

1.  **Multi-City Weather**:
    *   **Source**: OpenWeatherMap (5-Day / 3-Hour Forecast API) when an OWM key is set; otherwise Open-Meteo (no key needed, hourly data). Failed fetches back off (30s up to 15 min) instead of retrying every second.
    *   **Current**: Big temperature (colour-coded) with the weather icon, description and high/low, over a grid of Feels like, Rain, Humidity, Wind, UV index (with level), Air quality, Sunrise and Sunset.
    *   **Icons**: 21 [Meteocons](https://github.com/basmilius/weather-icons) with day/night variants, including sleet, hail and heavy rain. With OWM, the detailed condition codes are used so heavy rain, freezing rain and sleet are told apart.
    *   **Rain Probability**: In the details grid and in the forecast lists.
    *   **Hourly**: Scrollable 12-row forecast: every 3 hours over the next 36h (OWM) or every 2 hours over 24h (Open-Meteo).
    *   **Daily**: 7-day forecast with high/low temps and mid-day icons. With OWM, today's full-day high/low, UV and the days beyond OWM's range come from one extra Open-Meteo request.
2.  **TMB Bus Tracker**:
    *   **Real-time Arrivals**: Minutes to arrival, counting down between fetches; "Due" when the bus is arriving.
    *   **Instant Fetch**: Triggers fresh data immediately upon swiping to the screen or tapping to switch stations.

3.  **Stock/Crypto Ticker**:
    *   **Real-time Prices**: Yahoo Finance Public API (No API key required).
    *   **Trend Tracking**: Green/Red indicators for price changes.
    *   **Clean List**: Scrollable view of all configured symbols.

4.  **Hardware Support**:
    *   **Display**: CYD (Cheap Yellow Display) - ESP32-2432S024C.
    *   **Touch**: CST816S Capacitive Touch (Gestures + Taps).
    *   **Night mode** (optional): Screen off between set hours; tap to wake it for 30 s.

5.  **Reliability**:
    *   **WiFi recovery**: A lost connection is retried every 15 s, fully re-established after 2 minutes, and the clock restarts cleanly after 10 minutes offline. Nothing is fetched while offline; everything stale refreshes as soon as WiFi is back.
    *   **No hangs**: Every request has connect, TLS and read timeouts that fit inside the network task's watchdog; failures back off (30 s up to 15 min) and weather falls back to Open-Meteo.
    *   **Diagnostics**: The settings page Status section and the long-press info overlay show the last reset reason, WiFi drops with the last reason, memory and stack headroom. The serial log prints a `MON:` line every minute and `BOOT: last reset: ...` at startup.

## Credits

-   Weather icons: [Meteocons](https://github.com/basmilius/weather-icons) by Bas Milius (MIT License), converted with `tools/convert_meteocons.py`.

## Hardware

-   **Board**: ESP32-2432S024C (2.4" Touch Screen with CST820/CST816S).
    -   *Commonly known as the "Cheap Yellow Display" (CYD).*
-   **Power**: USB-C or Micro-USB (depending on board variant).
-   **Status LED**: Built-in RGB LED shows the rain outlook.

## Installation

1.  **Install PlatformIO**: This project is built using PlatformIO (VSCode Extension).
2.  **Open Project**: Open this folder in VSCode.
3.  **Build & Flash**:
    -   Connect your device via USB.
    -   Run the command: `pio run --target upload`.
    -   Monitor output: `pio device monitor`.

### Updating over WiFi (OTA)

After the first USB flash, updates can go over WiFi:

```
pio run -e ota -t upload
```

-   The device announces itself as `weatherclock.local`. If your PC can't resolve it, use the IP (shown on screen at boot and on the settings page): `pio run -t upload --upload-port 192.168.x.x`.
-   **Password**: the Settings & OTA Password (set in the WiFi setup portal or the web UI) protects both the settings page (user `admin`) and OTA. Without one, anyone on your network can change settings or flash firmware. For uploads, pass it through an environment variable so it never ends up in git:
    -   PowerShell: `$env:WEATHER_OTA_PASSWORD = "your-password"`
    -   bash: `export WEATHER_OTA_PASSWORD=your-password`
-   The screen shows the progress; the device restarts when done.
-   OTA needs two app slots, so the project uses the `min_spiffs.csv` partition table (2 × 1.9 MB). Coming from an older build that used `huge_app.csv`, do **one USB flash** first; WiFi and settings are kept.

## Configuration (Web UI)

No need to edit code! Configure everything via the web interface.

1.  **Connect to WiFi**:
    -   On first boot, connect to hotspot **`WeatherClockAP`**.
    -   Go to `192.168.4.1` to set up your home WiFi.
2.  **Access Settings**:
    -   Find the device IP in the Serial Monitor (e.g., `192.168.1.45`).
    -   Open that IP in your browser.
3.  **Customize**:
    -   **City & Bus Stop**: Set your location (comma-separated for multiple cities/stops, max 5 each). To add a country code, separate cities with `;` instead, e.g. `Paris,FR;London,GB`.
    -   **Stocks**: Comma-separated Yahoo Finance symbols (e.g., `AAPL,BTC-USD,GRF.MC`).
    -   **Lighting**:
        -   **Night Mode** (optional): Turns the screen off between the set hours. A tap turns it on for 30 seconds; that first tap only wakes it.
    -   **Timezone**: Select your local time.
    -   **LED Brightness**: Set RGB LED intensity (Low/Medium/High).
    -   **API Keys**: Enter your TMB App ID/Key and (optionally) OpenWeatherMap API Key. Without an OWM key, weather comes from Open-Meteo and AQI is not shown.
    -   **Settings & OTA Password** (optional, recommended): Protects this page (HTTP basic auth, user `admin`) and OTA updates. Can also be set in the first-time WiFi setup portal.
    -   **Status**: the bottom of the page shows firmware build, uptime, last reset reason, WiFi signal and drops, free memory and task stack headroom. The serial log prints the same as a `MON:` line every minute, with warnings when memory or stack runs low.

## Controls
-   **Swipe Up/Down**: Cycle between Apps (Weather <-> Bus <-> Stocks).
-   **Weather App**:
    -   **Tap Screen**: Toggle Forecast Mode (Current -> Hourly -> Daily).
    -   **Swipe Left/Right**: Switch between configured Cities (comma-separated list in Web UI).
-   **Bus App**:
    -   **Tap Screen**: Switch to next Bus Stop (if multiple are configured as comma-separated list).
-   **Stocks App**:
    -   Displays configured stock quotes with real-time price/change.

## Recent Updates
-   **NVS Optimization**: Reduced flash memory wear by caching API credentials in RAM instead of reading NVS every 60 seconds.
-   **OpenWeatherMap**: Preferred source for forecasts, geocoding and AQI when a key is configured; Open-Meteo is the keyless fallback.
-   **Enhanced Rain UI**: Rain probability shown in the details grid and in the forecast lists.
-   **Night mode**: The screen switches off at night instead of dimming (the panel loses its contrast when dimmed); tap to wake.
-   **Redesign**: Black theme with hairline dividers, big temperature hero and a details grid; consistent header, lists and settings page.
-   **Reliability**: WiFi auto-recovery, watchdog-safe network timeouts, reset and WiFi-drop diagnostics.
-   **Icons**: Sharper Meteocons (cropped to fill the image, dithered), plus sleet, hail, heavy rain and night drizzle/showers.
-   **Touch Navigation**: Simplified Bus Station switching via tap.

## API Keys

You need free API keys for data sources:
-   **TMB API**: Register at [developer.tmb.cat](https://developer.tmb.cat/) (Bus Data).
-   **OpenWeatherMap**: Register at [openweathermap.org](https://openweathermap.org/) (Optional: better forecasts + AQI).

## Project Structure

-   `src/main.cpp`: Main loop and task scheduler.
-   `lib/GuiController`: LVGL UI logic, screens, and rendering.
-   `lib/NetworkManager`: WiFi, NTP, NVS Storage, and Web Server.
-   `lib/BusService`: TMB API client.
-   `lib/StockService`: Yahoo Finance client.
-   `lib/WeatherService`: OpenWeatherMap and Open-Meteo client.
-   `lib/DataManager`: Background network task: fetch scheduling, backoff, WiFi recovery.
-   `lib/SystemMonitor`: Memory / stack monitoring, reset reason, WiFi drop counting.
-   `lib/LedController`: RGB LED management and alerts.
-   `lib/TouchDrv`: Driver for CST820/CST816S touch controller.
-   `tools/convert_meteocons.py`: Regenerates the weather icons (`uv run --with pillow python tools/convert_meteocons.py`).
