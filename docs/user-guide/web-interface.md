# 💡 Web Interface Guide

This guide covers the use and configuration of your Time Circuits display via its built-in web interface.

## First-Time WiFi Setup

On its first boot, the device will create its own WiFi hotspot to allow you to configure it.

1.  **Connect to the Hotspot**: Using your phone or computer, connect to the WiFi network named **`TimeCircuits-Setup`**.
2.  **Captive Portal**: A configuration page should automatically open. If it doesn't, manually navigate to `http://192.168.4.1` in your browser.
3.  **Configure and Save**: Click **"Configure WiFi,"** select your home network, enter your password, and click **"Save"**. The device will restart and connect to your network.

## Accessing the Web Interface

Once connected to your network, you can access the web UI by navigating to **`http://BTTF_TC.local`** in your browser. If that doesn't work, you can find the clock's IP address in your router's client list.

---

## Web Interface Tabs

The web interface is organized into six tabs for managing all aspects of your clock.

### **Time Circuits**
This is the main screen for setting the time displays. It directly controls the "Destination Time" (top row) and "Last Time Departed" (bottom row) displays.

#### **Destination Time & Year**
This section controls the *top* display row.
- **Time Zone**: Use this dropdown to select the time zone for the destination time. This is useful for accurately setting times in different parts of the world.
- **YEAR**: Enter the four-digit destination year. The clock will instantly update the header display to reflect this year, using the current month, day, and time.

#### **Last Time Departed & Presets**
This section controls the *bottom* display row.

- **Static Time Display**: The text at the top of this section shows the full date and time that is currently set for the "Last Time Departed" display.

- **Famous & Custom Time Jumps**: This dropdown contains a list of dates from the movies and any custom presets you have saved. Selecting an option from this list will immediately update the "Last Time Departed" display.

- **Add/Edit Presets**: This form allows you to create, edit, and delete your own custom presets.
    - **To add a new preset**: Fill in the "Preset Name," "Date," and "Time" fields and click **"Add to Presets"**.
    - **To edit a preset**: Select a custom preset from the dropdown. The form will populate with its details. Make your changes and click **"Update Preset"**.
    - **To delete a preset**: Select a custom preset from the dropdown and click **"Delete Selected Preset"**.
    - **To create a new one after editing**: Click **"+ Create New Preset"** to clear the form.

- **Cycle Presets Every (min, 0=Off)**: This slider sets an interval in minutes for the clock to automatically cycle through all available presets (both famous and custom). Setting it to `0` disables this feature.

### **Temporal Controls**
This tab controls the clock's automatic behaviors, visual effects, and sound system.

#### **Departure/Arrival (Sleep) Times**
This feature allows you to set a daily schedule for the clock to "depart" (enter a low-power sleep mode) and "arrive" (wake up and resume normal operation).
- **Departure Time**: The time the displays will turn off.
- **Arrival Time**: The time the displays will turn back on.
- **Awake Time Visualizer**: The horizontal bar provides a 24-hour visual representation of the schedule. The lighter portion shows the "awake" time, and the darker portion shows the "sleep" time.

#### **Display**
- **Display Brightness**: A slider to control the brightness of the LED displays, from `0` (off) to `7` (max).
- **24 Hour Format**: A toggle to switch the "Present Time" display between 12-hour (AM/PM) and 24-hour format.

#### **Animation Sequences**
- **Sequence Dropdown**: Select one of the many built-in cinematic animation sequences.
- **Run Button**: Immediately triggers the selected animation sequence. This is a great way to preview the effects.

#### **Sound**
- **Volume**: A master slider to control the volume of all sound effects and the internet radio.
- **Favorite Internet Radio Station**:
    - **Station Name**: A friendly name for your favorite station (e.g., "80s Hits").
    - **Station URL**: The direct streaming URL for the radio station.
    - **Play/Stop Button**: Starts or stops playback of the configured radio station.
    - **Status Display**: Shows the current state of the radio player (e.g., "Playing," "Stopped," "Connecting...").

### **Connectivity**
This tab manages all network-related settings, which are crucial for Home Assistant integration and keeping the "Present Time" display accurate.

#### **MQTT Broker Settings**
MQTT is the communication protocol used to connect your clock to Home Assistant and other smart home systems.
- **MQTT Broker Address**: The IP address or hostname of your MQTT broker (e.g., `192.168.1.100`).
- **MQTT Port**: The port for the MQTT broker, which is typically `1883`.
- **MQTT Username (optional)**: The username for your MQTT broker, if required.
- **MQTT Password (optional)**: The password for your MQTT broker, if required.

#### **Present Time & NTP**
This section controls the *middle* display row.
- **Time Synchronized**: Indicates whether the clock has successfully synchronized its time with an internet time server.
- **Time Zone**: Select your local time zone from the dropdown to ensure the "Present Time" is accurate.
- **Calibrate Present Time**: Manually triggers a time synchronization with an internet (NTP) server.

### **Data Link**
This tab unlocks advanced data display capabilities, allowing the clock to show real-time data from various internet sources. The three modes on this page—Stock Ticker, Live Weather, and Data Link—are mutually exclusive. Enabling one will disable the others.

#### **Stock Market Ticker Mode**
When enabled, this mode turns the clock into a real-time stock and cryptocurrency ticker.
- **Enable Stock Ticker Mode**: A toggle to activate this mode.
- **Financial Modeling Prep API Key**: A free API key from [Financial Modeling Prep](https://site.financialmodelingprep.com/developer/docs) is required.
- **Refresh Interval**: Sets how often (in minutes) the clock fetches updated stock data. A guidance message will appear to help you choose an interval that stays within the API's free tier limits.
- **Tracked Symbols**: This list shows all the stock or crypto symbols you are currently tracking. You can drag and drop to reorder them.
- **Add Asset**: Enter a stock symbol (e.g., `AAPL`), crypto symbol (e.g., `BTC-USD`), or index (e.g., `^GSPC`) and click **"Add"**.
- **Check Button**: Opens a new browser tab with the raw API data for the entered symbol, which is useful for debugging.

#### **Live Weather Display**
When enabled, this mode displays current and forecasted weather information.
- **Enable Live Weather**: A toggle to activate this mode.
- **City Name**: Enter the name of the city for the weather forecast. Click **"Lookup"** to validate the name and get its geographic coordinates. If multiple locations are found, a modal will appear for you to choose the correct one.
- **Latitude/Longitude**: These fields are automatically populated by the "Lookup" process.
- **Use Metric Units**: A toggle to switch between Fahrenheit/mph and Celsius/km/h.
- **Weather Display**: Shows a real-time overview of the current weather, today's forecast, and tomorrow's forecast.
- **Hourly Forecast**: A horizontally scrolling view of the forecast for the next several hours.
- **Refresh Button**: Manually triggers a refresh of the weather data.

#### **Data Link Configuration**
This is an advanced mode for displaying custom data from an MQTT broker.
- **Enable Data Link Marquee**: A toggle to activate this mode.
- **Number of Data Points**: A slider to select how many independent data points you want to configure (up to 5).
- **Data Point Configuration**: Each data point has its own set of controls:
    - **Data Source**:
        - **MQTT Push**: The clock will listen on the specified MQTT topic for messages to display.
        - **Static Text**: The clock will display the fixed text entered in the "Scrolling Text" field.
    - **MQTT Topic**: The MQTT topic the clock should subscribe to for this data point.
    - **Prefix/Suffix Text**: Optional text that will be added before or after the message received from MQTT.
    - **Scrolling Text**: The static text to be displayed if the Data Source is set to "Static Text."
    - **Scroll Speed**: Controls the speed of the marquee text.
    - **Clear/Duplicate Buttons**: Actions to clear the fields of a data point or duplicate it to a new one.

### **System**
This tab provides device status and system-level actions.

#### **System Status**
This section provides a real-time snapshot of the device's health and status.
- **Free Memory**: The amount of available RAM on the device.
- **Wi-Fi Signal**: The signal strength of the Wi-Fi connection (RSSI), measured in dBm.
- **Device Uptime**: How long the device has been running since its last reboot.
- **Home Assistant Custom ID**: The unique identifier for the device, which is also used as its MQTT client ID.

#### **Firmware Update (OTA)**
This allows you to update the device's software over the air (OTA) without needing to connect it to a computer.
- **File Input**: Click to select the new firmware (`.bin`) file from your computer.
- **Upload and Update Firmware Button**: Starts the update process. A progress bar will show the upload status. The device will automatically reboot upon successful completion.

#### **UI Theme**
Customize the look of this web interface by choosing one of the available themes. The change is applied instantly.

#### **Device Actions**
- **Great Scott! Button**: Triggers the "Great Scott!" animation sequence on the clock.
- **Reset All Settings to Default Button**: A factory reset option. This will erase all your custom settings and restore the device to its original configuration. A confirmation pop-up will appear before the reset is performed.

### **Help**
This tab provides a quick reference for the web UI and links to more comprehensive documentation.

- **Project Documentation**: This section provides a link to the main project documentation on GitHub, where you can find detailed information on hardware, software, and troubleshooting.
- **Web UI Guide**: This section contains a condensed version of this guide, providing a quick overview of each tab and a table of the available animation sequences and their descriptions.

---

## Saving Settings

The large **"Save and Engage Time Circuits"** button at the bottom of the page saves all changes. It is disabled by default and will only become active when you change a setting on any tab.

Pressing this button sends all configurations to the device, saves them to memory, and triggers the `Time Circuits Lock-In` animation to confirm the new settings have been applied.

### Advanced Data Display: MQTT Push

The **Data Link** feature allows your Time Circuits clock to display custom, real-time information from external systems. This is achieved using a technology called MQTT.

#### How It Works: The Basics of MQTT

MQTT is a lightweight messaging protocol perfect for smart home devices. It works like a postal service for your network:
-   A **Broker** is the central "post office."
-   A **Topic** is a "mailing address" (e.g., `timecircuits/alerts`).
-   A **Message** is the "letter" (e.g., `GARAGE DOOR OPEN`).

Your clock can **subscribe** to a topic, and any message **published** to that topic will be displayed on the screen.

---

#### Direct "MQTT Push"

This is the direct, technical method for sending data to the clock from any MQTT-capable source, such as a custom script or another IoT platform.

##### **Configuration**

1.  **Navigate to the "Data Link" Tab** in the clock's web UI.
2.  **Enable Data Link Marquee**: Toggle this switch ON.
3.  **Configure a Data Point**:
    *   **Data Source Type**: Select **MQTT**.
    *   **MQTT Topic**: Enter a unique topic for the clock to listen to (e.g., `timecircuits/external/realtime_clock`).
    *   **Prefix Text (Optional)**: Add text that will always appear before the message (e.g., `TIME IS NOW: `).
4.  **Save Settings**: Click the **"Save and Engage Time Circuits"** button.

##### **Practical Example: Real-Time Clock Python Script**

This Python script publishes the current time to the topic we configured above.

1.  **Install the necessary library**:
    ```bash
    pip install paho-mqtt
    ```
2.  **Create the script (`clock_publisher.py`)**:

    ```python
    import paho.mqtt.client as mqtt
    import time
    from datetime import datetime

    # --- Configuration ---
    MQTT_BROKER_HOST = "YOUR_MQTT_BROKER_IP"  # <-- IMPORTANT: CHANGE THIS
    MQTT_BROKER_PORT = 1883
    MQTT_TOPIC = "timecircuits/external/realtime_clock"

    def connect_mqtt():
        client = mqtt.Client(client_id="time_circuits_external_publisher")
        try:
            client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT)
            print(f"Successfully connected to MQTT broker at {MQTT_BROKER_HOST}")
            return client
        except Exception as e:
            print(f"Error connecting to MQTT broker: {e}")
            return None

    def publish_time(client):
        current_time_str = datetime.now().strftime("%I:%M %p")
        result = client.publish(MQTT_TOPIC, current_time_str)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"Published message to '{MQTT_TOPIC}': '{current_time_str}'")
        else:
            print(f"Failed to publish message. Return code: {result.rc}")

    if __name__ == "__main__":
        mqtt_client = connect_mqtt()
        if mqtt_client:
            mqtt_client.loop_start()
            try:
                while True:
                    publish_time(client)
                    time.sleep(60) # Update every minute
            except KeyboardInterrupt:
                print("\nScript stopped.")
            finally:
                mqtt_client.loop_stop()
                mqtt_client.disconnect()
                print("Disconnected from MQTT broker.")
    ```

3.  **Run the script**:
    ```bash
    python clock_publisher.py
    ```
    **Result**: The clock will immediately display `TIME IS NOW: 10:56 AM` (or the current time), updating every minute.