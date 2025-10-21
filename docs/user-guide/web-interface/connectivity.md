# Connectivity Tab

This tab manages all network-related settings, which are crucial for Home Assistant integration and keeping the "Present Time" display accurate.

[Image: Connectivity Tab]

---

### MQTT Broker Settings

MQTT is the communication protocol used to connect your clock to Home Assistant and other smart home systems.

*   **MQTT Broker Address**: The IP address or hostname of your MQTT broker (e.g., `192.168.1.100`).
*   **MQTT Port**: The port for the MQTT broker, which is typically `1883`.
*   **MQTT Username (optional)**: The username for your MQTT broker, if required.
*   **MQTT Password (optional)**: The password for your MQTT broker, if required.

---

### Present Time & NTP

This section controls the **middle** display row.

*   **Time Synchronized**: Indicates whether the clock has successfully synchronized its time with an internet time server.
*   **Time Zone**: Select your local time zone from the dropdown to ensure the "Present Time" is accurate.
*   **Calibrate Present Time**: Manually triggers a time synchronization with an internet (NTP) server.
