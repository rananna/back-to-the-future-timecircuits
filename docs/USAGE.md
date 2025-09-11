# 💡 Usage & Configuration Guide

This guide covers the day-to-day use and configuration of your Time Circuits display via its built-in web interface.

### **Table of Contents**
1. [First-Time WiFi Setup](#first-time-wifi-setup)
2. [Accessing the Web Interface](#accessing-the-web-interface)
3. [Web Interface Tabs](#web-interface-tabs)
4. [The "Engage Time Circuits" Button](#the-engage-time-circuits-button)
5. [Cinematic Save Sequence](#cinematic-save-sequence)

***

## First-Time WiFi Setup

1.  **Connect to the Hotspot**: On the first boot, the ESP32 will create a WiFi network named **BTTF-Clock-Setup**. Connect to this network with your phone or computer.
2.  **Captive Portal**: A captive portal should automatically open in your browser. If it doesn't, navigate to `192.18.4.1`.
3.  **Configure WiFi**: Select your home WiFi network (SSID), enter the password, and click "Save". The device will restart and connect to your network.

***

## Accessing the Web Interface

Once connected, you can access the web UI by navigating to the device's IP address in your browser. You can find the IP address in your router's client list or by monitoring the Serial Monitor in the Arduino IDE during boot.

<p align="center">
  <img src="../images/webui.png" alt="Web UI Screenshot" width="800">
</p>

***

## Web Interface Tabs

The web interface is organized into several tabs for easy configuration.

### Time Circuits Tab
* **Destination Time & Year**: Set the target year for the "Destination Time" display.
* **Last Time Departed & Presets**: Select from a list of famous dates from the movies, or add, update, and delete your own custom presets.

### Temporal Controls Tab
* **Departure/Arrival (Sleep) Times**: Configure a daily schedule to automatically turn the displays off and on to save power.
* **Display & Animation**: Adjust the display brightness, select from a wide range of animation styles, and control the duration and frequency of automatic animations.
* **Interactive Animation Preview**: When you select an animation style from the dropdown, you will now see a live, client-side preview of that animation, allowing you to see what it looks like without having to save your settings and trigger the full sequence on the hardware.
* **Animation Playlists**: Instead of the clock playing the same scheduled animation every time, you can now create a "playlist" of your favorite animation styles. When the "Use Playlist" option is enabled, the clock will cycle through your selected animations for the scheduled events, adding variety and preventing the effects from becoming repetitive.
* **Dynamic Quote Management**: For the "Quote Ticker" animation, you can now add, edit, and delete your own custom quotes, allowing you to personalize the clock with your favorite lines from the movies or your own custom messages.
* **Effects**: Control the "instability" of your time circuits by setting the frequency of random visual glitches and the chance of a major malfunction sequence.
* **Sound**: Adjust the volume of the sound effects and toggle them on or off.

### Data Link Tab
This is where you configure the display rows to show live, real-time data. You can enable either the Live Weather Display, the Data Link Marquee, or the Stock Ticker Mode.

#### Live Weather Display
* Transforms the display rows into a multi-page weather station.
* Simply enter a city name, and the clock will automatically fetch and display current conditions, temperature, wind speed, and more.

#### Data Link Marquee
This is a fully configurable marquee for displaying data from the internet.
* **Data Sources**: Configure up to 5 data points. Each can get its data from a Web API, MQTT Broker, or a Home Assistant Push.
* **API Wizard**: An easy-to-use tool that fetches data from a URL and lets you visually map JSON values to the displays without writing any code.

#### Stock Ticker Mode
This mode transforms the three display rows into a real-time stock and index ticker.
* **Configuration**: You must enter a free **Financial Modeling Prep (FMP) API Key** for this to function.
* **Symbols**: Enter up to three stock or index symbols (e.g., `AAPL`, `GOOGL`, `^GSPC` for the S&P 500) to display.

### System Tab
* **Device Status**: Displays real-time information about the device, including WiFi Signal Strength, free memory, and uptime.
* **Time Synchronization**: Shows whether the device's clock is synchronized with an NTP server. You can manually trigger a sync by clicking **Calibrate Present Time**.
* **UI Theme**: Change the color scheme of the web interface.
* **Device Actions**: Trigger a "Great Scott!" easter egg or reset all settings to their factory defaults.

***

## The "Engage Time Circuits" Button

The large **"Engage Time Circuits"** button at the bottom of the page is your primary way to save settings and trigger the clock's signature animation.

💡 **What it Does:** This button performs two actions at once:
1.  **Saves All Settings:** It saves every configuration option from all tabs.
2.  **Triggers Animation:** It immediately starts the full time travel sequence on the hardware.

Even though it saves all settings, the device is designed for longevity. It intelligently checks which settings have actually been changed and only writes the new values to its memory. This "smart saving" process is extremely fast and minimizes wear on the hardware.

> ⚡ **Tip for Quick Configuration**
> Because every press of the button triggers the full 17-second animation, it's most efficient to **make all of your desired changes across all tabs first**, and then press the save button only once when you are finished.

***

## Cinematic Save Sequence

Pressing the "Engage Time Circuits" button triggers a cinematic 17-second animation on the physical display, complete with synchronized sound effects and feedback in the web interface.

### Web Interface Feedback

When you press the button, you will see the following changes in the web UI:

1.  **Loading State**: The button will be temporarily disabled and show a loading spinner with the text "Saving..." to confirm your action is being processed.
2.  **Save Confirmation**: A message banner will appear at the top of the screen confirming "Settings Saved!".
3.  **Temporal Displacement Effect**: The entire web page will flash for the duration of the hardware animation, mimicking the bright lights of the DeLorean's temporal displacement.

### Hardware Animation and Sound Sequence

This table details the sequence of events on the physical clock.

| Phase | Duration | Visuals | Sound Effect |
| :--- | :--- | :--- | :--- |
| | | | (All sounds are controlled by the **"Time Travel Sound FX"** toggle under the "Temporal Controls" tab) |
| **1. Power Up** | 2 seconds | The displays flicker with random characters, simulating a power surge. | A low hum builds in intensity with the sound of crackling electricity and mechanical relays (**SAVE\_POWER\_UP.mp3**). |
| **2. Acceleration**| 10 seconds | The bottom row displays a speedometer ramping up to 88 MPH while the other rows continue to flicker. | A futuristic engine whine increases in pitch and volume, conveying a sense of immense speed (**SAVE\_ACCELERATION.mp3**). |
| **3. Time Travel**| 4 seconds | At 88 MPH, all displays flash brightly, then show a "time blur" effect where the years rapidly skim forward or backward. | A loud "sonic boom" transitions into a chaotic whoosh of temporal winds (**SAVE\_TIME\_TRAVEL.mp3**). |
| **4. Landing** | 1 second | The displays flicker one last time before settling on the new destination and present times. | A deep "thud" combined with a final crackle of electricity confirms the arrival (**SAVE\_LANDING.mp3**). |