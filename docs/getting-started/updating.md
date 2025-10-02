# 🚀 Updating Your Device

This guide covers the different methods for updating the firmware, web interface, and sound files on your Time Circuits device.

### **Table of Contents**
1. [Method 1: Web UI Firmware Update (Recommended)](#method-1-web-ui-firmware-update-recommended)
2. [Method 2: ArduinoOTA Update (For Developers)](#method-2-arduinoota-update-for-developers)
3. [Method 3: Web UI Data File Update](#method-3-web-ui-data-file-update)

---

## Method 1: Web UI Firmware Update (Recommended)

This is the easiest and most common method for updating the device's core software (firmware). This process flashes the main application logic without needing to connect the device to a computer.

1.  **Compile the Firmware**: In the Arduino IDE, compile your sketch (`Sketch` > `Verify/Compile`).
2.  **Export the Binary**: Export the compiled binary file (`Sketch` > `Export compiled Binary`). This will create a `.bin` file in your sketch folder.
3.  **Upload via Web UI**:
    *   In the web UI, navigate to the **System** tab.
    *   Under the **Firmware Update (OTA)** section, click **Choose File** and select the `.bin` file you just exported.
    *   Click **Upload and Update Firmware**. The update process will begin, and the device will automatically reboot when it's complete.

> ⚠️ **Password Note**: For security, the OTA update process requires a password. The default password is **`1.21gigawatts`**. This is currently hardcoded and can be changed in `web_server.cpp`.

---

## Method 2: ArduinoOTA Update (For Developers)

This method allows developers to upload new firmware directly from the Arduino IDE over the network, which is often faster for rapid, iterative development.

1.  **Connect to the Same Network**: Ensure your computer is on the same WiFi network as the Time Circuits device.
2.  **Select Network Port**: In the Arduino IDE, go to **Tools > Port**. Under the "Network Ports" section, you should see your device listed (e.g., `BTTF-Time-Circuits at 192.168.1.123`). Select it.
3.  **Upload**: Click the normal "Upload" button in the IDE. The new firmware will be compiled and sent to the device over WiFi.

---

## Method 3: Web UI Data File Update

This feature allows you to update the files that make up the web interface (HTML, CSS, JavaScript) or the sound effects (`.mp3` files) without re-flashing the entire firmware. This is useful for making UI tweaks or adding new sounds.

1.  **Navigate to the System Tab**: Open the web interface and go to the **System** tab.
2.  **Select Files**: Under the **UI File Update** section, click **Choose Files**. You can select multiple files at once.
3.  **Upload**: Click **Upload UI Files**. The files will be uploaded to the device's filesystem.
4.  **Refresh Browser**: Once the upload is complete, you must **manually refresh the page** in your browser to see the changes.
