# Data Link Tab

This tab unlocks advanced data display capabilities, allowing the clock to show real-time data from various internet sources. The three modes on this page—Stock Ticker, Live Weather, and Data Link—are mutually exclusive. Enabling one will disable the others.

![Data Link Tab](https://raw.githubusercontent.com/rananna/back-to-the-future-timecircuits/main/docs/assets/web_ui_data_link.png)

---

### Stock Market Ticker Mode

When enabled, this mode turns the clock into a real-time stock and cryptocurrency ticker.

*   **Enable Stock Ticker Mode**: A toggle to activate this mode.
*   **Financial Modeling Prep API Key**: A free API key from [Financial Modeling Prep](https://site.financialmodelingprep.com/developer/docs) is required.
*   **Refresh Interval**: Sets how often (in minutes) the clock fetches updated stock data. A guidance message will appear to help you choose an interval that stays within the API's free tier limits.
*   **Tracked Symbols**: This list shows all the stock or crypto symbols you are currently tracking. You can drag and drop to reorder them.
*   **Add Asset**: Enter a stock symbol (e.g., `AAPL`), crypto symbol (e.g., `BTC-USD`), or index (e.g., `^GSPC`) and click **"Add"**.
*   **Check Button**: Opens a new browser tab with the raw API data for the entered symbol, which is useful for debugging.

---

### Live Weather Display

When enabled, this mode displays current and forecasted weather information.

*   **Enable Live Weather**: A toggle to activate this mode.
*   **City Name**: Enter the name of the city for the weather forecast. Click **"Lookup"** to validate the name and get its geographic coordinates. If multiple locations are found, a modal will appear for you to choose the correct one.
*   **Latitude/Longitude**: These fields are automatically populated by the "Lookup" process.
*   **Use Metric Units**: A toggle to switch between Fahrenheit/mph and Celsius/km/h.
*   **Weather Display**: Shows a real-time overview of the current weather, today's forecast, and tomorrow's forecast.
*   **Hourly Forecast**: A horizontally scrolling view of the forecast for the next several hours.
*   **Refresh Button**: Manually triggers a refresh of the weather data.

---

### Data Link Configuration

This is an advanced mode for displaying custom data from an MQTT broker.

*   **Enable Data Link Marquee**: A toggle to activate this mode.
*   **Number of Data Points**: A slider to select how many independent data points you want to configure (up to 5).
*   **Data Point Configuration**: Each data point has its own set of controls:
    *   **Data Source**:
        *   **MQTT Push**: The clock will listen on the specified MQTT topic for messages to display.
        *   **Static Text**: The clock will display the fixed text entered in the "Scrolling Text" field.
    *   **MQTT Topic**: The MQTT topic the clock should subscribe to for this data point.
    *   **Prefix/Suffix Text**: Optional text that will be added before or after the message received from MQTT.
    *   **Scrolling Text**: The static text to be displayed if the Data Source is set to "Static Text."
    *   **Scroll Speed**: Controls the speed of the marquee text.
    *   **Clear/Duplicate Buttons**: Actions to clear the fields of a data point or duplicate it to a new one.
