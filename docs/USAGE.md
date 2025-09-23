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

![Web UI Screenshot](../images/webui.png)

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
This mode transforms the bottom display row into a scrolling, multi-page financial ticker. It supports stocks, ETFs, and cryptocurrencies from around the world, allowing you to track your portfolio at a glance.

##### 1. Activation & API Key
First, you need to enable and configure the mode in the "Data Link" tab of the web interface.

*   **Enable the Mode**: Toggle on "Stock Ticker Mode". This will reveal the settings panel.
*   **API Key**: You must provide a valid API key from the **Financial Modeling Prep** service. Without this key, the device cannot fetch any data. A free tier is available and is sufficient for this feature.
    > ⚠️ **Security Note:** Your API key is a secret credential. Treat it like a password and do not share it publicly.
*   **Refresh Interval**: Set how often the data should be refreshed, in minutes. The default is 20 minutes. Note that the free API tier has a daily call limit, so a very short interval may exhaust your quota quickly.

To get your API key:
1.  **Navigate to the Registration Page**: Open a web browser and go to the [Financial Modeling Prep registration page](https://site.financialmodelingprep.com/register).
2.  **Sign Up**: Fill out the required information to create a new account.
3.  **Find Your API Key**: Once you have created your account and logged in, navigate to your **Dashboard**. Your API key will be displayed in the **"Your API KEY"** section.
4.  **Copy and Paste**: Copy the API key from the dashboard and paste it into the "Financial Modeling Prep API Key" field in the clock's web interface.

##### 2. Adding & Managing Assets
This section allows you to build and manage your list of tracked assets.

*   **Add an Asset**:
    1.  **Symbol Validation**: When you enter a stock, ETF, or crypto symbol (e.g., `AAPL`, `SPY`, `BTCUSD`) and click "Add Asset," the clock first performs a validation check. It contacts the API to verify the symbol is valid and to retrieve its exchange information. If the symbol cannot be found, it will not be added.
    2.  **Immediate Fetch**: Upon successful validation, the asset is added to the "Tracked Assets" list, and the clock immediately triggers a background fetch to retrieve its price data. This ensures the asset's information appears on the display and in the UI almost instantly.

*   **Manage Assets**:
    *   **Reorder**: Click and drag any asset in the list to change the order in which they are displayed on the clock.
    *   **Remove**: Click the red **'×'** button next to an asset to remove it from your list.
    *   **Saving Changes**: All changes to the asset list (adding, removing, reordering) are saved automatically when you press the main **"Engage Time Circuits"** button at the bottom of the page.

##### 3. The Display
The physical display provides a rich, multi-page view of your assets.

*   **Display Cycle**: The clock automatically cycles through each of your tracked assets. For each asset, it displays **two pages** of information:
    1.  **Page 1: Price & Change**: Shows the asset's symbol, current price, and percentage change for the day.
        *   *Example: `AAPL $175.30 +1.23%`*
    2.  **Page 2: High, Low & Volume**: Shows the asset's symbol along with the highest and lowest price for the current trading day and the trading volume. Volume is automatically abbreviated (K for thousands, M for millions, B for billions).
        *   *Example: `AAPL HI $176.10 LO $173.80 VOL 52.5M`*

*   **Currency Symbols**: The clock automatically converts currency codes (e.g., `USD`, `EUR`, `GBP`) into their common symbols (`$`, `€`, `£`) on the display.

*   **Market Closed Behavior**: When the markets for your tracked stock/ETF assets are closed, the clock will not fetch new data. It will continue to display the last available data until the market re-opens. This does not apply to cryptocurrencies, which trade 24/7.

*   **Error Messages**: If the clock encounters a problem, it will display a specific error message on the marquee to help you diagnose the issue. Common errors include:
    *   `[SYMBOL] INVALID SYMBOL`: The ticker symbol could not be found or is not supported.
    *   `[SYMBOL] INVALID API KEY`: Your API key is incorrect, has expired, or has been disabled.
    *   `[SYMBOL] RATE LIMITED`: You have exceeded your daily API call limit. The system will automatically try again later.
    *   `[SYMBOL] CONNECTION FAILED`: The clock was unable to reach the API server. This is often a temporary network issue.
    *   `[SYMBOL] PENDING`: The asset has been added but the first data fetch is still in progress.

##### 4. Web UI Live Feedback
The web interface provides several tools for monitoring the stock ticker in real-time.

*   **Live Marquee Preview**: A preview of the text currently scrolling on the physical display is shown directly in the web UI.
*   **Tracked Assets List**: This list provides live updates for your assets. You can see the current price and percentage change, which refresh periodically. If there's an error with an asset, it will be shown here.
*   **API Usage Counter**: The UI displays the number of API calls made for the current day. This counter automatically resets to zero at midnight (based on your clock's time zone).

##### 5. How It Works: Data Fetching & Reliability
The stock ticker has several smart features to ensure data is both timely and efficient.

*   **Market Hours**: The system uses a general-purpose check for North American market hours (**9:30 AM to 4:00 PM Eastern Time, Mon-Fri**) to decide when to fetch data for stocks and ETFs. Data is not fetched outside of these hours to conserve API calls.
    *   **Cryptocurrencies**, which trade continuously, are fetched 24/7.
*   **Individual Asset Fetching**: To improve reliability, the clock fetches data for each asset in your list with a separate API call. This prevents a single invalid symbol from causing the entire update to fail.
*   **Automatic Retries**: If an API call for an asset fails due to a temporary issue (like a network error or rate limiting), the system will automatically retry the request up to two more times before marking it as failed.

##### 6. MQTT Control
You can manually cycle through the asset pages using MQTT commands. This is useful for quickly checking a specific data point without waiting for the automatic cycle.
*   **Next Page**: Publish any message to `bttf-time-circuits/[DEVICE_ID]/stock/next/command`
*   **Previous Page**: Publish any message to `bttf-time-circuits/[DEVICE_ID]/stock/previous/command`

##### 7. Limitations & Tracking Indices
*   **Free API Plan**: The free tier of the Financial Modeling Prep API is powerful but has limitations. Most importantly, it **does not support direct tracking of major market indices** like the S&P 500 (`^GSPC`) or the NASDAQ Composite (`^IXIC`). Attempting to add these symbols will result in an `INVALID SYMBOL` error.

*   **Using ETFs as a Proxy**: A great way to track these indices is by using **Exchange-Traded Funds (ETFs)**. These are funds that trade on stock exchanges, just like regular stocks, and are designed to mirror the performance of a specific index. Since they have regular ticker symbols, the clock can track them easily.

Here are some popular ETFs for major North American indices that you can use:

| Index | ETF Ticker | Description |
| :--- | :--- | :--- |
| **S&P 500** | `SPY` | Tracks the 500 largest U.S. publicly traded companies. |
| **Nasdaq-100**| `QQQ` | Tracks the 100 largest non-financial companies on the Nasdaq exchange. |
| **Dow Jones** | `DIA` | Tracks the 30 large, publicly-owned companies in the Dow Jones Industrial Average. |
| **Russell 2000**| `IWM` | Tracks an index of 2,000 small-cap U.S. companies. |
| **S&P/TSX 60**| `XIU.TO` | Tracks the 60 largest companies on the Toronto Stock Exchange (Canada). |

---

#### Data Link Marquee
This is a fully configurable marquee for displaying custom data from almost any source. It works by cycling through up to 5 "Data Points" on the "Last Time Departed" display row.

##### Global Settings
*   **MQTT Broker Settings**: If you plan to use MQTT or Home Assistant Push as a data source for any data point, you must configure your MQTT broker address, port, and credentials here.
*   **Refresh All Data Every (min)**: Sets a global interval for how often the clock will re-fetch data for all data points.

##### Configuring Data Points
You can configure up to 5 independent data points. Each one has its own set of options:

*   **Data Source**:
    *   **MQTT Broker**: Subscribe to an MQTT topic and display the message payload.
    *   **Home Assistant Push**: A special mode for use with the Home Assistant integration, allowing HA to push data directly to a specific display segment.
    *   **Static Text**: Display a fixed string of text.

*   **Prefix/Suffix Text**: When using the "MQTT Broker" data source, you can add static text that will appear before (prefix) and after (suffix) the text received from the MQTT topic. This is useful for adding labels or units to your data.
    *   *Example*: If your MQTT topic sends the number `23.5`, you could set the prefix to `TEMP:` and the suffix to `C` to display `TEMP: 23.5 C`.

##### Controlling a Data Point via MQTT
To send data to a specific data point, you need to configure the data point in the web UI to use the "MQTT Broker" data source and specify a unique MQTT topic for it.

*   **Example**:
    1.  In the "Data Link" tab, select "Data Link Marquee".
    2.  For "Data Point 1", set the "Data Source" to "MQTT Broker".
    3.  In the "MQTT Topic" field for Data Point 1, enter a topic like `timecircuits/bttf-clock-123456/datapoint/1`.
    4.  Save the settings.
    5.  Now, you can publish a message to the topic `timecircuits/bttf-clock-123456/datapoint/1` from any MQTT client, and the payload of the message will be displayed on the marquee.

*   **Display Mode**:
    *   **Scrolling Text**: This mode displays a single, continuous line of text that scrolls across the entire display row. This is best for long strings or simple messages.

*   **Formatting & Display**:
    *   **Prefix/Suffix**: Add static text before or after the main scrolling text.
    *   **Scroll Speed**: Controls how fast the text scrolls.

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

💡 **What it Does:** This button triggers a smart, asynchronous save-and-animate sequence:
1.  **Requests a Save:** It sends all configuration options from all tabs to the device.
2.  **Saves in Background:** The device saves the settings in the background. It intelligently checks which settings have actually been changed and only writes the new values to its memory to minimize wear on the hardware.
3.  **Triggers Animation on Success:** Once the save is successfully completed, the device immediately starts the full time travel sequence.

This "smart saving" process is extremely fast and reliable, ensuring that an animation only confirms a successful save.

> ⚡ **Tip for Quick Configuration**
> Because every press of the button triggers the full 17-second animation, it's most efficient to **make all of your desired changes across all tabs first**, and then press the save button only once when you are finished.

***

## Cinematic Save Sequence

Pressing the "Engage Time Circuits" button triggers a cinematic animation on the physical display, complete with synchronized sound effects and feedback in the web interface.

### Web Interface Feedback

When you press the button, you will see the following changes in the web UI:

1.  **Loading State**: The button will be temporarily disabled and show a loading spinner with the text "Saving..." to confirm your action is being processed.
2.  **Save Confirmation**: A message banner will appear at the top of the screen confirming "Settings Saved!".
3.  **Temporal Displacement Effect**: The entire web page will flash for the duration of the hardware animation, mimicking the bright lights of the DeLorean's temporal displacement.

### Hardware Animation and Sound Sequence

This table details the sequence of events on the physical clock.

> (All sounds are controlled by the **"Time Travel Sound FX"** toggle under the "Temporal Controls" tab)

| Phase | Duration | Visuals | Sound Effect |
| :--- | :--- | :--- | :--- |
| **1. Power Up** | 1 second | The displays begin to flicker and glitch, simulating a power surge. | The sound of crackling electricity and power surges (**electric_sparks.mp3**). |
| **2. Animation**| 10 seconds | The displays perform the animation selected in the "Animation Style" dropdown. This can range from a simple flicker to complex patterns. | The initial sound effect continues to play. |
| **3. Cool Down**| 1 second | The displays flicker one last time before settling on the new destination and present times. | The sound fades out. |