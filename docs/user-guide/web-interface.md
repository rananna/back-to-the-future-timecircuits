# 💡 Web Interface Guide

This guide covers the use and configuration of your Time Circuits display via its built-in web interface.

## First-Time WiFi Setup

On its first boot, the device will create its own WiFi hotspot to allow you to configure it.

1.  **Connect to the Hotspot**: Using your phone or computer, connect to the WiFi network named **`TimeCircuits-Setup`**.
2.  **Captive Portal**: A configuration page should automatically open. If it doesn't, manually navigate to `http://192.168.4.1` in your browser.
3.  **Configure and Save**: Click **"Configure WiFi,"** select your home network, enter your password, and click **"Save"**. The device will restart and connect to your network.

## Accessing the Web Interface

Once connected to your network, you can access the web UI by navigating to **`http://BTTF_TC.local`** in your browser. If that doesn't work, you can find the clock's IP address in your router's client list.

---

## Web Interface Tabs

The web interface is organized into six tabs for managing all aspects of your clock.

### **Time Circuits**
This is the main screen for setting the time displays.
*   **Destination Time**: Set the year and time zone for the top display row.
*   **Last Time Departed & Presets**: Control the bottom display row by selecting movie-based presets or creating your own custom dates.
*   **Automatic Cycling**: Set an interval (in minutes) for the clock to automatically cycle through the presets (`0` disables).

### **Temporal Controls**
This tab controls the clock's automatic behaviors and visual effects.
*   **Sleep Schedule**: Set a daily schedule to automatically turn the displays off and on.
*   **Display**: Adjust brightness and toggle 24-hour format.
*   **Animation Sequences**: Select and run any of the built-in, multi-track cinematic animations.
*   **Sound**: Control the master volume and enable/disable time travel sound effects.
*   **Favorite Radio**: Configure and play your favorite internet radio stream.

### **Connectivity**
This tab manages all network-related settings.
*   **MQTT Broker**: Configure the connection to your MQTT broker, which is required for Home Assistant integration and the "Data Link" features.
*   **Present Time (NTP)**: Set your local time zone.

### **Data Link**
This tab unlocks advanced data display capabilities.
*   **Stock Ticker**: Shows real-time stock prices. Requires a free API key from [Financial Modeling Prep](https://site.financialmodelingprep.com/developer/docs).
*   **Live Weather**: Shows the current weather for a specified city.
*   **Data Link**: For advanced users, this allows the clock to display custom data pushed from an MQTT broker.

### **System**
This tab provides device status and system-level actions.
*   **System Status**: View WiFi signal strength, free memory, and uptime.
*   **Firmware Update**: Update the device's software over the air (OTA).
*   **UI Theme**: Customize the look of the web interface.
*   **Device Actions**: Trigger a "Great Scott!" animation or reset all settings to their factory defaults.

### **Help**
This tab contains a quick reference guide and a link to this official documentation site.

---

## Saving Settings

The large **"Save and Engage Time Circuits"** button at the bottom of the page saves all changes. It is disabled by default and will only become active when you change a setting on any tab.

Pressing this button sends all configurations to the device, saves them to memory, and triggers the `Time Circuits Lock-In` animation to confirm the new settings have been applied.