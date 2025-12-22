# ESP32 Weather & Bus Clock (CYD)

A smart desktop display for Barcelona, built on the **ESP32-2432S024C** (Cheap Yellow Display). It shows real-time weather from Open-Meteo, TMB (Transports Metropolitans de Barcelona) bus arrival times, and Stock/Crypto prices.

## Features

-   **Real-Time Weather**: Current temperature, humidity, wind, and AQI (Air Quality Index).
-   **Forecasts**: Swipe to see Hourly (24h) and Daily (7-day) forecasts.
-   **Bus Arrivals**: Real-time arrival estimates for your favorite Barcelona TMB bus stop. Includes accurate colors for V/H/D lines.
-   **Stock Ticker**: Track US Stocks (e.g., AAPL) and Crypto (e.g., BTC-USD) in real-time via Finnhub.
-   **LED Weather Alerts**: RGB LED indicates rain forecast (Red=Rain, Orange=Soon, Yellow=Later, Green=Clear).
-   **Smart Clock**: Syncs automatically with internet time (NTP).
-   **Web Configuration**: Configure all settings (WiFi, Keys, Location, LED, Night Mode) from your browser.
-   **Global Navigation**: Circular app cycling (Weather ↔️ Stocks ↔️ Bus).

## Navigation (Gestures)

**Switching Apps (Vertical Swipes)**:
-   **Swipe UP**: Next App (Weather -> Stocks -> Bus -> Weather).
-   **Swipe DOWN**: Previous App (Weather -> Bus -> Stocks -> Weather).

**Inside Weather App**:
-   **Swipe LEFT/RIGHT**: Cycle Views (Current -> Hourly -> Daily).

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
    -   **City & Bus Stop**: Set your location.
    -   **Stocks**: Comma-separated symbols (e.g., `AAPL,BINANCE:BTCUSDT`).
    -   **Timezone**: Select your local time.
    -   **Night Mode**: Set start/end hours for screen dimming.
    -   **LED Brightness**: Set RGB LED intensity (Low/Medium/High).
    -   **API Keys**: Enter your TMB App ID/Key and Finnhub API Key.

## API Keys

You need free API keys for data sources:
-   **TMB API**: Register at [developer.tmb.cat](https://developer.tmb.cat/) (Bus Data).
-   **Finnhub**: Register at [finnhub.io](https://finnhub.io/) (Stock Data).
-   *Weather data (Open-Meteo) is free and key-less.*

## Project Structure

-   `src/main.cpp`: Main loop and task scheduler.
-   `lib/GuiController`: LVGL UI logic, screens, and rendering.
-   `lib/NetworkManager`: WiFi, NTP, NVS Storage, and Web Server.
-   `lib/BusService`: TMB API client.
-   `lib/StockService`: Finnhub API client.
-   `lib/WeatherService`: Open-Meteo API client.
-   `lib/LedController`: RGB LED management and alerts.
-   `lib/TouchDrv`: Driver for CST820/CST816S touch controller.
