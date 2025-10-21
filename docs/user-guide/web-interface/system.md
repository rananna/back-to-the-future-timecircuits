# System Tab

This tab provides device status and system-level actions.

![System Tab](https://raw.githubusercontent.com/rananna/back-to-the-future-timecircuits/main/docs/assets/web_ui_system.png)

---

### System Status

This section provides a real-time snapshot of the device's health and status.

*   **Free Memory**: The amount of available RAM on the device.
*   **Wi-Fi Signal**: The signal strength of the Wi-Fi connection (RSSI), measured in dBm.
*   **Device Uptime**: How long the device has been running since its last reboot.
*   **Home Assistant Custom ID**: The unique identifier for the device, which is also used as its MQTT client ID.

---

### Firmware Update (OTA)

This allows you to update the device's software over the air (OTA) without needing to connect it to a computer.

*   **File Input**: Click to select the new firmware (`.bin`) file from your computer.
*   **Upload and Update Firmware Button**: Starts the update process. A progress bar will show the upload status. The device will automatically reboot upon successful completion.

---

### UI Theme

Customize the look of this web interface by choosing one of the available themes. The change is applied instantly.

---

### Device Actions

*   **Great Scott! Button**: Triggers the "Great Scott!" animation sequence on the clock.
*   **Reset All Settings to Default Button**: A factory reset option. This will erase all your custom settings and restore the device to its original configuration. A confirmation pop-up will appear before the reset is performed.
