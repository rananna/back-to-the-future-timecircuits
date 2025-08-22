# 💡 Usage & Configuration Guide

This guide covers the day-to-day use and configuration of your Time Circuits display via its built-in web interface.

## First-Time WiFi Setup

1.  **Connect to the Hotspot**: On the first boot, the ESP32 will create a WiFi network named **BTTF-Clock-Setup**. Connect to this network with your phone or computer.
2.  **Captive Portal**: A captive portal should automatically open in your browser. If it doesn't, navigate to `192.18.4.1`.
3.  **Configure WiFi**: Select your home WiFi network (SSID), enter the password, and click "Save". The device will restart and connect to your network.

## Accessing the Web Interface

Once connected, you can access the web UI by navigating to the device's IP address in your browser. You can find the IP address in your router's client list or by monitoring the Serial Monitor in the Arduino IDE during boot.

<p align="center">
  <img src="../images/webui.png" alt="Web UI Screenshot" width="800">
</p>

## Web Interface Tabs

The web interface is organized into several tabs for easy configuration.

### Time Circuits Tab
* **Destination Time & Year**: Set the target year for the "Destination Time" display.
* **Last Time Departed & Presets**: Select from a list of famous dates from the movies, or add, update, and delete your own custom presets.

### Temporal Controls Tab
* **Departure/Arrival (Sleep) Times**: Configure a daily schedule to automatically turn the displays off and on to save power.
* **Display & Animation**: Adjust the display brightness, select from 10 different time travel animation styles, and control the duration and frequency of automatic animations.
* **Effects**: Control the "instability" of your time circuits by setting the frequency of random visual glitches and the chance of a major malfunction sequence.
* **Sound**: Adjust the volume of the sound effects and toggle them on or off.

### Data Link Tab
This is where you configure the bottom display row to show live, real-time data. You can enable either the Live Weather Display or the Data Link Marquee.

#### Live Weather Display
* Transforms the bottom row into a multi-page weather station.
* Simply enter a city name, and the clock will automatically fetch and display current conditions, temperature, wind speed, and more.

#### Data Link Marquee
This is a fully configurable marquee for displaying data from the internet.
* **Data Sources**: Configure up to 5 data points. Each can get its data from:
    1.  **Web API (HTTP)**: Pull data from any JSON-based web API.
    2.  **MQTT Broker**: Subscribe to an MQTT topic for real-time data from smart home devices.
    3.  **Home Assistant Push**: Allow Home Assistant to push data directly to the display slot.
* **API Wizard**: An easy-to-use tool that fetches data from a URL and lets you visually map JSON values to the displays without writing any code.

### Network & System Tab
* **Present Time**: Set your current time zone. The clock will automatically handle Daylight Saving Time. You can also manually trigger a sync with NTP time servers.
* **UI Theme**: Change the color scheme of the web interface.
* **Device Actions**: Trigger a "Great Scott!" easter egg or reset all settings to their factory defaults.