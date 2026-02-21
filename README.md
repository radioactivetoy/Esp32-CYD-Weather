# ESP32 Weather & Bus Clock (CYD)

A smart desktop display for Barcelona, built on the **ESP32-2432S024C** (Cheap Yellow Display). It shows real-time weather from OpenWeatherMap, TMB (Transports Metropolitans de Barcelona) bus arrival times, and Stock/Crypto prices.

## 🎮 Controls & Navigation

The interface relies on intuitive **Touch Gestures**:

| Action | Gesture | Function |
| :--- | :--- | :--- |
| **Switch App** | **Swipe UP / DOWN** | Cycle between **Weather** ↔ **Bus** ↔ **Crypto/Stocks** |
| **Switch Page** | **Swipe LEFT / RIGHT** | **Weather**: Next/Prev City |
| **Switch Station**| **Tap Screen (Bus)** | **Bus**: Next Bus Stop |
| **Toggle View** | **Tap Screen (Weather)** | **Weather**: Cycle Views (Current → Hourly → Daily) |
| **Refresh Data** | **Auto / Swipe / Tap** | **Bus**: Auto-refreshes on entry, on tap, & every 60s <br> **Stock**: 5 min <br> **Weather**: 10 min |

## ✨ Features (Polished)

1.  **Multi-City Weather**:
    *   **Source**: OpenWeatherMap (5-Day / 3-Hour Forecast API).
    *   **Current**: Design-foward "Glassmorphism" card with Pills (Humidity, Wind, Pressure, AQI).
    *   **Rain Probability**: Shown in blue appended to description (e.g., "Overcast 30%") and in forecast lists.
    *   **Hourly**: Scrollable list of 24h forecast.
    *   **Daily**: 7-Day forecast with high/low temps and mid-day icons.
2.  **TMB Bus Tracker**:
    *   **Real-time Arrivals**: Shows minutes/seconds remaining.
    *   **Instant Fetch**: Triggers fresh data immediately upon swiping to the screen or tapping to switch stations.

3.  **Stock/Crypto Ticker**:
    *   **Real-time Prices**: Yahoo Finance Public API (No API key required).
    *   **Trend Tracking**: Green/Red indicators for price changes.
    *   **Clean List**: Scrollable view of all configured symbols.

4.  **Hardware Support**:
    *   **Display**: CYD (Cheap Yellow Display) - ESP32-2432S024C.
    *   **Touch**: CST816S Capacitive Touch (Gestures + Taps).
    *   **Sensor**: LDR (Light Dependent Resistor) for Auto-Brightness.

## Hardware

-   **Board**: ESP32-2432S024C (2.4" Touch Screen with CST820/CST816S).
    -   *Commonly known as the "Cheap Yellow Display" (CYD).*
-   **Power**: USB-C or Micro-USB (depending on board variant).
-   **Sensors**: Built-in LDR (Light Sensor) and RGB LED.

## Installation

1.  **Install PlatformIO**: This project is built using PlatformIO (VSCode Extension).
2.  **Open Project**: Open this folder in VSCode.
3.  **Build & Flash**:
    -   Connect your device via USB.
    -   Run the command: `pio run --target upload`.
    -   Monitor output: `pio device monitor`.

## Configuration (Web UI)

No need to edit code! Configure everything via the web interface.

1.  **Connect to WiFi**:
    -   On first boot, connect to hotspot **`WeatherClockAP`**.
    -   Go to `192.168.4.1` to set up your home WiFi.
2.  **Access Settings**:
    -   Find the device IP in the Serial Monitor (e.g., `192.168.1.45`).
    -   Open that IP in your browser.
3.  **Customize**:
    -   **City & Bus Stop**: Set your location (comma-separated for multiple cities/stops).
    -   **Stocks**: Comma-separated symbols (e.g., `AAPL,BINANCE:BTCUSDT`).
    -   **Lighting**:
        -   **Day Brightness**: Slider (1-100%) for active hours.
        -   **Night Brightness**: Slider (1-100%) for night mode.
        -   **Night Mode**: Enable auto-dimming between specific hours.
    -   **Timezone**: Select your local time.
    -   **LED Brightness**: Set RGB LED intensity (Low/Medium/High).
    -   **API Keys**: Enter your TMB App ID/Key and OpenWeatherMap API Key.

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
-   **OpenWeatherMap Migration**: Fully replaced Open-Meteo for Forecasts, Geocoding, and AQI for better accuracy.
-   **Enhanced Rain UI**: Rain probability now displayed alongside weather description and in forecast lists.
-   **Configurable Backlight**: Set specific brightness levels for Day and Night modes via Web UI.
-   **Touch Navigation**: Simplified Bus Station switching via tap.

## API Keys

You need free API keys for data sources:
-   **TMB API**: Register at [developer.tmb.cat](https://developer.tmb.cat/) (Bus Data).
-   **OpenWeatherMap**: Register at [openweathermap.org](https://openweathermap.org/) (Weather Data).

## Project Structure

-   `src/main.cpp`: Main loop and task scheduler.
-   `lib/GuiController`: LVGL UI logic, screens, and rendering.
-   `lib/NetworkManager`: WiFi, NTP, NVS Storage, and Web Server.
-   `lib/BusService`: TMB API client.
-   `lib/StockService`: Finnhub API client.
-   `lib/WeatherService`: OpenWeatherMap API client.
-   `lib/LedController`: RGB LED management and alerts.
-   `lib/TouchDrv`: Driver for CST820/CST816S touch controller.
