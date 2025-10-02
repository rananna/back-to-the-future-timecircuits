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
On its first boot, the ESP32 is unable to connect to your local WiFi network, so it will create its own WiFi hotspot to allow you to configure it.

1.  **Connect to the "BTTF-Clock-Setup" Hotspot**
    *   Using your phone or computer, scan for new WiFi networks.
    *   Connect to the network with the SSID: **`BTTF-Clock-Setup`**.

2.  **Open the Captive Portal**
    *   Once connected, a "captive portal" page should automatically open in your web browser.
    *   If the page does not open automatically, manually navigate to `http://192.168.4.1`.

3.  **Configure and Save**
    *   From the portal, click the **"Configure WiFi"** button.
    *   Select your home WiFi network from the list of available networks.
    *   Enter your WiFi password and click **"Save"**.
    *   The device will restart and automatically connect to your home network.

***

## Accessing the Web Interface

Once the clock is connected to your network, you can access its web interface in two ways:
1.  **Easy Way (mDNS)**: Simply navigate to **`http://bttf-clock.local`** in your browser. This works on most networks without any special configuration.
2.  **IP Address**: If the mDNS address doesn't work, you'll need the clock's IP address. You can find this in your router's client list or by monitoring the Serial Monitor in the Arduino IDE during boot.

***

## Web Interface Overview

The web interface is organized into several tabs for easy configuration.

### Time Circuits Tab
This is the heart of the time-setting functionality.

*   **Destination Time & Year**: Set the destination time and year for the top display.
*   **Last Time Departed & Presets**: This section controls the bottom display row, which shows the "Last Time Departed." You can manually set a date and time, or select from a list of presets.
    *   **Movie Presets**: The dropdown list is pre-populated with iconic dates from the *Back to the Future* movies. Selecting one will instantly load its date and time.
    *   **Custom Presets**: You can save your own favorite dates (like birthdays or anniversaries) to this list. To add a new custom preset:
        1.  Manually enter the desired date and time in the "Last Time Departed" fields.
        2.  Enter a descriptive name for your preset in the text box below the dropdown.
        3.  Click the "Add" button. Your custom preset will now appear in the dropdown list for easy recall.
    *   **Deleting Presets**: To remove a custom preset, simply select it from the dropdown and click the "Delete" button. Movie presets cannot be deleted.

### Temporal Controls Tab
This tab controls the clock's automatic behaviors, visual effects, and sound.
*   **Sleep Schedule**: Set a daily schedule to automatically turn the displays off and on. When the clock enters sleep mode, it will play a "power down" sound, and when it wakes up, it will play a "power up" sound. This is useful for saving energy and preventing the bright lights from disturbing you at night.
*   **Automatic Preset Cycling**: This feature automatically cycles the "Last Time Departed" display through your list of movie and custom presets.
    *   **Cycle Interval**: Set the number of minutes the clock should wait before switching to the next preset. Setting this to `0` disables the feature.
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
*   **For Home Assistant Users**: To unlock deep integration with your smart home, see the **[🏠 Home Assistant Integration Guide](home-assistant.md)**.
*   **For Developers**: For a technical deep dive into the firmware and API, see the **[🔬 Developer's Guide](DEVELOPMENT.md)**.
*   **For Advanced Automations**: The clock includes a powerful command sequencer for creating custom, multi-step animations with sounds and text. For a full list of pre-defined sequences (like "Intruder Alert" and "Party Mode") and instructions on how to build your own, see the `run_sequence` service documentation in the **[🏠 Home Assistant Integration Guide](home-assistant.md)**.

***

## Saving Settings

The large **"Engage Time Circuits"** button at the bottom of the page is the main "save" button for the entire interface.

💡 **How it Works:** Pressing this button sends all configuration options from all tabs to the device. The clock saves the settings to its internal memory and then triggers the full time-travel animation sequence to confirm that the new settings have been applied.

> ⚡ **Tip for Quick Configuration**
> It's most efficient to make all your desired changes across all tabs *first*, and then press the "Engage Time Circuits" button only once when you're finished.