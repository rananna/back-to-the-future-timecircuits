# Back to the Future - ESP32 Time Circuits Display

<p align="center">
  <img alt="A photo of the completed BTTF Clock Prop" src="20250722_134713.jpg" width="800">
</p>

<p align="center">
  <img alt="License" src="https://img.shields.io/badge/License-MIT-yellow.svg">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-ESP32-purple.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-00979D.svg">
  <img alt="Power" src="https://img.shields.io/badge/Power-1.21_Gigawatts!-orange.svg">
</p>

> **Great Scott!** You've found the schematics for a fully-functional, WiFi-enabled Time Circuits display. While it can't *actually* travel through time (the flux capacitor technology is still a bit tricky), it brings the iconic look, feel, and sounds of the DeLorean's dashboard right to your desk. Using an ESP32, 12 alphanumeric displays, and a little bit of 1.21-gigawatt... I mean, 5-volt... ingenuity, this display connects to your network to show the Destination Time, Present Time, and Last Time Departed, all fully configurable from a slick, mobile-friendly web interface.

---
## 🌟 Demonstration

A picture is worth a thousand words, and a video is worth a million! Check out the video below for a full demonstration of the clock's features, including the boot sequence, time travel animation, and a tour of the web interface.

*[Placeholder for a GIF or link to a YouTube video showing the clock in action.]*

---

## ✨ Features

This project is more than just a clock; it's a feature-packed, interactive prop.

<p align="center">
  <img src="webui.png" alt="Web UI Screenshot" width="800">
</p>
*[Image: A screenshot of the web interface showing the three time circuit displays and various settings tabs.]*

#### **Core Functionality**
* **Three-Row BTTF Display**: Three full rows of displays for Destination Time, Present Time, and Last Time Departed.
* **Accurate & Automatic Time**:
    * **NTP Synchronization**: Automatically fetches the current time from a pool of NTP servers (`pool.ntp.org`, `time.google.com`, `time.nist.gov`) to ensure the "Present Time" is always perfectly accurate.
    * **Full Time Zone Support**: Includes a comprehensive list of world time zones with automatic Daylight Saving Time adjustments. Both "Present Time" and "Destination Time" can be set to different time zones.
* **Audio-Visual Experience**:
    * **Sound Effects**: An integrated DFPlayer Mini MP3 module plays iconic movie sounds for events like time travel, button confirmations, and power-ups. The system dynamically scans the SD card for sound files.
    * **Physical Time Travel Animations**: Trigger a physical animation on the hardware where all displays flicker with random dates and times before settling on the new present time.
    * **Multiple Animation Styles**: Choose from several animation styles via the web UI, including "Sequential Flicker," "Random Flicker," "Counting Up," "Wave Flicker," and a "Glitch Effect".
    * **Boot Sequence**: A cinematic, non-blocking startup sequence plays on the displays, showing messages like "88 MPH," "RECALIBRATING," and "CAPACITOR FULL".

#### **Advanced Web Interface**
* **Live Control**: A mobile-friendly web interface allows for full control over all the clock's settings.
* **Thematic Header**: The UI header is a screen-accurate, real-time replica of the physical display, updating every second.
* **Live Preview Mode**: See changes on the physical clock instantly as you adjust sliders and toggles in the UI, without needing to hit "Save".
* **WiFi Manager**: On first boot, the ESP32 creates a WiFi hotspot and captive portal for easy initial network setup. The device can also be configured to reset its WiFi credentials from the web UI.
* **Customizable UI Themes**: Change the color scheme of the web interface to one of several included themes, such as "OUTATIME," "Plutonium Glow," or "Mr. Fusion".

#### **Customization & Convenience**
* **Live Wind Speedometer Mode**: Switch the "Last Time Departed" row into a real-time speedometer that shows the current wind speed for your geographic location, fetched from the Open-Meteo API.
* **Preset Time Jumps**: The web UI comes pre-loaded with famous dates from the movies. You can also add, edit, and delete your own custom date presets, which are saved on the device.
* **Power Saving "Sleep" Mode**: Displays can be configured to automatically turn off and on at user-defined "departure" and "arrival" times to save energy.
* **Over-the-Air (OTA) Updates**: Update the firmware wirelessly over your WiFi network using the Arduino IDE, no physical connection required after the initial flash.

---
## ⚙️ Theory of Operation

For those who want to dive deeper or modify the code, here’s a brief overview of how the software works.

* **Main Loop (`loop()`)**: The main loop is designed to be non-blocking. Instead of using long `delay()` calls, it delegates tasks to various handler functions on each iteration. It continuously calls `handleBootSequence()` until the startup animations are complete. Afterward, it runs handlers like `handleDisplayAnimation()`, `handlePresetCycling()`, `handleSleepSchedule()`, and `restoreDisplayAfterGlitch()` to manage the clock's state.

* **NTP Time Synchronization**: The clock's accuracy is maintained by a robust NTP client. On startup, it attempts to sync the time. If the initial sync fails, it automatically cycles to the next server in a predefined list (`NTP_SERVERS`) and retries after a short interval (`NTP_INITIAL_RETRY_INTERVAL_MS`). Once successful, it re-syncs periodically (`NTP_SUCCESS_INTERVAL_MS`) to maintain accuracy. A sync can also be manually triggered from the web interface.

* **Web Server & API**: The project uses the **ESPAsyncWebServer** library to host the web interface and provide a RESTful API. The `server.on()` function is used to define various API endpoints (e.g., `/api/settings`, `/api/time`, `/api/saveSettings`). These endpoints allow the frontend JavaScript to fetch data from the ESP32 and send commands to update settings, trigger animations, or request a sync. This client-server architecture cleanly separates the hardware control logic from the user interface.

---
##  BOM (Bill of Materials)

| Category          | Component                                                                  | Qty | Notes                                                                   |
| :---------------- | :------------------------------------------------------------------------- | :-: | :---------------------------------------------------------------------- |
| **Microcontroller** | [ESP32 Dev Module](https://www.aliexpress.com/item/1005006212080137.html)     |  1  | A standard 30-pin or 38-pin module will work.                           |
| **Audio** | [DFPlayer Mini MP3 Module](https://www.aliexpress.com/item/1005008228039985.html) |  1  | For playing sound effects.                                              |
|                   | [MicroSD Card (≤32GB)](https://www.aliexpress.com/item/1005008978876553.html)  |  1  | Must be formatted as FAT32.                                             |
|                   | [Small 8 Ohm Speaker](https://www.aliexpress.com/item/1005006682079525.html)      |  1  | A 0.5W or 1W speaker is sufficient.                                     |
| **Displays** | **Adafruit HT16K33 14-Segment Alphanumeric Displays** | 12  | The core of the display. Ensure they are the 14-segment "Alphanumeric" type. |
| **Indicators** | [5mm LEDs (Any Color)](https://www.aliexpress.com/item/1005003912454852.html)         |  6  | For the AM/PM indicators on each row.                                   |
| **Passive Comp.** | [220-330Ω Resistors](https://www.aliexpress.com/item/1005002091320103.html)   |  6  | Current-limiting resistors for the LEDs.                                |
| **Prototyping** | [Dupont Jumper Wires](https://www.aliexpress.com/item/1005003641187997.html)      | 1 set| For connecting all components.                                          |
| **Power** | 5V Power Supply                                                          |  1  | A supply rated for at least **2A** is recommended to power the ESP32 and all 12 displays. |

---

## 🔌 Wiring & Schematics

This project uses two separate I2C buses to manage all 12 displays without address conflicts. The connection table below is the definitive guide for wiring your components.

#### Component Wiring Table

| Component | ESP32 Pin | Connection / Notes | Source |
| :--- | :--- | :--- | :--- |
| **I2C Bus 1 (SDA)** | `GPIO 21` | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. | |
| **I2C Bus 1 (SCL)** | `GPIO 22` | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. | |
| **I2C Bus 2 (SDA)** | `GPIO 25` | Connects to the SDA pin of the 4 "Last Time Departed" row displays. | |
| **I2C Bus 2 (SCL)** | `GPIO 26` | Connects to the SCL pin of the 4 "Last Time Departed" row displays. | |
| **DFPlayer Mini (RX)** | `GPIO 17` | Connects to the **TX** pin of the DFPlayer. | |
| **DFPlayer Mini (TX)** | `GPIO 16` | Connects to the **RX** pin of the DFPlayer. | |
| **Destination AM LED** | `GPIO 13` | Connects to the anode (+) of the AM LED for the Destination row. | |
| **Destination PM LED** | `GPIO 14` | Connects to the anode (+) of the PM LED for the Destination row. | |
| **Present AM LED** | `GPIO 32` | Connects to the anode (+) of the AM LED for the Present row. | |
| **Present PM LED** | `GPIO 27` | Connects to the anode (+) of the PM LED for the Present row. | |
| **Last Dept. AM LED** | `GPIO 2` | Connects to the anode (+) of the Last Departed row LED. | |
| **Last Dept. PM LED** | `GPIO 4` | Connects to the anode (+) of the Last Departed row LED. | |
| **Power (+5V)** | `5V` | Connects to the VCC/VIN pin of all components (ESP32, Displays, DFPlayer). | |
| **Ground (GND)** | `GND` | Connects all GND pins to a common ground rail. | |

#### I2C Bus and Display Addresses

* **I2C Bus 1** (`SDA: 21`, `SCL: 22`):
    * **Destination Row**: `0x70` (Month), `0x71` (Day), `0x72` (Year), `0x73` (Time)
    * **Present Row**: `0x74` (Month), `0x75` (Day), `0x76` (Year), `0x77` (Time)
* **I2C Bus 2** (`SDA: 25`, `SCL: 26`):
    * **Last Time Departed Row**: `0x70` (Month), `0x71` (Day), `0x72` (Year), `0x73` (Time)

**

---

## 🚀 Installation & Setup

1.  **Install Arduino IDE and ESP32 Core**:
    * Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
    * Follow [these instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) to add the ESP32 board manager to your Arduino IDE.
    * In the Board Manager, select "ESP32 Dev Module" as your board.

2.  **Install Libraries**:
    * Open the Arduino Library Manager (`Sketch` -> `Include Library` -> `Manage Libraries...`) and install the following:
        * `Adafruit GFX Library`
        * `Adafruit LED Backpack`
        * `DFRobotDFPlayerMini` by DFRobot
        * `WiFiManager` by tzapu
        * `ArduinoJson` by Benoit Blanchon (v6.x recommended)
        * `ESPAsyncWebServer`
        * `AsyncTCP`

3.  **Configure I2C Display Addresses**:
    * **This is a critical step!** Each of the 12 display modules must have a unique address on its I2C bus. You must solder the address selection jumpers on the back of each board. Refer to the [Adafruit tutorial](https://learn.adafruit.com/adafruit-led-backpack/changing-i2c-address) for instructions on how to do this.

4.  **Prepare the Filesystem (LittleFS)**:
    * Download and install the [Arduino ESP32 filesystem uploader tool](https://github.com/espressif/arduino-esp32/blob/master/tools/arduino-esp32fs-plugin/README.md).
    * Place the `index.html`, `style.css`, and `script.js` files inside a `data` folder in your sketch directory (`back-to-the-future-timecircuits/data/`).
    * In the Arduino IDE, select `Tools` -> `ESP32 Sketch Data Upload` to flash the web files to the ESP32's LittleFS filesystem.

5.  **Prepare the SD Card**:
    * Format your MicroSD card to **FAT32**.
    * Create a folder named `mp3` in the root of the SD card.
    * Place your sound effect files (in `.mp3` format) inside the `/mp3/` folder. The filenames will be used as keys, so name them descriptively (e.g., `TIME_TRAVEL.mp3`, `ARRIVAL_THUD.mp3`). The code automatically maps these names to playable tracks.

6.  **Upload the Code**:
    * Open the `back-to-the-future-timecircuits.ino` file in the Arduino IDE.
    * Select the correct COM port for your ESP32.
    * Click the "Upload" button to flash the main firmware.

---

## 💡 Configuration & Usage

1.  **First-Time WiFi Setup**:
    * On the first boot, the ESP32 will create a WiFi network named `timecircuits`.
    * Connect to this network with your phone or computer. A captive portal should automatically open.
    * Select your home WiFi network, enter the password, and save. The device will then connect to your network and restart.

2.  **Accessing the Web Interface**:
    * Once connected, the device will be accessible at `http://timecircuits.local/` from any device on the same network.
    * Use the tabs—**Time Circuits**, **Temporal Controls**, and **Onboard Systems**—to configure all aspects of the clock.

3.  **Key Settings to Configure**:
    * **Time Circuits Tab**: Set your destination year, select a "Famous Time Jump," and configure preset cycling.
    * **Temporal Controls Tab**: Adjust display brightness, sound volume, animation styles, and sleep/wake times.
    * **Onboard Systems Tab**: Set your present time zone, sync with NTP servers, and manage network settings.

4.  **Engage Time Circuits!**:
    * After making changes, the **"Engage Time Circuits (Save All Settings)"** button will glow. Click it to save your configuration to the device's permanent memory. A time travel animation will play on the physical display to confirm the save.

---

## ❓ Troubleshooting

Having trouble? Here are some common issues and their solutions.

* **My displays are not turning on or are behaving erratically.**
    * **Check Power:** Ensure your 5V power supply can provide at least 2 Amps. Powering everything from a computer's USB port may not be sufficient. Verify all VCC and GND connections are secure.
    * **Check I2C Wiring:** Double-check your SDA and SCL connections for both I2C buses. A single loose wire can cause the entire bus to fail.

* **Only some of my displays work.**
    * This is almost always an **I2C address conflict**. Each display on the *same bus* must have a unique address. Carefully re-check the solder jumpers on the back of each display module to ensure they match the addresses listed in the wiring section.

* **I can't connect to the `timecircuits` WiFi network.**
    * This hotspot is only created on the very first boot or after the WiFi credentials have been reset. If the device has already connected to your home network, it will not appear again. To re-trigger it, you must either reset the credentials from the web UI or re-flash the device after clearing the preferences.

* **The web interface shows "Not Found" or is missing content.**
    * This means the LittleFS filesystem data was not uploaded correctly. Re-run the `Tools -> ESP32 Sketch Data Upload` step in the Arduino IDE. Make sure your `data` folder is correctly placed within your sketch directory.

* **No sounds are playing.**
    * **Check Wiring:** Verify the RX/TX connections between the ESP32 and the DFPlayer Mini. Remember that the ESP32's TX connects to the player's RX and vice-versa.
    * **Check SD Card:** Ensure your MicroSD card is formatted as **FAT32**. Create a folder named `mp3` in the root directory and place your `.mp3` files inside it.

Enjoy your new Time Circuits Display! Don't be surprised if you suddenly have an urge to drive 88 miles per hour.