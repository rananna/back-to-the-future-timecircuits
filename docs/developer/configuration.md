# ⚙️ Configuration Reference

This document provides a reference for all major configuration options, including firmware settings (compile-time) and web UI settings.

## Firmware Configuration (`.ino` file)

These settings are defined as constants at the top of the main `back-to-the-future-timecircuits.ino` file. To change them, you must edit the `.ino` file and re-flash the firmware.

| Constant | Default Value | Description |
| :--- | :--- | :--- |
| `SERIAL_BAUD_RATE` | `115200` | The baud rate for the USB serial monitor connection. |
| `DEFAULT_TIME_ZONE` | `"America/New_York"` | The default time zone used for NTP time synchronization if none is set in the web UI. A full list of valid time zones can be found [here](https://gist.github.com/askovi/4352695). |
| `NTP_SERVER_1` | `"pool.ntp.org"` | The primary Network Time Protocol (NTP) server for syncing time. |
| `NTP_SERVER_2` | `"time.nist.gov"` | The secondary NTP server. |
| `INITIAL_DESTINATION_TIME` | `1635739200` | The default "Destination Time" shown on first boot, as a Unix timestamp. (Default: Nov 1, 2021) |
| `WIFI_MANAGER_TIMEOUT` | `300` | The number of seconds the device will wait in setup mode before timing out and restarting. |

## Web Server Configuration (`web_server.cpp`)

These settings are located in the `web_server.cpp` file.

| Constant | Default Value | Description |
| :--- | :--- | :--- |
| `OTA_PASSWORD` | `"1.21gigawatts"` | The password required to perform Over-the-Air (OTA) firmware updates via the web interface. **It is highly recommended to change this.** |

## Web UI Configuration

These settings are configured through the clock's web interface and are saved to the device's internal memory. They do not require re-flashing the firmware.

### Time Circuits Tab
*   **Destination Time & Year**: Sets the target date and time for the top display row.
*   **Last Time Departed & Presets**: Manages the bottom display row and a list of saved favorite destinations.

### Temporal Controls Tab
*   **Sleep Schedule**: Automatically turns the displays off and on at specified times.
*   **Display Brightness**: An integer from `0` (dimmest) to `15` (brightest).
*   **24-Hour Format**: Toggles between 12-hour (with AM/PM) and 24-hour time display.
*   **Time Travel Animation Style**: Selects the visual effect used for the time travel sequence.
*   **Sound Volume**: Controls the volume of the speaker (`0` to `100`).

### Data Link Tab
*   **Weather Mode**:
    *   `Enable`: Toggles the live weather display.
    *   `City`: The city to fetch weather data for.
    *   `Use Metric Units`: Switches between Fahrenheit/MPH and Celsius/KPH.
*   **Stock Ticker Mode**:
    *   `Enable`: Toggles the stock ticker display.
    *   `API Key`: Your personal API key from Financial Modeling Prep.
    *   `Refresh Interval`: How often to fetch new stock data, in minutes.
    *   `Tracked Assets`: The list of stock or crypto symbols to display.
*   **Data Link Marquee**:
    *   `Enable`: Toggles the custom marquee display.
    *   `Global MQTT Settings`: Broker address, port, and credentials for MQTT-based data points.
    *   `Data Points (1-5)`: Configuration for each scrolling message, including the data source (MQTT, Static Text), topics, and formatting.

### Network & System Tab
*   **Device Name**: The hostname for the device on the network (e.g., `bttf-clock`).
*   **OTA Password**: The password for web-based firmware updates can also be set here, which overrides the hardcoded value in `web_server.cpp`.