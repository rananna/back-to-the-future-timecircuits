# Back to the Future - ESP32 Time Circuits Display

<p align="center">
  <img alt="A photo of the completed BTTF Clock Prop" src="images/bttf.png" width="800">
</p>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-ESP32-purple.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-00979D.svg">
  <img alt="Power" src="https://img.shields.io/badge/Power-1.21_Gigawatts!-orange.svg">
</p>

> **Great Scott!** You've found the schematics for a fully-functional, WiFi-enabled Time Circuits display. While it can't *actually* travel through time (the flux capacitor technology is still a bit tricky), it brings the iconic look, feel, and sounds of the DeLorean's dashboard right to your desk. Using an ESP32, 12 alphanumeric displays, and a little bit of 1.21-gigawatt... I mean, 5-volt... ingenuity, this display connects to your network to show the Destination Time, Present Time, and Last Time Departed, all fully configurable from a slick, mobile-friendly web interface.

---

## Table of Contents
1.  [🌟 Demonstration](#-demonstration)
2.  [✨ Features](#-features)
3.  [📸 Gallery](#-gallery)
4.  [🛠️ BOM (Bill of Materials)](#️-bom-bill-of-materials)
5.  [🚀 Installation & Setup](#-installation--setup)
6.  [💡 Configuration & Usage](#-configuration--usage)
7.  [📦 Enclosure & Case Design](#-enclosure--case-design)
8.  [❓ Troubleshooting](#-troubleshooting)
9.  [🤝 Contributing](#-contributing)
10. [🔬 Theory of Operation](#-theory-of-operation)
11. [📜 License](#-license)

---

## 🌟 Demonstration

A picture is worth a thousand words, and a video is worth a million! Check out the video below for a full demonstration of the clock's features.

**[High-Quality GIF or Embedded YouTube Video Here]**

*This demonstration showcases:*
* *The cinematic boot-up sequence.*
* *The full time travel animation with authentic sound effects.*
* *A quick tour of the web interface, highlighting how to change the destination year and switch UI themes.*

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
## 📸 Gallery

Here are a few shots of the completed Time Circuits display.

| Front View | Wiring Close-up | Enclosure Internals |
| :---: | :---: | :---: |
| **[High-quality photo of the finished clock]** | **[Detailed shot of the wiring on the breadboard or perfboard]** | **[Photo showing how components are organized inside the case]** |
| **Web UI - Time Circuits Theme** | **Web UI - OUTATIME Theme** | **Web UI - Mr. Fusion Theme** |
| **[Screenshot of the web UI with the default green theme]** | **[Screenshot of the web UI with the red 'OUTATIME' theme]** | **[Screenshot of the web UI with the orange 'Mr. Fusion' theme]** |

---
## 🛠️ BOM (Bill of Materials)

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

4.  **Wiring & Schematics**:
    ![schematic diagram](images/bttf_bb.png)
    This project uses two separate I2C buses to manage all 12 displays without address conflicts. Follow the steps and tables below carefully.

    #### Wiring Best Practices
    * **Use a Breadboard**: For initial setup, a breadboard is highly recommended to easily connect and test components.
    * **Color-Coded Wires**: Using standard wire colors (e.g., **Red** for 5V, **Black** for GND, **Yellow** for SDA, **Green** for SCL) will make wiring and troubleshooting much easier.
    * **Common Ground Rail**: It is crucial that all components share a common ground. Connect all GND pins to the same ground rail on your breadboard or a common wire.

    #### Step-by-Step Wiring Sequence

    1.  **Power Rails**: Connect your 5V power supply to the main power and ground rails of your breadboard.
    2.  **Wire I2C Bus 1 (Destination & Present Time)**:
        * Connect **GPIO 21 (SDA)** of the ESP32 to the SDA pin of the 8 "Destination" and "Present" row displays. Daisy-chain the SDA pins of these 8 displays together.
        * Connect **GPIO 22 (SCL)** of the ESP32 to the SCL pin of the same 8 displays. Daisy-chain the SCL pins together.
    3.  **Wire I2C Bus 2 (Last Time Departed)**:
        * Connect **GPIO 25 (SDA)** of the ESP32 to the SDA pin of the 4 "Last Time Departed" row displays.
        * Connect **GPIO 26 (SCL)** of the ESP32 to the SCL pin of these 4 displays.
    4.  **Connect the Audio Module**:
        * Wire the DFPlayer Mini according to the table below. Remember that the ESP32's TX pin connects to the player's RX, and vice-versa.
    5.  **Wire the Indicator LEDs**:
        * Connect the anode (+) of each of the 6 AM/PM LEDs through a 220-330Ω resistor to its corresponding GPIO pin on the ESP32. Connect the cathode (-) of each LED to the common ground rail.
    6.  **Final Power Connections**: Connect the VCC/VIN and GND pins of all components (ESP32, Displays, DFPlayer) to your main 5V and ground rails.

    #### Component Wiring Table

| Component | ESP32 Pin | Suggested Wire Color | Connection / Notes |
| :--- | :--- | :--- | :--- |
| **I2C Bus 1 (SDA)** | `GPIO 21` | Yellow | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 1 (SCL)** | `GPIO 22` | Green | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 2 (SDA)** | `GPIO 25` | Blue | Connects to the SDA pin of the 4 "Last Time Departed" row displays. |
| **I2C Bus 2 (SCL)** | `GPIO 26` | White | Connects to the SCL pin of the 4 "Last Time Departed" row displays. |
| **DFPlayer Mini (RX)** | `GPIO 17` | Orange | Connects to the **TX** pin of the DFPlayer. **Cross this connection!** |
| **DFPlayer Mini (TX)** | `GPIO 16` | Purple | Connects to the **RX** pin of the DFPlayer. **Cross this connection!** |
| **Destination AM LED** | `GPIO 13` | | Connects to the anode (+) of the AM LED for the Destination row. |
| **Destination PM LED** | `GPIO 14` | | Connects to the anode (+) of the PM LED for the Destination row. |
| **Present AM LED** | `GPIO 32` | | Connects to the anode (+) of the AM LED for the Present row. |
| **Present PM LED** | `GPIO 27` | | Connects to the anode (+) of the PM LED for the Present row. |
| **Last Dept. AM LED** | `GPIO 2` | | Connects to the anode (+) of the Last Departed row LED. |
| **Last Dept. PM LED** | `GPIO 4` | | Connects to the anode (+) of the Last Departed row LED. |
| **Power (+5V)** | `5V` | Red | Connects to the VCC/VIN pin of all components (ESP32, Displays, DFPlayer). |
| **Ground (GND)** | `GND` | Black | Connects all GND pins to a common ground rail. **Crucial for stability!** |

    #### I2C Bus and Display Addresses

    * **I2C Bus 1** (`SDA: 21`, `SCL: 22`):
        * **Destination Row**: `0x70` (Month), `0x71` (Day), `0x72` (Year), `0x73` (Time)
        * **Present Row**: `0x74` (Month), `0x75` (Day), `0x76` (Year), `0x77` (Time)
    * **I2C Bus 2** (`SDA: 25`, `SCL: 26`):
        * **Last Time Departed Row**: `0x70` (Month), `0x71` (Day), `0x72` (Year), `0x73` (Time)

5.  **Upload Web Interface Files to LittleFS**:
    * To get the web interface onto the ESP32, you need to upload the contents of the `data` folder to its flash memory. The easiest way to do this is with the **Arduino ESP32 filesystem uploader** tool.
    * **Installation**: Follow the installation instructions at the official repository: [ESP32 FS Plugin](https://github.com/earlephilhower/arduino-littlefs-upload).
    * **Usage**:
        1.  Ensure your `index.html`, `style.css`, and `script.js` files are inside a `data` folder in your main sketch directory.
        2.  In the Arduino IDE, select `Tools` -> `ESP32 Sketch Data Upload`.
        3.  This will flash the web files to the ESP32's LittleFS filesystem.

6.  **Prepare the SD Card**:
    * Format your MicroSD card to **FAT32**.
    * Create a folder named `mp3` in the root of the SD card.
    * For the clock's main sound effects to work, you must name your files *exactly* as follows and place them in the `/mp3/` folder. The system is case-sensitive!
    | Required Filename | Triggered By |
    | :--- | :--- |
    | `TIME_TRAVEL.mp3` | The main time travel animation. |
    | `ACCELERATION.mp3`| The "speeding up to 88mph" part of the animation. |
    | `WARP_WHOOSH.mp3` | The moment the time jump occurs. |
    | `ARRIVAL_THUD.mp3`| Sound played upon completion of the time travel sequence. |
    | `CONFIRM_ON.mp3` | Saving settings, waking from sleep, other confirmations. |
    | `SLEEP_ON.mp3` | Entering sleep mode. |
    | `EASTER_EGG.mp3` | Triggered by the "Great Scott!" button in the UI. |
    | `NOT_FOUND.mp3` | (Optional) A fallback sound played if a sound is not found. |

7.  **Upload the Main Code**:
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

## 📦 Enclosure & Case Design
A proper enclosure is what brings this project to life, turning it from a collection of electronics into a true prop replica.

**[Photo of your completed enclosure from the front]**
**[Photo of the enclosure from the side or back, showing how it's assembled]**

#### Construction Tips & Materials
* **Main Body**: [Describe the material you used, e.g., "The main body was constructed from a laser-cut 3mm MDF, painted matte black to replicate the look of the movie prop."]
* **Mounting**: [Explain how you mounted the components, e.g., "The display boards were mounted to a perfboard backplane, which was then secured to the enclosure using standoffs. The ESP32 and DFPlayer are housed in a separate compartment to keep wiring tidy."]
* **3D Printable Files**: [If you have them, link to the files, e.g., "The 3D models for the enclosure are available on Thingiverse: [link to your Thingiverse page]."]

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

---
## 🤝 Contributing

Contributions are what make the open-source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".

1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feature/AmazingFeature`)
5.  Open a Pull Request

---
## 🔬 Theory of Operation

For those who want to understand the inner workings of the Time Circuits clock, this section provides a high-level overview of the project's architecture.

* **The Brain: ESP32**: The entire project is orchestrated by a single **ESP32** microcontroller. This powerful chip was chosen for its dual-core processor, ample memory, and, most importantly, its built-in Wi-Fi and Bluetooth capabilities. It's responsible for everything from driving the displays and playing sounds to running the web server for the control interface.

* **Web-Based Control (REST API)**: The clock is controlled via a web interface hosted directly on the ESP32 using the `ESPAsyncWebServer` library. Your phone or computer communicates with the clock using a simple **REST API**. When you change a setting in the web UI (like the destination year), your browser sends an HTTP request to an API endpoint on the ESP32 (e.g., `POST /api/saveSettings`). The ESP32 code parses this request, updates the settings, and saves them to its flash memory.

* **Dual I2C Buses**: The project uses twelve 14-segment display modules, but the I2C protocol, which is used to control them, only supports eight unique addresses per bus (`0x70` to `0x77`). To overcome this limitation, the project leverages the ESP32's ability to create multiple I2C buses on different GPIO pins.
    * **Bus 1 (`GPIO 21/22`)**: Controls the eight displays for the "Destination Time" and "Present Time" rows.
    * **Bus 2 (`GPIO 25/26`)**: Controls the four displays for the "Last Time Departed" row.
    This dual-bus architecture allows all twelve displays to be controlled independently without address conflicts.

* **Animation State Machine**: The complex time travel animations are managed by a **state machine** within the main `.ino` file. When an animation is triggered, a global variable (`currentPhase`) transitions through a series of states (e.g., `ANIM_FLICKER`, `ANIM_DIM_OUT`). The main `loop()` function checks the current state and the time elapsed since the last state change to determine what to show on the displays and what sounds to play. This non-blocking approach ensures the device remains responsive even during a multi-second animation sequence.

---
## 📜 License

[Add License Information Here, e.g., "Distributed under the MIT License. See `LICENSE.txt` for more information."]