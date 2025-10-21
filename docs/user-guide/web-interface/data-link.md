# Data Link Tab

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

---

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
