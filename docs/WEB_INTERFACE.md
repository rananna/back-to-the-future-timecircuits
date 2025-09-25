# 🌐 Web Interface Guide

This guide provides a detailed, field-by-field explanation of every setting available in the web interface. For a general overview, see the **[Usage Guide](USAGE.md)**.

### **Table of Contents**
1. [Time Circuits Tab](#time-circuits-tab)
2. [Temporal Controls Tab](#temporal-controls-tab)
3. [Data Link Tab](#data-link-tab)
4. [Network & System Tab](#network--system-tab)
5. [The "Engage" Button](#the-engage-button)

---

## Time Circuits Tab
This tab contains the core settings for the three time displays.

![Web UI Time Circuits Tab](../images/webui_time_circuits.png)

#### Destination Time
*   **Month / Day / Year / Time**: Sets the date and time shown on the top "Destination Time" display row.
*   **Time Zone**: Sets the time zone for the Destination Time display. This is useful for representing a time in a different part of a world (e.g., setting a time for a friend in another country).

#### Present Time
*   **Time Zone**: Sets the time zone for the middle "Present Time" display. This should be set to your local time zone. The clock uses this setting, along with NTP synchronization, to display the correct current time.

#### Last Time Departed
*   **Month / Day / Year / Time**: Manually sets the date and time shown on the bottom "Last Time Departed" display row.
*   **Presets**: This dropdown provides a list of famous dates from the *Back to the Future* movies. Selecting a preset will automatically fill in the date and time fields above.
    *   **Add/Remove Custom Presets**: You can add your own favorite dates to this list.
        *   To **add** a preset, first set the desired date and time using the manual input fields, then enter a descriptive name in the text box and click **"Add"**.
        *   To **remove** a preset, select it from the dropdown and click **"Remove"**.
*   **Cycle Presets Every (Minutes)**: When set to a value greater than 0, the "Last Time Departed" display will automatically cycle through all available presets (both movie and custom) at the specified interval. Set to `0` to disable this feature.

---

## Temporal Controls Tab
This tab controls the clock's automatic behaviors, visual effects, and sound.

![Web UI Temporal Controls Tab](../images/webui_temporal_controls.png)

#### Sleep Schedule
*   **Sleep At / Wake At**: Sets a daily schedule to automatically turn the displays off and on. This is useful for saving energy or preventing the clock from being too bright at night. The displays will turn off at the "Sleep At" time and turn back on at the "Wake At" time.

#### Display & Animation
*   **Brightness**: A slider to control the brightness of all 14-segment displays.
*   **Use 24-Hour Format**: Toggles the time display format between 12-hour (with AM/PM LEDs) and 24-hour.
*   **Time Travel Every (Minutes)**: When set to a value greater than 0, the clock will automatically trigger the full time travel animation sequence at the specified interval. This is a fun way to have the clock "glitch" through time periodically. Set to `0` to disable.
*   **Animation Style**: Choose from over 20 unique animation styles for the time travel sequence. This includes everything from a simple flicker to a full "temporal displacement" effect.
*   **Animation Duration (ms)**: Sets the length of the time travel animation, in milliseconds.

#### Sound
*   **Enable Time Travel Sound**: Toggles the iconic DeLorean sound effects during the time travel animation sequence.
*   **Notification Volume**: A slider to control the volume of all sound effects.

---

## Data Link Tab
This tab unlocks the clock's advanced data display capabilities, allowing it to show real-time information on the bottom display row.

> **Note**: The Weather, Stock, and Data Link Marquee modes are mutually exclusive. Enabling one will automatically disable the others.

For a more in-depth explanation of these features, see the **[📈 Data Link, Weather & Stock Ticker Guide](guides/DATA_LINK.md)**.

![Web UI Data Link Tab](../images/webui_data_link.png)

#### Live Weather
*   **Enable Live Weather**: Turns on the weather display mode.
*   **City Name & Lookup**: Enter a city name and click "Lookup" to fetch its geographic coordinates.
*   **Use Metric Units**: Toggles the display between Fahrenheit/MPH and Celsius/KPH.

#### Stock Ticker
*   **Enable Stock Ticker Mode**: Turns on the stock ticker display mode.
*   **Financial Modeling Prep API Key**: A **required** API key from the Financial Modeling Prep service.
*   **Refresh Interval (Minutes)**: How often to fetch updated stock data.
*   **Add/Manage Assets**: A list of stock or crypto symbols to track. You can add, remove, and reorder assets in this list.

#### Data Link Marquee
*   **Enable Data Link Marquee**: Turns on the most flexible data display mode.
*   **Global MQTT Broker Settings**: Configure the connection details for your MQTT broker if you plan to use MQTT-based data points.
*   **Data Points (1-5)**: Configure up to 5 independent "pages" of data to be cycled on the display. Each data point can be sourced from:
    *   **Static Text**: A fixed message.
    *   **MQTT Push**: The value from a specific MQTT topic.
    *   **Home Assistant Push**: Data sent directly from Home Assistant.

---

## Network & System Tab
This tab provides information about the device's status and allows you to perform system-level actions.

![Web UI System Tab](../images/webui_system.png)

#### System Status
*   This section displays useful diagnostic information, including the clock's current IP address, WiFi signal strength (RSSI), free memory, and uptime.

#### Firmware Update (OTA)
*   **Upload Firmware (.bin file)**: This allows you to update the clock's main firmware without connecting it to a computer. You must first compile the code in the Arduino IDE and export the binary file (`.bin`).
    > **Security Note**: This feature is protected by a password. The default password is `1.21gigawatts`. See the **[Updating Guide](UPDATING.md#changing-the-ota-password)** for instructions on how to change it.

#### UI File Update
*   **Upload UI or Sound Files**: This tool allows you to update the files that make up the web interface (HTML, CSS, JS) or the sound effects (`.mp3`). This is useful for making UI tweaks or adding new sounds without re-flashing the entire device.

#### Device Actions
*   **Reboot Device**: Restarts the ESP32.
*   **Reset to Factory Defaults**: Erases **all** settings stored on the device, including WiFi credentials, time zone configurations, and API keys. The device will restart and enter setup mode, creating the `BTTF-Clock-Setup` hotspot.

---

## The "Engage" Button
The large **"Engage Time Circuits"** button at the bottom of the page is the main "save" button for the entire interface.

💡 **How it Works:** Pressing this button sends all configuration options from all tabs to the device. The clock saves the settings to its internal memory and then triggers the full time-travel animation sequence to confirm that the new settings have been applied.

> ⚡ **Tip for Quick Configuration**
> It's most efficient to make all your desired changes across all tabs *first*, and then press the "Engage Time Circuits" button only once when you're finished.