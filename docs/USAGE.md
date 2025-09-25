# 💡 Usage & Configuration Guide

This guide covers the day-to-day use and configuration of your Time Circuits display via its built-in web interface.

### **Table of Contents**
1. [First-Time WiFi Setup](#first-time-wifi-setup)
2. [Accessing the Web Interface](#accessing-the-web-interface)
3. [Web Interface Overview](#web-interface-overview)
4. [Advanced Features](#advanced-features)
5. [Saving Settings](#saving-settings)

***

## First-Time WiFi Setup

1.  **Connect to the Hotspot**: On its first boot, the ESP32 will create a WiFi network named **`BTTF-Clock-Setup`**. Connect to this network with your phone or computer.
2.  **Captive Portal**: A captive portal should automatically open in your browser. If it doesn't, open a browser and navigate to `192.168.4.1`.
3.  **Configure WiFi**: In the portal, select your home WiFi network (SSID), enter your password, and click "Save". The device will restart and connect to your network.

***

## Accessing the Web Interface

Once the clock is connected to your network, you can access its web interface in two ways:
1.  **Easy Way (mDNS)**: Simply navigate to **`http://bttf-clock.local`** in your browser. This works on most networks without any special configuration.
2.  **IP Address**: If the mDNS address doesn't work, you'll need the clock's IP address. You can find this in your router's client list or by monitoring the Serial Monitor in the Arduino IDE during boot.

![Web UI Screenshot](../images/webui.png)

***

## Web Interface Overview

The web interface is organized into several tabs for easy configuration.

### Time Circuits Tab
This is the heart of the time-setting functionality.
*   **Destination Time & Year**: Set the destination time and year for the top display.
*   **Last Time Departed & Presets**: Manage the "Last Time Departed" display and a list of your favorite time-jump destinations from the movies or your own custom entries.

### Temporal Controls Tab
This tab controls the clock's automatic behaviors, visual effects, and sound.
*   **Sleep Schedule**: Set a daily schedule to automatically turn the displays off and on to save energy.
*   **Display & Animation**: Adjust brightness, toggle 24-hour format, and choose from over 20 unique time travel animation styles.
*   **Sound**: Control the volume of the sound effects.

### Data Link Tab
This tab unlocks the clock's advanced data display capabilities, allowing it to show real-time information on the bottom display row.
*   **For detailed instructions** on configuring the Weather, Stock Ticker, and custom Data Link Marquee modes, please see the **[📈 Data Link, Weather & Stock Ticker Guide](guides/DATA_LINK.md)**.

### Network & System Tab
This tab provides information about the device's status and allows you to perform system-level actions.
*   **System Status**: View WiFi signal strength, free memory, and uptime.
*   **Firmware & UI Updates**: Update the device's software and web interface files over-the-air (OTA).
*   **Device Actions**: Reboot the device or reset all settings to factory defaults.

***

## Advanced Features

Beyond the web interface, many of the clock's features can be controlled programmatically.
*   **For Home Assistant Users**: To unlock deep integration with your smart home, see the **[🏠 Home Assistant Integration Guide](HOME_ASSISTANT.md)**.
*   **For Developers**: For a technical deep dive into the firmware and API, see the **[🔬 Developer's Guide](DEVELOPMENT.md)**.

***

## Saving Settings

The large **"Engage Time Circuits"** button at the bottom of the page is the main "save" button for the entire interface.

💡 **How it Works:** Pressing this button sends all configuration options from all tabs to the device. The clock saves the settings to its internal memory and then triggers the full time-travel animation sequence to confirm that the new settings have been applied.

> ⚡ **Tip for Quick Configuration**
> It's most efficient to make all your desired changes across all tabs *first*, and then press the "Engage Time Circuits" button only once when you're finished.