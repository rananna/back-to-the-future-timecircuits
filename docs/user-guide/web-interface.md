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
This is the main screen for setting the time displays.
*   **Destination Time**: Set the year and time zone for the top display row.
*   **Last Time Departed & Presets**: Control the bottom display row by selecting movie-based presets or creating your own custom dates.
*   **Automatic Cycling**: Set an interval (in minutes) for the clock to automatically cycle through the presets (`0` disables).

### **Temporal Controls**
This tab controls the clock's automatic behaviors and visual effects.
*   **Sleep Schedule**: Set a daily schedule to automatically turn the displays off and on.
*   **Display**: Adjust brightness and toggle 24-hour format.
*   **Animation Sequences**: Select and run any of the built-in, multi-track cinematic animations.
*   **Sound**: Control the master volume and enable/disable time travel sound effects.
*   **Favorite Radio**: Configure and play your favorite internet radio stream.

### **Connectivity**
This tab manages all network-related settings.
*   **MQTT Broker**: Configure the connection to your MQTT broker, which is required for Home Assistant integration and the "Data Link" features.
*   **Present Time (NTP)**: Set your local time zone.

### **Data Link**
This tab unlocks advanced data display capabilities.
*   **Stock Ticker**: Shows real-time stock prices. Requires a free API key from [Financial Modeling Prep](https://site.financialmodelingprep.com/developer/docs).
*   **Live Weather**: Shows the current weather for a specified city.
*   **Data Link**: For advanced users, this allows the clock to display custom data pushed from an MQTT broker.

### **System**
This tab provides device status and system-level actions.
*   **System Status**: View WiFi signal strength, free memory, and uptime.
*   **Firmware Update**: Update the device's software over the air (OTA).
*   **UI Theme**: Customize the look of the web interface.
*   **Device Actions**: Trigger a "Great Scott!" animation or reset all settings to their factory defaults.

### **Help**
This tab contains a quick reference guide and a link to this official documentation site.

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