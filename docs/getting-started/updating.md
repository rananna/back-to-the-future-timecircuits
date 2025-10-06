# 🚀 Updating Your Device

This guide covers the methods for updating the firmware and data files (web interface, sounds) on your Time Circuits device.

### **Table of Contents**
1. [Updating Firmware (Main Software)](#-updating-firmware-main-software)
2. [Updating Data Files (Web UI & Sounds)](#-updating-data-files-web-ui--sounds)

---
## 펌 Updating Firmware (Main Software)

There are two ways to update the main firmware. The Web UI method is recommended for most users.

### **Method 1: Web UI Update (Recommended)**

This is the easiest method and does not require a physical connection to the device.

1.  **Compile the Firmware**: In the Arduino IDE, compile your sketch by selecting `Sketch` > `Verify/Compile`.
2.  **Export the Binary**: After a successful compilation, export the binary file by selecting `Sketch` > `Export compiled Binary`. This will create a `.bin` file in your sketch folder.
3.  **Upload via Web UI**:
    *   In the web UI, navigate to the **System** tab.
    *   Under the **Firmware Update (OTA)** section, click **Choose File** and select the `.bin` file you just exported.
    *   Click **Upload and Update Firmware**. The update process will begin, and the device will automatically reboot when it's complete.

### **Method 2: ArduinoOTA (For Developers)**

This method allows developers to upload new firmware directly from the Arduino IDE over the network, which is faster for iterative development.

1.  **Connect to the Same Network**: Ensure your computer is on the same WiFi network as the Time Circuits device.
2.  **Select Network Port**: In the Arduino IDE, go to **Tools > Port**. Under the "Network Ports" section, you should see your device listed (e.g., `BTTF_TC at 192.168.1.123`). Select it.
3.  **Upload**: Click the normal "Upload" button in the IDE. The new firmware will be compiled and sent to the device over WiFi.

> 🔐 **A Note on OTA Security**: For security, the ArduinoOTA update process is protected by a password. You can find and change the default password in the `setup()` function of the main `back-to-the-future-timecircuits.ino` file.

---
## 🗂️ Updating Data Files (Web UI & Sounds)

This feature allows you to update the contents of the `data` folder—the files that make up the web interface (HTML, CSS, JavaScript) or the sound effects (`.mp3` files)—without re-flashing the entire firmware.

1.  **Navigate to the System Tab**: Open the web interface and go to the **System** tab.
2.  **Select Files**: Under the **UI File Update** section, click **Choose Files**. You can select multiple files at once.
3.  **Upload**: Click **Upload UI Files**. The files will be uploaded to the device's filesystem, overwriting any existing files with the same name.
4.  **Refresh Browser**: Once the upload is complete, you must **manually refresh the page** in your browser to see the changes.
