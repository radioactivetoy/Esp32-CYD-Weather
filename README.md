# ESP32 Weather & Bus Clock (CYD)

A smart desktop display for Barcelona, built on the **ESP32-2432S024C** (Cheap Yellow Display). It shows real-time weather from Open-Meteo, TMB (Transports Metropolitans de Barcelona) bus arrival times, and Stock/Crypto prices.

## 🎮 Controls & Navigation

The interface relies on intuitive **Touch Gestures**:

| Action | Gesture | Function |
| :--- | :--- | :--- |
| **Switch App** | **Swipe UP / DOWN** | Cycle between **Weather** ↔ **Bus** ↔ **Crypto/Stocks** |
| **Switch Page** | **Swipe LEFT / RIGHT** | **Weather**: Next/Prev City <br> **Bus**: Next/Prev Bus Stop |
| **Toggle View** | **Tap Screen** | **Weather**: Cycle Views (Current → Hourly → Daily → Graph) |
| **Refresh Data** | **Auto / Swipe Entry** | **Bus**: Auto-refreshes on entry (Instant) & every 60s <br> **Stock**: 5 min |

## ✨ Features (Polished)

1.  **Multi-City Weather**:
    *   **Current**: Design-foward "Glassmorphism" card with Pills (Humidity, Wind, AQI).
    *   **Hourly**: Scrollable list of 24h forecast.
    *   **Daily**: 7-Day forecast with high/low temps.
    *   **Graph**: 24h Temperature Trend visualization.
    *   **Unified Header**: Clean 40px Header (Title Left, Time Right) matches all apps.

2.  **TMB Bus Tracker**:
    *   **Real-time Arrivals**: Shows minutes/seconds remaining.
    *   **Color Coded**:
        *   🔴 Arriving Now (< 2 min) - *Pulses!*
        *   🟡 Approaching (< 5 min)
        *   🟢 Free Flow (> 5 min)
    *   **Optimized Header**: Max-width stop name with background watermarked icon.
    *   **Instant Fetch**: Triggers fresh data immediately upon swiping to the screen.

3.  **Stock/Crypto Ticker**:
    *   **Real-time Prices**: Yahoo Finance API.
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
