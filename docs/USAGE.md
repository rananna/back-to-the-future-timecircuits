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

This tab is the heart of the time-setting functionality for your display.

#### Destination Time & Year
*   **Time Zone**: Select the time zone for your destination time. This is useful if you want the destination time to be in a different zone from your present time.
*   **YEAR**: Enter the four-digit year you want the DeLorean to travel to. This is the primary input for the "Destination Time" display.

#### Last Time Departed & Presets

This section controls the "Last Time Departed" display and allows you to manage a list of your favorite time-jump destinations.

*   **Famous & Custom Time Jumps**: This dropdown contains all the iconic dates from the *Back to the Future* movies, plus any custom presets you've saved.
    *   Selecting a preset from this list will immediately update the "Last Time Departed" display.
    *   If you select a **custom** preset, its details will populate the form below, allowing you to edit or delete it.

*   **Managing Custom Presets**: You can create your own list of favorite dates.
    *   **To Add a Preset**: Leave the dropdown on "-- Select a Famous Date --". Fill in the "Preset Name," "Date," and "Time" fields, and click **Add to Presets**.
    *   **To Update a Preset**: Select your custom preset from the dropdown. The form will fill with its current details. Make your changes and click **Update Preset**.
    *   **To Delete a Preset**: Select your custom preset from the dropdown and click **Delete Selected Preset**.

*   **Cycle Presets**: To have the clock automatically rotate the "Last Time Departed" display through all available presets (both movie and custom), set the slider to the desired interval in minutes. Setting it to **0** disables this feature.

### Temporal Controls Tab

This tab controls the clock's automatic behaviors, visual effects, and sound.

#### Sleep Schedule
This feature is for energy saving, not for simulating time travel. It allows you to set a daily schedule to automatically turn the displays off and on.
*   **Departure Time**: The time the displays will turn **off**. When this happens, the clock will play `SLEEP_ON.mp3`.
*   **Arrival Time**: The time the displays will turn back **on**. When this happens, the clock will play `CONFIRM_ON.mp3`.

#### Display & Animation
*   **Display Brightness**: Adjust the brightness of all the LED displays.
*   **24 Hour Format**: Toggle the present time display between 12-hour (with AM/PM) and 24-hour format.
*   **Animation Style**: Select from over 20 unique visual effects for the time travel sequence.
*   **Preview Button**: Click this to see a live, client-side preview of the selected animation style in the header clocks. This lets you test out animations without having to save your settings.
*   **Animation Every (min, 0=Off)**: Set an interval in minutes for the clock to automatically play the full time travel sequence. Set to 0 to disable.

#### Sound
*   **Volume**: Adjust the volume of all sound effects.
*   **Time Travel Sound FX**: Toggle the main cinematic sound effects on or off.

### Data Link Tab
This is where you configure the display rows to show live, real-time data from the internet or your smart home. The tab is split into three modes, and **only one can be active at a time**. Enabling one will disable the others.

---

#### Live Weather Display
This mode transforms the "Last Time Departed" display row into a comprehensive, multi-page weather station. An internet connection is required, and the data automatically refreshes periodically.

**Configuration Steps:**
1.  **Enable Weather Mode**: Toggle on "Enable Live Weather".
2.  **Enter City Name**: Type the name of a city you want weather data for (e.g., `Hill Valley`).
3.  **Lookup Coordinates**: Click the **Lookup** button. The clock will use a free geocoding service to find the latitude and longitude for the city. These coordinates will appear in the read-only fields below the button. If the city name is ambiguous, a pop-up will ask you to choose the correct location.
4.  **Fetch Weather**: Once the coordinates are found, the clock will automatically use them to fetch the latest weather data from the free [Open-Meteo API](https://open-meteo.com/).
5.  **Refresh Data**: You can click the **Refresh** button at any time to manually trigger a new weather data fetch using the saved coordinates.

While fetching data, the display will show `WEA TH ER ----`. Once loaded, it will cycle through the following 7 pages of information, with each page scrolling across the display:

1.  **Current Conditions**
    *   Displays the current temperature and a description of the weather (e.g., "Partly Cloudy").
    *   *Example: `CURRENTLY 72.5F, PARTLY CLOUDY`*

2.  **Tomorrow's Forecast**
    *   Shows the predicted high and low temperatures for the following day, along with a description of the expected conditions.
    *   *Example: `TOMORROW HIGH 80F, LOW 65F, CLEAR SKY`*

3.  **Wind & Precipitation**
    *   Details the current wind speed, maximum wind gust for the day, and the probability of precipitation.
    *   *Example: `WIND 10 MPH, MAX 25 MPH, PRECIP 20%`*

4.  **Sunrise & Sunset**
    *   Shows the local sunrise and sunset times, automatically formatted for 12/24 hour time.
    *   *Example: `SUNRISE 630AM, SUNSET 845PM`*

5.  **Hourly Forecast**
    *   Provides a look at the next 3 hours, showing the temperature and expected conditions for each hour.
    *   *Example: `NEXT 3 HRS 71F CLEAR, 70F CLOUDY, 69F RAIN`*

6.  **Feels Like & Humidity**
    *   Displays the apparent ("feels like") temperature and the current relative humidity.
    *   *Example: `FEELS LIKE 78F, HUMIDITY 55%`*

7.  **Today's High & Low**
    *   Shows the forecasted high and low temperatures for the current day.
    *   *Example: `TODAY HIGH 82F, LOW 61F`*

> 💡 **Metric vs. Imperial:** The units used (Celsius/Fahrenheit, KPH/MPH) are automatically determined by the "Use Metric Units" setting in this section.

---

#### Stock Ticker Mode
This mode transforms the three display rows into a real-time stock and index ticker, providing a near-real-time view of market activity. Here’s a detailed look at how it works.

##### 1. Configuration & Activation
First, you need to enable and configure the mode in the "Data Link" tab of the web interface.

*   **Enable the Mode**: Toggle on "Stock Ticker Mode".
*   **API Key**: You must provide a valid API key from the **Financial Modeling Prep** service. Without this key, the device cannot fetch any data.
*   **Stock Symbols**: Enter up to three stock or index symbols (e.g., `AAPL` for Apple Inc. or `^GSPC` for the S&P 500) to be displayed on the three rows.

##### 2. Retrieving Your API Key
The stock data is sourced from a service called [Financial Modeling Prep](https://site.financialmodelingprep.com/developer/docs). You will need to register for a free account to get an API key.

1.  **Navigate to the Registration Page**: Open a web browser and go to the [Financial Modeling Prep registration page](https://site.financialmodelingprep.com/register).
2.  **Sign Up**: Fill out the required information to create a new account.
3.  **Find Your API Key**: Once you have created your account and logged in, navigate to your **Dashboard**. Your API key will be displayed in the **"Your API KEY"** section.
4.  **Copy and Paste**: Copy the API key from the dashboard and paste it into the "API Key" field in the clock's web interface.

All these settings are saved to the device's non-volatile storage, so they persist even after a reboot.

##### 3. Data Fetching
Once the mode is active, the device automatically begins the process of fetching live data.

*   **State Control**: The device prioritizes showing stock data over the default clock or other modes.
*   **Market Hours Check**: The device is programmed to only fetch data when the stock market is likely open (weekdays, 9:30 AM to 4:00 PM Eastern Time).
*   **Asynchronous Fetching**: If the market is open, the device triggers a data refresh every **5 minutes**. To ensure the display remains responsive, it launches separate background tasks to fetch data for each symbol.
*   **Data Formatting**: After receiving data from the API, the code formats it into simple strings that are perfectly sized for the 7-segment displays. For example, the price is condensed to fit within 4 characters, and the percentage change is formatted to two decimal places (e.g., `+1.25`).
*   **Error Handling**: If an API call fails, the device will show a helpful status message on the display (e.g., `NO API KEY`).

##### 4. Display Layout
The fetched data is shown across the three display rows.

*   **Symbol**: The stock symbol is displayed across the "Month" and "Day" segments.
*   **Price**: The current price is displayed in the "Year" segment.
*   **Change**: The day's percentage change is displayed in the "Time" segment.

---

#### Data Link Marquee
This is a fully configurable marquee for displaying custom data from almost any source. It works by cycling through up to 5 "Data Points" on the "Last Time Departed" display row.

##### Global Settings
*   **MQTT Broker Settings**: If you plan to use MQTT or Home Assistant Push as a data source for any data point, you must configure your MQTT broker address, port, and credentials here.
*   **Refresh All Data Every (min)**: Sets a global interval for how often the clock will re-fetch data for all API-based data points.

##### Configuring Data Points
You can configure up to 5 independent data points. Each one has its own set of options:

*   **Data Source**:
    *   **Web API (HTTP)**: Fetch data from any public or private API endpoint.
    *   **MQTT Broker**: Subscribe to an MQTT topic and display the message payload.
    *   **Home Assistant Push**: A special mode for use with the Home Assistant integration, allowing HA to push data directly to a specific display segment.

*   **Display Mode**:
    *   **Four Column Data**: This mode mimics the standard time circuits display, allowing you to map incoming data to the `MONTH`, `DAY`, `YEAR`, and `TIME` segments. This is ideal for structured data.
    *   **Scrolling Text**: This mode displays a single, continuous line of text that scrolls across the entire display row. This is best for long strings or simple messages.

*   **API Wizard & Advanced Settings**: This section provides tools for connecting to nearly any web API.
    *   **API Examples**: This dropdown contains pre-configured URLs for common public APIs (e.g., weather, crypto prices) to help you get started quickly.
    *   **API URL**: The full URL of the API endpoint you want to fetch data from.
    *   **Auth Header Key / Value**: These optional fields allow you to connect to APIs that require authentication. Enter the header name (e.g., `X-API-Key`) and your secret key or token.
    *   **Analyze API (Button)**: This is the easiest way to configure an API data point.
        1.  Enter the URL for your API endpoint (and auth headers, if needed).
        2.  Click **Analyze API**. The clock will fetch the data and display the resulting JSON structure below.
        3.  Click on a form field you want to populate (e.g., the `YEAR` input). The field will be highlighted.
        4.  Click on a value from the JSON results. The tool will automatically generate the correct JSON path (e.g., `results[0].user.name`) and map it to the highlighted field.

*   **Formatting & Display**:
    *   **Prefix/Suffix**: Add static text before or after the values in the `YEAR` and `TIME` fields (or the main `Scrolling Text` field).
    *   **Icon**: In "Four Column Data" mode, you can replace the `DAY` segment with a pre-defined icon (e.g., Sun, Cloud, Heart). This is useful for at-a-glance information.

*   **Scroll Speed**: Controls how fast the text scrolls in the `YEAR` and `TIME` fields (if the text is too long) or the main `Scrolling Text` field.

### Network & System Tab
This tab provides information about the device's status and allows you to perform system-level actions.

*   **Present Time**: Configure the time zone for the "Present Time" display and manually trigger a synchronization with an NTP time server.
*   **System Status**: Displays real-time information about the device, including WiFi Signal Strength, free memory, and uptime.
*   **Firmware & UI Updates**: These forms allow you to update the device's software over-the-air (OTA).
    *   **Firmware Update (OTA)**: Upload a new firmware (`.bin`) file to update the main controller software.
    *   **UI File Update**: Upload new web interface files (`.html`, `.css`, `.js`) to update the look or functionality of this web UI.
*   **UI Theme**: Change the color scheme of this web interface. This setting is cosmetic and does not affect the physical display. Available themes include:
    *   **Time Circuits**: The classic green and yellow.
    *   **OUTATIME**: A red and orange "warning" theme.
    *   **88 MPH**: A cool blue and cyan theme.
    *   **Plutonium Glow**: A radioactive green and yellow.
    *   **Mr. Fusion**: An orange and white theme.
    *   **Clock Tower**: A vintage brown and beige theme.
*   **Device Actions**:
    *   **Great Scott!**: Trigger a fun easter egg sound effect.
    *   **Reset All Settings**: Reset all configuration options on all tabs to their factory defaults. **Use with caution!**

***

## Advanced Control via MQTT & API

Beyond the web interface, many of the clock's features can be controlled programmatically via MQTT or direct API calls, making it highly extensible and easy to integrate into a smart home environment. For a complete list of all available MQTT topics and API endpoints, see the [Home Assistant Guide](HOME_ASSISTANT.md).

### The Sequencer

The Sequencer is a powerful feature that allows you to create custom, scripted animations. You can define a series of steps that will be executed in order, allowing you to build complex visual and audio displays.

**How to Use It:**
The Sequencer is controlled by sending a single string to the following MQTT topic:
`bttf-time-circuits/[DEVICE_ID]/run_sequence/command`

The string must be a series of commands separated by semicolons.

**Command Syntax:**
`command(arg1,arg2,...);command2(arg1);...`

**Available Commands:**

*   `text(target, value)`: Displays a string on a specific segment of the display.
    *   `target`: The display segment to write to. Examples: `dest_year`, `pres_day`, `last_time`.
    *   `value`: The text to display.
*   `flash(target, duration)`: Makes a specific display segment flash for a duration.
    *   `target`: The display segment to flash.
    *   `duration`: The duration of the flash effect in milliseconds.
*   `sound(filename)`: Plays a sound effect from the device's memory.
    *   `filename`: The name of the sound file (e.g., `arrival_chime`, `flux_capacitor_power_on`). Do not include the `.mp3` extension.
*   `wait(duration)`: Pauses the sequence for a specific amount of time.
    *   `duration`: The time to wait in milliseconds.

**Example Sequence:**
The following string would display "HELLO" on the destination year, wait half a second, play a beep, and then display "WORLD" on the present year.

`text(dest_year,HELLO);wait(500);sound(sys_beep);text(pres_year,WORLD)`

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