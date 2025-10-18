# 🚀 Firmware Installation Guide

This guide covers the software and firmware installation for your Time Circuits clock.

## 1. Install the Arduino IDE

*   Download and install the latest version of the **[Arduino IDE](https://www.arduino.cc/en/software)**.
*   Add support for ESP32 microcontrollers by following the official **[installation instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)** from Espressif.

## 2. Install Required Libraries

This project relies on several external libraries. Install the latest version of each from the Arduino Library Manager (`Sketch` > `Include Library` > `Manage Libraries...`).

*   `Adafruit GFX Library`
*   `Adafruit LED Backpack`
*   `WiFiManager` by tzapu
*   `ArduinoJson` by Benoit Blanchon (**v7.x recommended**)
*   `ESPAsyncWebServer` by ESP32-Community
*   `AsyncTCP` by ESP32-Community
*   `PubSubClient` by Nick O'Leary
*   `ESP32-audioI2S` by schreibfaul

## 3. Configure I2C Display Addresses

> [!WARNING]
> **This is the most critical step of the entire build.** Each of the 12 display modules must have a unique I2C address. This is set by soldering the "address jumpers" on the back of each display's circuit board. If the addresses are not set correctly, the displays will not work.

Use the table below to carefully apply solder to the address jumpers for each display.

| Display Row | Display Purpose | Final I2C Address | Solder Bridge A2 (+4) | Solder Bridge A1 (+2) | Solder Bridge A0 (+1) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Destination** | Month | **`0x70`** | No | No | No |
| **Destination** | Day | **`0x71`** | No | No | **Yes** |
| **Destination** | Year | **`0x72`** | No | **Yes** | No |
| **Destination** | Time | **`0x73`** | No | **Yes** | **Yes** |
| **Present** | Month | **`0x74`** | **Yes** | No | No |
| **Present** | Day | **`0x75`** | **Yes** | No | **Yes** |
| **Present** | Year | **`0x76`** | **Yes** | **Yes** | No |
| **Present** | Time | **`0x77`** | **Yes** | **Yes** | **Yes** |
| **Last Departed** | Month | **`0x70`** | No | No | No |
| **Last Departed** | Day | **`0x71`** | No | No | **Yes** |
| **Last Departed** | Year | **`0x72`** | No | **Yes** | No |
| **Last Departed** | Time | **`0x73`** | No | **Yes** | **Yes** |

## 4. Configure the Arduino IDE

Before uploading, you must configure two critical settings in the Arduino IDE's **Tools** menu.

#### A. Set the Partition Scheme

A custom partition scheme is required to allocate enough space for the web interface and sound files.

1.  **Confirm File Location**: Ensure the `partitions.csv` file from the repository is in the same folder as the main `back-to-the-future-timecircuits.ino` file.
2.  **Select Custom Scheme**: Restart the Arduino IDE. Navigate to **Tools > Partition Scheme** and select **"Custom (partitions.csv)"**.

> [!IMPORTANT]
> If you don't see the "Custom" option, the IDE could not find the `partitions.csv` file. Double-check its location and restart the IDE.

#### B. Install the Filesystem Uploader

The web interface and sound effects must be uploaded to the ESP32's internal flash memory. This requires a special plugin.

1.  **Install Plugin**: Download and install the **[`arduino-littlefs-upload`](https://github.com/earlephilhower/arduino-littlefs-upload/releases)** plugin.
2.  **Restart IDE**: Restart the Arduino IDE after installing the plugin.

![A screenshot of the Arduino IDE's Tools menu, highlighting the "Partition Scheme" and "ESP32 LittleFS Data Upload" options.](../images/arduino-ide-tools-menu.png)

## 5. Upload the Code and Data

Now you are ready to flash the firmware.

1.  **Upload Filesystem**: In the Arduino IDE, select **Tools > ESP32 LittleFS Data Upload**. This will upload the contents of the `data` folder.
2.  **Upload Main Firmware**: Open the `back-to-the-future-timecircuits.ino` file, select your ESP32 board and COM port from the **Tools** menu, and click the main **Upload** button.

---

> 🎉 **Firmware installation is complete!**
>
> Now, proceed to the **[First-Time WiFi Setup](./wifi-setup.md)** guide to get your clock online.