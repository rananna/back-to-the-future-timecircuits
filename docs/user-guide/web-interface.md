# 💡 Web Interface Guide

This guide covers the use and configuration of your Time Circuits display via its built-in web interface.

### **Table of Contents**
1. [First-Time WiFi Setup](#-first-time-wifi-setup)
2. [Accessing the Web Interface](#-accessing-the-web-interface)
3. [Web Interface Tabs](#-web-interface-tabs)
4. [Saving Settings](#-saving-settings)

---

## 🌐 First-Time WiFi Setup
On its first boot, the device cannot connect to your local WiFi network, so it will create its own WiFi hotspot to allow you to configure it.

1.  **Connect to the Hotspot**: Using your phone or computer, scan for new WiFi networks and connect to the one named **`TimeCircuits-Setup`**.
2.  **Open Captive Portal**: A "captive portal" page should automatically open. If it doesn't, manually navigate to `http://192.168.4.1` in your browser.
3.  **Configure and Save**: Click **"Configure WiFi,"** select your home network, enter your password, and click **"Save"**. The device will restart and connect to your network.

---

## 🖥️ Accessing the Web Interface

Once connected to your network, you can access the web UI in two ways:
1.  **Easy Way (mDNS)**: Navigate to **`http://BTTF_TC.local`** in your browser.
2.  **IP Address**: If mDNS doesn't work, find the clock's IP address in your router's client list or via the Arduino IDE's Serial Monitor during boot.

---

## 🎛️ Web Interface Tabs

The web interface is organized into five tabs.

### **Time Circuits Tab**
This is the heart of the time-setting functionality.
*   **Destination Time & Year**: Set the destination year and time zone for the top display row.
*   **Last Time Departed & Presets**: This section controls the "Last Time Departed" display (bottom row).
    *   **Movie Presets**: The dropdown is pre-populated with iconic dates from the movies.
    *   **Custom Presets**: You can add, edit, or delete your own favorite dates (like birthdays or anniversaries) using the form.
    *   **Automatic Cycling**: Use the slider to set an interval (in minutes) for the clock to automatically cycle through the presets. `0` disables this feature.

### **Temporal Controls Tab**
This tab controls the clock's automatic behaviors, visual effects, and sound.
*   **Departure/Arrival (Sleep) Times**: Set a daily schedule to automatically turn the displays off (depart) and on (arrive). This is useful for saving energy and preventing the bright lights from disturbing you at night.
*   **Display & Animation**: Adjust brightness and toggle 24-hour format.
*   **Animation Sequences**: Select a pre-programmed, multi-track animation from the dropdown and click **"Run"** to play it instantly.
*   **Sound**: Control the master volume and enable or disable the main time travel sound effects.
*   **Internet Radio**: Play internet radio streams directly on the device. You can add, edit, and delete stations from your playlist.

### **Data Link Tab**
This tab unlocks advanced data display capabilities.
*   **Stock Market Ticker**: Enable this mode to show real-time stock prices. Requires a free API key from [Financial Modeling Prep](https://site.financialmodelingprep.com/developer/docs).
*   **Live Weather Display**: Enable this mode to show the current weather for a specified city.
*   **Data Link Configuration**: For advanced users, this allows the clock to display data pushed from an MQTT broker.

### **Network & System Tab**
This tab provides device status and system-level actions.
*   **Present Time**: Set the time zone for the "Present Time" display and manually sync with an NTP server.
*   **System Status**: View WiFi signal strength, free memory, and uptime.
*   **Firmware Update (OTA)**: Update the device's main software over the air.
*   **UI Theme**: Customize the look and feel of this web interface.
*   **Device Actions**: Trigger a "Great Scott!" animation or reset all settings to their factory defaults.

### **Help Tab**
This tab contains a quick reference guide built directly into the UI, explaining the features of each tab and providing a list of all available animation sequences and their descriptions.

---

##💾 Saving Settings

The large **"Save and Engage Time Circuits"** button at the bottom of the page is the main "save" button for the entire interface. It is disabled by default and will only become active when you make a change to a setting.

💡 **How it Works:** Pressing this button sends all configuration options from all tabs to the device. The clock saves the settings to its internal memory and then triggers your selected animation sequence to confirm that the new settings have been applied.

> ⚡ **Tip for Quick Configuration**
> It's most efficient to make all your desired changes across all tabs *first*, and then press the "Save and Engage Time Circuits" button only once when you're finished.