# Back to the Future - ESP32 Time Circuits Display

<p align="center">
  <img alt="A photo of the completed BTTF Clock Prop" src="images/bttf.png" width="800">
</p>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-ESP32-purple.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-00979D.svg">
  <img alt="Filesystem" src="https://img.shields.io/badge/Filesystem-LittleFS-green.svg">
  <img alt="JSON" src="https://img.shields.io/badge/JSON-ArduinoJSON-blue.svg">
  <img alt="Time Sync" src="https://img.shields.io/badge/Time%20Sync-NTP-orange.svg">
  <img alt="Power" src="https://img.shields.io/badge/Power-1.21_Gigawatts!-orange.svg">
  <a href="LICENSE.txt"><img alt="License" src="https://img.shields.io/badge/License-MIT-blue.svg"></a>
</p>

> **Great Scott!** You've found the schematics for a fully-functional, WiFi-enabled Time Circuits display. While it can't *actually* travel through time (the flux capacitor technology is still a bit tricky), it brings the iconic look, feel, and sounds of the DeLorean's dashboard right to your desk. Using an ESP32, 12 alphanumeric displays, and a little bit of 1.21-gigawatt... I mean, 5-volt... ingenuity, this display connects to your network to show the Destination Time, Present Time, and Last Time Departed, all fully configurable from a slick, mobile-friendly web interface.

---

## 📜 Table of Contents
1.  [🌟 Demonstration](#-demonstration)
2.  [💡 Project Philosophy](#-project-philosophy)
3.  [✨ Features](#-features)
4.  [🛠️ Bill of Materials (BOM)](#️-bill-of-materials-bom)
5.  [🔌 Wiring & Schematics](#-wiring--schematics)
6.  [🚀 Installation & Setup](#-installation--setup)
7.  [💡 Configuration & Usage](#-configuration--usage)
8.  [🔬 Theory of Operation](#-theory-of-operation)
9.  [❓ Troubleshooting](#-troubleshooting)
10. [📈 Known Issues & Future Work](#-known-issues--future-work)
11. [🤝 Contributing](#-contributing)
12. [📜 License](#-license)

---

## 🌟 Demonstration

A picture is worth a thousand words, but a video is worth a million! Check out the demonstration below for a full walkthrough of the clock's features in action.

**[Embedded YouTube Video Here]**
*A full video demonstration of the clock, from the boot-up sequence to a walkthrough of the web UI.*

<p align="center">
  **[Animated GIF of the web interface in action]**<br>
  *A preview of the live web interface, showing theme changes and real-time control.*
</p>

This demonstration showcases:
* The cinematic boot-up sequence with authentic sound effects.
* The full time travel animation, complete with flickering displays.
* A walkthrough of the mobile-friendly web interface, showing how to change the destination year, select themes, and trigger a time jump.

[Back to Top](#-table-of-contents)

---

## 💡 Project Philosophy

The goal of this project is to create a prop that is more than just a static display. It's designed to be:
* **Authentic**: Replicating the look, sounds, and behavior of the movie prop as closely as possible, including dynamic effects like display glitches.
* **Interactive**: Controllable in real-time through a modern, intuitive web interface that can be used on any phone or computer.
* **Customizable**: Allowing users to easily change themes, add their own preset dates, and configure every aspect of the clock's behavior.
* **Expandable**: Built on the powerful ESP32 platform with clean, well-documented code to serve as a foundation for future enhancements.

[Back to Top](#-table-of-contents)

---

## ✨ Features

This project is more than just a clock; it's a feature-packed, interactive prop designed for fans and makers alike.

### Features at a Glance

| Feature                  | Description                                                                                                                              |
| :----------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- |
| **Accurate Timekeeping** | Syncs with NTP servers for perfect time and supports all world timezones.                                                           |
| **Authentic Sounds** | An integrated MP3 player provides iconic sounds for time travel, confirmations, and more.                                           |
| **Full Web Control** | A mobile-friendly web interface allows for complete control over all settings, themes, and animations.                               |
| **Live Speedometer** | The "Last Time Departed" row can be switched to a live wind speedometer using the Open-Meteo API.                                      |
| **Customizable Presets** | Save and manage your own favorite time jump presets directly on the device.                                                          |
| **OTA Updates** | Update the firmware wirelessly over your Wi-Fi network.                                                                            |


<p align="center">
  <img src="images/webui.png" alt="Web UI Screenshot" width="800">
</p>
*[Image: A screenshot of the web interface showing the three time circuit displays and various settings tabs.]*

#### **Core Functionality**
* **Three-Row BTTF Display**: Three full rows of displays for Destination Time, Present Time, and Last Time Departed.
* **Accurate & Automatic Time**:
    * **NTP Synchronization**: Automatically fetches the current time from a pool of NTP servers (`pool.ntp.org`, `time.google.com`, `time.nist.gov`) to ensure the "Present Time" is always perfectly accurate.
    * **Full Time Zone Support**: Includes a comprehensive list of world time zones with automatic Daylight Saving Time adjustments.
* **Audio-Visual Experience**:
    * **Dynamic Sound Effects**: An integrated DFPlayer Mini MP3 module plays iconic movie sounds for events like time travel and button confirmations.
    * **Physical Time Travel Animations**: Trigger a physical animation on the hardware where all displays flicker with random dates and times.
    * **Multiple Animation Styles**: Choose from several animation styles via the web UI, including "Sequential Flicker," "Random Flicker," and "Wave Flicker."
    * **Random Glitch Effect**: A configurable "instability" setting allows for random, intermittent display glitches, making the prop feel more authentic.

#### **Advanced Web Interface**
* **Live Control**: A mobile-friendly web interface for full control over all the clock's settings.
* **Live Preview Mode**: See changes on the physical clock instantly as you adjust sliders and toggles in the UI, without needing to hit "Save".
* **WiFi Manager**: On first boot, the ESP32 creates a WiFi hotspot and captive portal for easy network setup.
* **Customizable UI Themes**: Change the color scheme of the web interface to one of several included themes.

#### **Customization & Convenience**
* **Live Wind Speedometer Mode**: Switch the "Last Time Departed" row into a real-time speedometer that shows the current wind speed for your geographic location, fetched from the Open-Meteo API.
* **Preset Time Jumps**: The web UI comes pre-loaded with famous dates from the movies. You can also add, edit, and delete your own custom date presets.
* **Power Saving "Sleep" Mode**: Displays can be configured to automatically turn off and on at user-defined times.
* **Over-the-Air (OTA) Updates**: Update the firmware wirelessly over your WiFi network.

[Back to Top](#-table-of-contents)

---

## 🛠️ Bill of Materials (BOM)

| Category          | Component                                                                  | Qty | Notes                                                                   |
| :---------------- | :------------------------------------------------------------------------- | :-: | :---------------------------------------------------------------------- |
| **Microcontroller** | [ESP32 Dev Module](https://www.aliexpress.com/item/1005006212080137.html)     |  1  | A standard 30-pin or 38-pin module will work.                           |
| **Audio** | [DFPlayer Mini MP3 Module](https://www.aliexpress.com/item/1005008228039985.html) |  1  | For playing sound effects.                                              |
|                   | [MicroSD Card (≤32GB)](https://www.aliexpress.com/item/1005008978876553.html)  |  1  | Must be formatted as FAT32.                                             |
|                   | [Small 8 Ohm Speaker](https://www.aliexpress.com/item/1005006682079525.html)      |  1  | A 0.5W or 1W speaker is sufficient.                                     |
| **Displays** | **Adafruit HT16K33 14-Segment Alphanumeric Displays** | 12  | Ensure they are the 14-segment "Alphanumeric" type. |
| **Indicators** | [5mm LEDs (Any Color)](https://www.aliexpress.com/item/1005003912454852.html)         |  6  | For the AM/PM indicators on each row.                                   |
| **Passive Comp.** | [220-330Ω Resistors](https://www.aliexpress.com/item/1005002091320103.html)   |  6  | Current-limiting resistors for the LEDs.                                |
| **Prototyping** | [Dupont Jumper Wires](https://www.aliexpress.com/item/1005003641187997.html)      | 1 set| For connecting all components.                                          |
| **Power** | 5V Power Supply                                                          |  1  | A supply rated for at least **2A** is recommended. |

[Back to Top](#-table-of-contents)

---

## 🔌 Wiring & Schematics

![schematic diagram](images/bttf_bb.png)

This project uses two separate I2C buses to manage all 12 displays without address conflicts. Using standard wire colors (e.g., **Red** for 5V, **Black** for GND) will make wiring and troubleshooting much easier. **It is crucial that all components share a common ground.**

### Component Wiring Table

| Component | ESP32 Pin | Connection / Notes |
| :--- | :--- | :--- |
| **I2C Bus 1** | `GPIO 21` (SDA) | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. |
| | `GPIO 22` (SCL) | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 2** | `GPIO 25` (SDA) | Connects to the SDA pin of the 4 "Last Time Departed" row displays. |
| | `GPIO 26` (SCL) | Connects to the SCL pin of the 4 "Last Time Departed" row displays. |
| **DFPlayer Mini** | `GPIO 17` | Connects to the **TX** pin of the DFPlayer. **Cross this connection!** |
| | `GPIO 16` | Connects to the **RX** pin of the DFPlayer. **Cross this connection!** |
| **Destination LEDs** | `GPIO 13` | Anode (+) of AM LED |
| | `GPIO 14` | Anode (+) of PM LED |
| **Present LEDs** | `GPIO 32` | Anode (+) of AM LED |
| | `GPIO 27` | Anode (+) of PM LED |
| **Last Dept. LEDs** | `GPIO 2` | Anode (+) of AM LED |
| | `GPIO 4` | Anode (+) of PM LED |
| **Power & Ground** | `5V` | Connects to the VCC/VIN pin of all components (ESP32, Displays, DFPlayer). |
| | `GND` | Connects all GND pins to a common ground rail. **Crucial for stability!** |

### I2C Bus and Display Addresses

* **I2C Bus 1** (`SDA: 21`, `SCL: 22`):
    * **Destination Row**: `0x70` (Month), `0x71` (Day), `0x72` (Year), `0x73` (Time)
    * **Present Row**: `0x74` (Month), `0x75` (Day), `0x76` (Year), `0x77` (Time)
* **I2C Bus 2** (`SDA: 25`, `SCL: 26`):
    * **Last Time Departed Row**: `0x70` (Month), `0x71` (Day), `0x72` (Year), `0x73` (Time)

[Back to Top](#-table-of-contents)

---

## 🚀 Installation & Setup

### 1. Install Arduino IDE and ESP32 Core
* Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
* Follow [these instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) to add the ESP32 board manager.
* In the Board Manager, select "ESP32 Dev Module" as your board.

### 2. Install Software Dependencies
Open the Arduino Library Manager (`Sketch` -> `Include Library` -> `Manage Libraries...`) and install the following:

| Library                 | Recommended Version |
| :---------------------- | :------------------ |
| `Adafruit GFX Library`  | Latest              |
| `Adafruit LED Backpack` | Latest              |
| `DFRobotDFPlayerMini`   | Latest              |
| `WiFiManager`           | Latest              |
| `ArduinoJson`           | v6.x or later       |
| `ESPAsyncWebServer`     | Latest              |
| `AsyncTCP`              | Latest              |

### 3. Configure I2C Display Addresses
**This is the most critical hardware step!** Each of the 12 display modules must have a unique address on its I2C bus.
* You must solder the address selection jumpers on the back of each board.
* Refer to the excellent [Adafruit tutorial](https://learn.adafruit.com/adafruit-led-backpack/changing-i2c-address) for clear instructions on how to do this.

> **Pro Tip:** If your displays aren't working, the first thing to check is the I2C addresses. You can run an "I2C Scanner" sketch to see which addresses the ESP32 can detect on each bus. This will quickly tell you if you have an addressing conflict or a wiring problem.

### 4. Upload Web Interface Files to LittleFS
The web interface files must be uploaded to the ESP32's flash memory. The easiest way is with the **Arduino ESP32 filesystem uploader** tool.
* **Installation**: Download the tool from the [ESP32 FS Plugin Releases Page](https://github.com/earlephilhower/arduino-littlefs-upload/releases) and follow the installation instructions.
* **Usage**:
    1.  Ensure your `index.html`, `style.css`, and `script.js` files are inside a `data` folder in your main sketch directory.
    2.  In the Arduino IDE, select `Tools` -> `ESP32 Sketch Data Upload`.

### 5. Prepare the SD Card
* Format your MicroSD card to **FAT32**.
* Create a folder named `mp3` in the root of the SD card.
* Place your sound files in the `/mp3/` folder, named *exactly* as follows (case-sensitive):
    * `TIME_TRAVEL.mp3`
    * `ACCELERATION.mp3`
    * `WARP_WHOOSH.mp3`
    * `ARRIVAL_THUD.mp3`
    * `CONFIRM_ON.mp3`
    * `SLEEP_ON.mp3`
    * `EASTER_EGG.mp3`
    * `NOT_FOUND.mp3` (Optional fallback sound)

### 6. Upload the Main Code
* Open the `.ino` file in the Arduino IDE.
* Select the correct COM port for your ESP32.
* Click the "Upload" button.

[Back to Top](#-table-of-contents)

---

## 💡 Configuration & Usage

1.  **First-Time WiFi Setup**:
    * On the first boot, the ESP32 will create a WiFi network named `timecircuits`.
    * Connect to this network with your phone or computer. A captive portal should automatically open.
    * Select your home WiFi network, enter the password, and save. The device will then connect to your network and restart.

2.  **Accessing the Web Interface**:
    * Once connected, the device is accessible at `http://timecircuits.local/`.
    * Use the tabs to configure all aspects of the clock.

3.  **Engage Time Circuits!**:
    * After making changes, the **"Engage Time Circuits (Save All Settings)"** button will glow. Click it to save your configuration and trigger a confirmation animation.

[Back to Top](#-table-of-contents)

---

## 🔬 Theory of Operation

The project is orchestrated by a single **ESP32** microcontroller, chosen for its dual-core processor and built-in Wi-Fi. It runs the web server, drives the displays, and plays sounds. The web interface communicates with the ESP32 using a **REST API**, sending HTTP requests to endpoints like `/api/saveSettings`.

To manage all twelve displays, the project uses a **dual-I2C bus architecture**. The ESP32's ability to create multiple I2C buses on different GPIO pins allows us to overcome the 8-address limit of a single bus.
* **Bus 1 (`GPIO 21/22`)**: Controls the eight displays for the "Destination Time" and "Present Time" rows.
* **Bus 2 (`GPIO 25/26`)**: Controls the four displays for the "Last Time Departed" row.

Complex animations are managed by a **non-blocking state machine** in the main loop. This ensures the device remains responsive to web requests even during a multi-second animation sequence.

[Back to Top](#-table-of-contents)

---

## ❓ Troubleshooting

* **My displays are not turning on or are behaving erratically.**
    * **Check Power:** Ensure your 5V power supply can provide at least 2 Amps. A computer's USB port is likely insufficient.
    * **Check I2C Wiring:** A single loose SDA or SCL wire can cause the entire bus to fail.

* **Only some of my displays work.**
    * This is almost always an **I2C address conflict**. Each display on the *same bus* must have a unique address. Carefully re-check the solder jumpers.

* **The web interface shows "Not Found" or is missing content.**
    * This means the LittleFS filesystem data was not uploaded correctly. Re-run the `Tools -> ESP32 Sketch Data Upload` step.

* **No sounds are playing.**
    * **Check Wiring:** Verify the RX/TX connections between the ESP32 and the DFPlayer Mini are crossed.
    * **Check SD Card:** Ensure it's formatted as **FAT32** and that the `/mp3/` folder and filenames are correct.

[Back to Top](#-table-of-contents)

---

## 📈 Known Issues & Future Work

This project is fully functional, but there's always room for improvement!
* **Known Issues**:
    * The web interface does not currently have a confirmation prompt before deleting a custom preset.
* **Future Work / Ideas**:
    * **Physical Keypad**: Adding a physical keypad for a more tactile experience when entering the destination time.
    * **Story Mode**: A mode that automatically cycles through the key dates and times from the movies.
    * **Sound Pack Manager**: A feature in the web UI to upload and manage sound packs directly.

[Back to Top](#-table-of-contents)

---

## 🤝 Contributing

Contributions are what make the open-source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".

1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feature/AmazingFeature`)
5.  Open a Pull Request

[Back to Top](#-table-of-contents)

---

## 📜 License

Distributed under the MIT License. See `LICENSE.txt` for more information.

[Back to Top](#-table-of-contents)