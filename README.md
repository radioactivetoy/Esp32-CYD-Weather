# ESP32 Weather & Bus Clock (CYD)

A smart desktop display for Barcelona, built on the **ESP32-2432S024C** (Cheap Yellow Display). It shows real-time weather from Open-Meteo and TMB (Transports Metropolitans de Barcelona) bus arrival times.

## Features

-   **Real-Time Weather**: Current temperature, humidity, wind, and AQI (Air Quality Index).
-   **Forecasts**: Swipe to see Hourly (24h) and Daily (7-day) forecasts.
-   **Bus Arrivals**: Real-time arrival estimates for your favorite TMB bus stop. Includes accurate colors for V/H/D lines.
-   **Smart Clock**: Syncs automatically with internet time (NTP).
-   **Web Configuration**: Configure City, Bus Stop, and API Keys directly from your browser.
-   **Gestures**:
    -   **Swipe UP**: Switch to Bus Screen.
    -   **Swipe LEFT/RIGHT**: Cycle Weather views (Current -> Hourly -> Daily -> Trends).
    -   **Swipe DOWN**: Refresh data.

## Hardware

-   **Board**: ESP32-2432S024C (2.4" Touch Screen with CST820/CST816S).
    -   *Commonly known as the "Cheap Yellow Display" (CYD).*
-   **Power**: USB-C or Micro-USB (depending on board variant).

## Installation

1.  **Install PlatformIO**: This project is built using PlatformIO (VSCode Extension).
2.  **Open Project**: Open this folder in VSCode.
3.  **Build & Flash**:
    -   Connect your device via USB.
    -   Run the command: `pio run --target upload`.
    -   Monitor output: `pio device monitor`.

## Configuration (Web UI)

No need to edit code to change your settings!

1.  **Connect to WiFi**:
    -   On first boot, the device will create a hotspot named **`WeatherClockAP`**.
    -   Connect to it (Password: none or check serial logs).
    -   Go to `192.168.4.1` to configure your home WiFi credentials.
2.  **Access Settings**:
    -   Once connected to your WiFi, the device will show its IP address on the Serial Monitor (e.g., `192.168.1.XX`).
    -   Open that IP in your web browser.
3.  **Setup Keys & Location**:
    -   **City Name**: Your city (e.g., "Barcelona").
    -   **Bus Stop ID**: The TMB Stop Code (e.g., "2156").
    -   **TMB App ID**: Your TMB API App ID.
    -   **TMB App Key**: Your TMB API App Key.
    -   Click **Save & Reboot**.

## API Keys

You need free API keys for the bus data:
-   **TMB API**: Register at [developer.tmb.cat](https://developer.tmb.cat/) to get your `App ID` and `App Key`.
-   *Weather data (Open-Meteo) does not require an API key.*

## Project Structure

-   `src/main.cpp`: Main loop and task scheduler.
-   `lib/GuiController`: LVGL UI logic, screens, and rendering.
-   `lib/NetworkManager`: WiFi, NTP, and Web Server logic.
-   `lib/BusService`: TMB API client.
-   `lib/WeatherService`: Open-Meteo API client.
-   `lib/TouchDrv`: Driver for CST820/CST816S touch controller.
