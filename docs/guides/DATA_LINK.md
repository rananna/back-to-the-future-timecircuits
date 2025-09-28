# 📈 Data Link, Weather & Stock Ticker Guide

This guide covers the advanced data display features of your Time Circuits clock, which can show live, real-time data from the internet or your smart home.

---

### **Target Display Row**

Before enabling a data mode, you can choose which of the three main display rows will be used to show the data-linked content.

*   **Location**: This setting is located at the top of the **Data Link** tab in the web interface.
*   **Options**: You can select `Top`, `Middle`, or `Bottom`.
*   **Behavior**: When a data mode (Weather, Stock Ticker, or Data Link Marquee) is active, it will take over the selected row. The other two rows will continue to show their normal time information. For example, if you set the target row to "Top," the weather forecast will appear there, while the "Present Time" and "Last Time Departed" displays will function as usual.

The **Data Link** tab in the web interface is split into three powerful modes, and **only one can be active at a time**. Enabling one will automatically disable the others.

### **Table of Contents**
1. [Live Weather Display](#live-weather-display)
2. [Stock Ticker Mode](#stock-ticker-mode)
3. [Data Link Marquee](#data-link-marquee)

---

## Live Weather Display
This mode transforms the bottom display row into a comprehensive, multi-page weather station. An internet connection is required, and the data automatically refreshes periodically.

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

## Stock Ticker Mode
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

## Data Link Marquee
This is the most powerful and flexible data display mode. It transforms the bottom display row into a fully configurable marquee that can display custom data from multiple sources like MQTT, Home Assistant, or just static text. It works by cycling through up to 5 independent "Data Points," each with its own source and formatting.

##### 1. Activating the Marquee
To begin, you must first enable the Data Link Marquee mode.
1.  Navigate to the **Data Link** tab.
2.  Toggle on the **"Enable Data Link Marquee"** switch.
This will disable the Weather and Stock Ticker modes and reveal the marquee configuration panel.

##### 2. Global Settings
These settings apply to all data points that use MQTT.
*   **Global MQTT Broker Settings**: If you plan to use `MQTT Push` or `Home Assistant Push` as a data source for any data point, you **must** configure your MQTT broker's address, port, and credentials here. If you only use the `Static Text` data source, these settings can be left blank.

##### 3. Configuring Data Points
This is where you define what data to show on the display.

*   **Number of Data Points**: Use this slider to select how many data points you want to display, from 1 to 5. The clock will cycle through them in order. For each number you select, a new configuration block will appear below.

*   **Data Point Header**: Each data point has a header with its title (e.g., "Data Point 1") and two helper buttons:
    *   **Clear**: Resets all fields for that data point to their default values.
    *   **Duplicate**: Copies the configuration of the current data point to a new data point at the end of the list. This is useful for creating several similar data points without re-entering all the settings.

*   **Data Source**: This dropdown determines where the data for this point comes from.
    *   **`MQTT Push`**: The most common option. The clock will subscribe to the MQTT topic you specify and display the payload of any message it receives. This is perfect for showing real-time data from sensors or other smart home devices.
        *   When you select this, the **MQTT Topic**, **Prefix Text**, and **Suffix Text** fields will become visible.
    *   **`Home Assistant Push`**: A special mode for seamless integration with Home Assistant. In this mode, Home Assistant can directly push data to the display without needing a specific MQTT topic configured on the clock.
    *   **`Static Text`**: The simplest option. The clock will display a fixed string of text that you enter. This is great for reminders, labels, or decorative messages.
        *   When you select this, the **Scrolling Text** field becomes visible.

*   **Configuration Fields**:
    *   **MQTT Topic**: (Visible for `MQTT Push` only) Enter the full MQTT topic the clock should listen to for this data point's value.
    *   **Prefix Text**: (Visible for `MQTT Push` only) Static text that will always be displayed *before* the value received from MQTT. Useful for adding labels (e.g., `TEMP: `).
    *   **Suffix Text**: (Visible for `MQTT Push` only) Static text that will always be displayed *after* the value received from MQTT. Useful for adding units (e.g., `°C`).
    *   **Scrolling Text**: (Visible for `Static Text` only) The exact text you want to be displayed on the marquee.
    *   **Scroll Speed**: A slider that controls how fast the text for this data point scrolls across the display. Faster speeds have a lower `ms/char` value.

##### 4. Examples

**Example 1: Displaying a Temperature from MQTT**
You have a temperature sensor in your living room that publishes the temperature to the MQTT topic `home/livingroom/temp`. You want the clock to display `LIVING ROOM: 72.5°F`.

1.  Set **Number of Data Points** to 1.
2.  In the "Data Point 1" block:
    *   Set **Data Source** to `MQTT Push`.
    *   Set **MQTT Topic** to `home/livingroom/temp`.
    *   Set **Prefix Text** to `LIVING ROOM: `.
    *   Set **Suffix Text** to `°F`.
3.  Now, when your sensor publishes `72.5` to the topic, the display will automatically show `LIVING ROOM: 72.5°F`.

**Example 2: Displaying a Static Reminder**
You want to create a simple reminder to take out the trash.

1.  Set **Number of Data Points** to 2 (or another empty slot).
2.  In the new "Data Point" block:
    *   Set **Data Source** to `Static Text`.
    *   Set **Scrolling Text** to `TAKE OUT THE TRASH`.
3.  This message will now appear in the display rotation.