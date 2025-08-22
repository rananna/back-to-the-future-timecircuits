# Back to the Future - ESP32 Time Circuits Display

<p align="center">
  <img alt="A photo of the completed BTTF Clock Prop" src="images/bttf.png" width="800">
</p>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-ESP32-purple.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-00979D.svg">
  <img alt="Power" src="https://img.shields.io/badge/Power-1.21_Gigawatts!-orange.svg">
  <a href="LICENSE.txt"><img alt="License" src="https://img.shields.io/badge/License-MIT-blue.svg"></a>
</p>

> **Great Scott!** You've found the schematics for a fully-functional, WiFi-enabled Time Circuits display. While it can't *actually* travel through time (the flux capacitor technology is still a bit tricky), it brings the iconic look, feel, and sounds of the DeLorean's dashboard right to your desk. Using an ESP32, 12 alphanumeric displays, and a little bit of 1.21-gigawatt... I mean, 5-volt... ingenuity, this display connects to your network to show the Destination Time, Present Time, and Last Time Departed, all fully configurable from a slick, mobile-friendly web interface and deeply integrated with Home Assistant.

---
## 🚀 Quick Start Guide
For experienced makers familiar with the ESP32, here's the fast track to getting your Time Circuits running:

1.  **Clone the Repository:** Download the project files to your computer.
2.  **Install Libraries:** Use the Arduino IDE's Library Manager to install all the libraries listed in the **[Installation & Setup](#-installation--setup)** section.
3.  **Set Partition Scheme:** In the Arduino IDE, go to `Tools` -> `Partition Scheme` and select **"Huge APP (3MB No OTA/1MB SPIFFS)"**.
4.  **Upload Filesystem:** Go to `Tools` -> `ESP32 Sketch Data Upload` to flash the web interface files.
5.  **Upload Firmware:** Upload the main `.ino` file to your ESP32.
6.  **Connect & Configure:** Connect to the `BTTF-Clock-Setup` WiFi network to configure your home WiFi credentials.

---

## Table of Contents
1.  [🌟 Demonstration](#-demonstration)
2.  [✨ Features](#-features)
3.  [📸 Gallery](#-gallery)
4.  [🛠️ Bill of Materials (BOM)](#️-bill-of-materials-bom)
5.  [🔌 Wiring & Schematics](#-wiring--schematics)
6.  [🔩 3D Printed Case & Assembly](#-3d-printed-case--assembly)
7.  [🚀 Installation & Setup](#-installation--setup)
8.  [💡 Configuration & Usage](#-configuration--usage)
    * [Home Assistant Integration](#-home-assistant-integration)
    * [Home Assistant Blueprints](#-home-assistant-blueprints)
    * [Example Home Assistant Automations](#-example-home-assistant-automations)
    * [Adding Real-Time Data with the API Wizard (Example)](#-adding-real-time-data-with-the-api-wizard-example)
    * [Using the MQTT Data Link](#-using-the-mqtt-data-link)
    * [20 API Ideas for the Time Circuits Display](#-20-api-ideas-for-the-time-circuits-display)
9.  [🏗️ Project Structure](#️-project-structure)
10. [🔬 Technical Deep Dive](#-technical-deep-dive)
11. [❓ Frequently Asked Questions (FAQ)](#-frequently-asked-questions-faq)
12. [🤝 Contributing](#-contributing)
13. [📜 License](#-license)

---

## 🌟 Demonstration

A picture is worth a thousand words, but a video is worth a million! Check out the video below for a full demonstration of the clock's features in action.

**[High-Quality GIF or Embedded YouTube Video Here]**

*This demonstration showcases:*
* The cinematic boot-up sequence with authentic sound effects.
* The full, multi-phase time travel animation, complete with an 88 MPH acceleration sequence and temporal displacement effects.
* A walkthrough of the mobile-friendly web interface, showing how to change settings, select themes, and trigger a time jump.

---

## ✨ Features

This project is more than just a clock; it's a feature-packed, interactive prop designed for fans and makers alike.

<p align="center">
  <img src="images/webui.png" alt="Web UI Screenshot" width="800">
</p>
*[Image: A screenshot of the web interface showing the three time circuit displays and various settings tabs.]*

#### **Core Functionality**
* **Three-Row BTTF Display**: Three full rows of displays for Destination Time, Present Time, and Last Time Departed.
* **Accurate & Automatic Time**:
    * **NTP Synchronization**: Automatically fetches the current time from a pool of NTP servers to ensure the "Present Time" is always perfectly accurate.
    * **Full Time Zone Support**: Includes a comprehensive list of world time zones with automatic Daylight Saving Time adjustments. Both "Present Time" and "Destination Time" can be set to different time zones.

#### **Authentic Audio-Visual Experience**
* **Dynamic Sound Effects**: An integrated DFPlayer Mini MP3 module plays iconic movie sounds for events like time travel, button confirmations, and power-ups.
* **Multi-Phase Time Travel Animation**: A screen-accurate, multi-stage animation sequence brings time travel to life:
    1.  **Acceleration to 88 MPH**: The bottom display transforms into a speedometer, showing the speed climbing from 0 to 88 MPH while the other displays flicker as they attempt to lock on.
    2.  **White Flash Climax**: At the moment 88 MPH is reached, all displays flash brilliantly white.
    3.  **Temporal Displacement**: The displays flicker with chaotic, random values.
    4.  **Time Blur**: All three rows rapidly cycle through years, months, and days, creating the illusion of time blurring past.
    5.  **Arrival Echo**: A final jolt and flicker where the "Present Time" briefly shows the "Destination Time" before settling.
* **Customizable Animation Styles**: Choose from 10 different visual styles for the time travel sequence to customize your experience.
* **"Temporal Glitch" on Time Sync**: After the first successful time synchronization with an NTP server, the "Present Time" display will flicker with random characters before locking onto the correct time, simulating a temporal calibration.
* **Iconic Date Override**: For maximum authenticity, the entire animation sequence temporarily uses the iconic dates from Marty's first time jump (departing Oct 26, 1985, arriving Nov 05, 1955). The clock's real time is restored upon completion.
* **Random Glitch & Malfunction Effects**: A configurable "instability" setting allows for random, intermittent display glitches. There's also a separately configurable chance for a more dramatic **"malfunction" sequence**, where displays go haywire, show an error message like "TIME CIRCUIT OVERLOAD," and simulate a full reboot.
* **Cinematic Boot Sequence**: On startup, the displays perform a "Capacitor Charge-Up" animation, filling the segments from bottom to top, accompanied by a rising sound effect, simulating the device powering on.

#### **Advanced Web Interface & Connectivity**
* **Live, Interactive Header**: The UI header is a screen-accurate, real-time replica of the physical display. Clicking on any row instantly navigates you to the corresponding settings section.
* **Two-Way UI Synchronization**: The web UI is kept perfectly in sync with the device. If a setting is changed from Home Assistant, you will see the change reflected on the web page in real-time without needing a refresh.
* **WiFi Manager**: On first boot, the ESP32 creates a WiFi hotspot and captive portal named **BTTF-Clock-Setup** for easy initial network setup.
* **Home Assistant Integration**: Full MQTT auto-discovery support for seamless integration with Home Assistant, providing extensive control and automation capabilities.

#### **Dynamic Bottom Display Modes**
The bottom "Last Time Departed" row can be reconfigured to display live, real-time data from the internet. Choose from one of three modes:
* **1. Last Time Departed (Classic Mode)**: The default screen-accurate display showing the time of the last "time jump".
* **2. Live Weather Display**: Transforms the bottom row into a multi-page weather station. It automatically fetches data for a configured city and cycles through pages showing current conditions, temperature, wind speed, and more.
* **3. Data Link Marquee**: A fully configurable marquee for displaying data from any JSON-based web API or MQTT topic.
    * **Non-Blocking Requests**: Each API request runs in its own dedicated task, ensuring that slow servers will never freeze or stutter the clock's animations.
    * **Multiple Data Sources**: Configure each of the 5 marquee data points to get its information from a Web API, an MQTT topic, or have its content **pushed directly from Home Assistant**.
    * **API Wizard**: An easy-to-use tool in the web UI that fetches data from a URL and lets you visually map JSON values to the displays without writing any code.
    * **Scrolling Text Mode**: Configure any data point to scroll long text strings across the entire 13-character width of the display row.

#### **Customization & Convenience**
* **Preset Time Jumps**: The web UI comes pre-loaded with famous dates from the movies. You can also **add, update, and delete** your own custom date presets, which are saved to the device's flash memory.
* **UI Themes**: Change the color scheme of the web interface to one of several included themes, such as "OUTATIME," "Plutonium Glow," "88 MPH," "Mr. Fusion," or "Clock Tower".
* **Power Saving "Sleep" Mode**: Displays can be configured to automatically turn off and on at user-defined "departure" and "arrival" times to save energy.

---
## 📸 Gallery

Here are a few shots of the completed Time Circuits display.

| Front View | Wiring Close-up | Enclosure Internals |
| :---: | :---: | :---: |
| **** | **** | **[Image showing components organized inside the case]** |
| **Web UI - API Wizard** | **Home Assistant Device View** | **Web UI - Mr. Fusion Theme** |
| **** | **** | **** |

---
## 🛠️ Bill of Materials (BOM)

| Category          | Component                                                                  | Qty | Notes                                                                   |
| :---------------- | :------------------------------------------------------------------------- | :-: | :---------------------------------------------------------------------- |
| **Microcontroller** | [ESP32 Dev Module](https://www.aliexpress.com/item/1005006212080137.html)     |  1  | A **38-pin** module is required for this project.                           |
| **Audio** | [DFPlayer Mini MP3 Module](https://www.aliexpress.com/item/1005008228039985.html) |  1  | For playing sound effects.                                              |
|                   | [MicroSD Card (≤32GB)](https://www.aliexpress.com/item/1005008978876553.html)  |  1  | Must be formatted as FAT32.                                             |
|                   | [Small 8 Ohm Speaker](https://www.aliexpress.com/item/1005006682079525.html)      |  1  | A 0.5W or 1W speaker is sufficient.                                     |
| **Displays** | **Adafruit HT16K33 14-Segment Alphanumeric Displays** | 12  | The core of the display. Ensure they are the **14-segment "Alphanumeric"** type. |
| **Indicators** | [5mm LEDs (Any Color)](https://www.aliexpress.com/item/1005003912454852.html)         |  6  | For the AM/PM indicators on each row.                                   |
| **Passive Comp.** | [220-330Ω Resistors](https://www.aliexpress.com/item/1005002091320103.html)   |  6  | Current-limiting resistors for the LEDs.                                |
| **Prototyping** | [Dupont Jumper Wires](https://www.aliexpress.com/item/1005003641187997.html)      | 1 set| For connecting all components.                                          |
| **Power** | 5V Power Supply                                                          |  1  | A supply rated for at least **2A** is recommended to power the ESP32 and all 12 displays. |

---

## 🔌 Wiring & Schematics

![schematic diagram](images/bttf_bb.png)
This project uses two separate I2C buses to manage all 12 displays without address conflicts. Follow the steps and tables below carefully.

#### Soldering Instructions
For a permanent and reliable build, soldering is recommended over using a breadboard. Follow this general order of operations:

1.  **Prepare the LEDs**:
    * Solder a **220Ω resistor** to the **anode** (the longer leg) of each of the 6 LEDs (red, green, and yellow).
    * Solder a wire to the other end of the resistor and another wire to the **cathode** (the shorter leg) of the LED.
    * Use **heat-shrink tubing** to insulate all exposed connections to prevent shorts.
    * It's best to install these prepared LEDs into the 3D printed case first, as they can be difficult to access later.

2.  **Prepare the Displays & I2C Buses**:
    * Solder header pins to all 12 alphanumeric display modules.
    * **I2C Bus 1 (Parallel Bus):** This bus is shared by the **Destination** and **Present** time rows (8 displays total). Create a parallel wiring harness by making common lines for 5V, GND, SDA (GPIO 21), and SCL (GPIO 22) that connect to all 8 of these displays.
    * **I2C Bus 2 (Standalone Bus):** This bus is dedicated to the **Last Time Departed** row (4 displays). Wire 5V, GND, SDA (GPIO 25), and SCL (GPIO 26) to these 4 displays.

#### Component Wiring Table

| Component | ESP32 Pin | Suggested Wire Color | Connection / Notes |
| :--- | :--- | :--- | :--- |
| **I2C Bus 1 (SDA)** | `GPIO 21` | Yellow | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 1 (SCL)** | `GPIO 22` | Green | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 2 (SDA)** | `GPIO 25` | Blue | Connects to the SDA pin of the 4 "Last Time Departed" row displays. |
| **I2C Bus 2 (SCL)** | `GPIO 26` | White | Connects to the SCL pin of the 4 "Last Time Departed" row displays. |
| **DFPlayer Mini (RX)** | `GPIO 16` | Purple | Connects to the **TX** pin of the DFPlayer. This is `DFP_RX_PIN` in the code. **Cross this connection!** |
| **DFPlayer Mini (TX)** | `GPIO 17` | Orange | Connects to the **RX** pin of the DFPlayer. This is `DFP_TX_PIN` in the code. **Cross this connection!** |
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

---
## 🔩 3D Printed Case & Assembly

To give your project a professional and screen-accurate finish, a 3D printed enclosure is highly recommended. The case not only protects the electronics but also correctly positions the displays and LEDs for the authentic Time Circuits look.

You can download the STL files required for printing the case from the link below. The model is well-designed and has been used successfully by many builders in the community.

**➡️ [Download 3D Print Files from Printables.com](https://www.printables.com/model/207536-back-to-the-future-time-circuits-display)**

**Printing & Assembly Tips:**
* **Filament**: PLA or PETG are suitable for all parts.
* **Resolution**: A layer height of 0.2mm provides a good balance between speed and quality.
* **Assembly Order**: It is highly recommended to install the LEDs into the case *first*, as they are difficult to access once the display modules are in place.

---
## 🚀 Installation & Setup

1.  **Install Arduino IDE and ESP32 Core**:
    * Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
    * Follow [these instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) to add the ESP32 board manager to your Arduino IDE.
    * **Update to the latest version of the ESP32 Core** via the Boards Manager. This is crucial as it contains important bug fixes for the SSL libraries.

2.  **Configure the Partition Scheme**:
    > ⚠️ **Critical Step:** You **must** change the Partition Scheme to accommodate the large application size. Failure to do so will result in upload errors.

    * In the Arduino IDE, navigate to `Tools` -> `Partition Scheme`.
    * Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from the dropdown menu. This allocates the maximum space to the main program.

3.  **Install Required Libraries**:
    * This project relies on several key libraries. All of them can be installed using the Arduino IDE's built-in Library Manager.
    * Open the Library Manager by navigating to **`Sketch` -> `Include Library` -> `Manage Libraries...`**.
    * Search for and install the latest version of each of the following libraries:
        * [`Adafruit GFX Library`](https://github.com/adafruit/Adafruit-GFX-Library)
        * [`Adafruit LED Backpack`](https://github.com/adafruit/Adafruit_LED_Backpack)
        * [`DFRobotDFPlayerMini`](https://github.com/DFRobot/DFRobotDFPlayerMini) by DFRobot
        * [`WiFiManager`](https://github.com/tzapu/WiFiManager) by tzapu
        * [`ArduinoJson`](https://github.com/bblanchon/ArduinoJson) by Benoit Blanchon (v6.x or v7.x is recommended)
        * [`ESPAsyncWebServer`](https://github.com/me-no-dev/ESPAsyncWebServer) by ESP32-Community
        * [`AsyncTCP`](https://github.com/me-no-dev/AsyncTCP) by ESP32-Community
        * [`PubSubClient`](https://github.com/knolleary/pubsubclient) by Nick O'Leary

4.  **Configure I2C Display Addresses**:
    > ⚠️ **Critical Step:** Each of the 12 display modules must have a unique address on its I2C bus. You must solder the address selection jumpers on the back of each board. Refer to the [Adafruit tutorial](https://learn.adafruit.com/adafruit-led-backpack/changing-i2c-address) for instructions on how to do this.

    #### **Project-Specific Jumper Connections**
    This table outlines the exact solder jumper settings you'll need for each of the 12 display modules on both I2C buses to ensure they all have the correct, unique addresses required by the project code.

| Display Row | Display Purpose | I2C Address | A2 Jumper (+4) | A1 Jumper (+2) | A0 Jumper (+1) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Destination** | Month | **0x70** | Leave Open | Leave Open | Leave Open |
| **Destination** | Day | **0x71** | Leave Open | Leave Open | **Solder Bridge** |
| **Destination** | Year | **0x72** | Leave Open | **Solder Bridge** | Leave Open |
| **Destination** | Time | **0x73** | Leave Open | **Solder Bridge** | **Solder Bridge** |
| **Present** | Month | **0x74** | **Solder Bridge** | Leave Open | Leave Open |
| **Present** | Day | **0x75** | **Solder Bridge** | Leave Open | **Solder Bridge** |
| **Present** | Year | **0x76** | **Solder Bridge** | **Solder Bridge** | Leave Open |
| **Present** | Time | **0x77** | **Solder Bridge** | **Solder Bridge** | **Solder Bridge** |
| **Last Departed** | Month | **0x70** | Leave Open | Leave Open | Leave Open |
| **Last Departed** | Day | **0x71** | Leave Open | Leave Open | **Solder Bridge** |
| **Last Departed** | Year | **0x72** | Leave Open | **Solder Bridge** | Leave Open |
| **Last Departed** | Time | **0x73** | Leave Open | **Solder Bridge** | **Solder Bridge** |

5.  **Upload Web Interface Files to LittleFS**:
    * To get the web interface onto the ESP32, you need to upload the contents of the `data` folder to its flash memory. The easiest way to do this is with the **Arduino ESP32 filesystem uploader** tool.
    * **Installation**: Follow the installation instructions at the official repository: [ESP32 FS Plugin](https://github.com/earlephilhower/arduino-littlefs-upload).
    * **Usage**:
        1.  Ensure your `index.html`, `style.css`, and script files are inside a `data` folder in your main sketch directory.
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
| `FLUX_CAPACITOR_CHARGE.mp3` | The initial "power up" phase of the animation. |
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
    * On the first boot, the ESP32 will create a WiFi network named **BTTF-Clock-Setup**.
    * Connect to this network with your phone or computer. A captive portal should automatically open.
    * Select your home WiFi network, enter the password, and save. The device will then connect to your network and restart.

2.  **Accessing the Web Interface**:
    * Once connected, find the device's IP address by checking your router's client list or by monitoring the Serial Monitor output in the Arduino IDE when the device boots.
    * You can then access the web UI by entering that IP address directly into your browser.

3.  **Key Settings to Configure**:
    * **Time Circuits Tab**: Set your destination year, select a "Famous Time Jump," and add, update, or delete your own custom presets.
    * **Temporal Controls Tab**: Adjust display brightness, sound volume, animation styles, and the frequency of the random glitch and malfunction effects.
    * **Data Link Tab**: Enable and configure the Live Weather Display or the Data Link Marquee. Set up your city for weather or configure individual API/MQTT data points.
    * **Network & System Tab**: Set your present time zone, sync with NTP servers, change the UI theme, and reset all settings to default.

4.  **Engage Time Circuits!**:
    * After making changes, the **"Engage Time Circuits (Save All Settings)"** button will become enabled. Click it to save your configuration to the device's permanent memory. A time travel animation will play on the physical display to confirm the save.

### 🏠 Home Assistant Integration

This project includes deep, "headless" integration with Home Assistant using the MQTT protocol. When you configure the MQTT broker settings, the clock will automatically announce itself to your Home Assistant instance via the auto-discovery protocol.

This creates a rich set of controls and sensors, allowing you to manage the clock and use it as a dynamic notification display without ever needing to open the web UI.

<p align="center">
  <img src="images/ha_screenshot.png" alt="Home Assistant Screenshot" width="800">
</p>
*[Image: A screenshot of the Home Assistant device page showing the various controls and sensors for the Time Circuits.]*


#### **Available Entities:**

* **Complete "Headless" Configuration**:
    * **Profile Selector**: A dropdown to select an on-device profile (`Standard`, `Cinematic`, `Silent Night`, `Unstable`) that instantly applies a bundle of pre-configured settings.
    * **Full Settings Control**: All major settings from the web UI are exposed as entities, including `Destination Year`, `Brightness`, `Volume`, `Animation Style`, `Glitch Frequency`, `Sound Toggle`, `24-Hour Format`, `Sleep/Wake Times`, and more.

* **Advanced Display & Animation Control**:
    * **Granular Text Entities**: Control the text of all **12 individual display segments** (`dest_month`, `pres_year`, etc.) directly from Home Assistant.
    * **On-Device Sequencer**: A special text entity (`run_sequence`) that accepts a simple script to run perfectly timed, non-blocking audio-visual sequences directly on the device.
    * **Effect Triggers**: A dropdown to trigger cinematic effects like `Glitch`, `Malfunction`, or the `Boot Sequence` on demand.
    * **Flash Command**: A text input to make any display segment flash for a few seconds to draw attention.

* **Dynamic Data & Marquee Control**:
    * **Temporary Marquee Override**: Send a temporary, scrolling message with a duration. After the time expires, the clock automatically reverts to its previous state.
    * **Data Source Switching**: Dynamically change the source for any of the 5 marquee data points between `API`, `MQTT`, and `Home Assistant Push`.
    * **Direct Data Push**: In "Home Assistant Push" mode, you can send the state of any HA entity directly to a marquee display slot via MQTT.

* **System & Diagnostic Entities**:
    * **System Actions**: Buttons to `Reboot`, `Factory Reset`, or `Force NTP Sync` the device from your HA dashboard.
    * **Consolidated Status Sensor**: A primary sensor reports the main status (`Idle`, `Animating`, `Asleep`) and exposes a rich set of diagnostic data as attributes, including `WiFi Strength`, `Uptime`, `Free Memory`, and more.
    * **Binary Sensors**: Dedicated binary sensors for `is_animating`, `is_malfunctioning`, etc., make it easy to trigger automations.

### 🏠 Home Assistant Blueprints

To make the most powerful features easy to use, this project includes several Home Assistant Blueprints. Simply import them into your instance to create complex automations from a simple UI.

* **Advanced Notifier**: A user-friendly way to use the on-device sequencer. Fill in fields for a message, sound, and flash effect, and the blueprint generates the script to run a perfectly timed audio-visual alert.
* **Dynamic Data Display**: Easily display the state of any Home Assistant sensor on one of the marquee slots. The blueprint automatically triggers updates whenever the sensor's value changes.
* **Cinematic Scene Trigger**: A simple blueprint to set a destination year and trigger the full time travel animation, perfect for adding cinematic flair to your existing scenes and automations.

### Example Home Assistant Automations

For a full list of useful and well-thought-out automations, please see the dedicated examples file:

**➡️ [View Home Assistant Automation Examples](HOME_ASSISTANT_EXAMPLES.md)**

### 💡 Adding Real-Time Data with the API Wizard (Example)

The "Data Link" feature is one of the most powerful aspects of this project, allowing you to display real-time data from almost any online API directly on your time circuits. The **API Wizard** makes this process incredibly simple.

<p align="center">
  <img src="images/api_wizard.png" alt="API Wizard Screenshot" width="600">
</p>
*[Image: A screenshot of the API Wizard in the web UI, showing the results of an API call and how to map a value to a display field.]*


---

#### **Example: Displaying a Stock Price**

Let's set up a data point to show the current price for Apple (AAPL), formatted as "**APL | $ 175.43**".

1.  **Navigate to the "Data Link" Tab** and enable the **"Data Link Marquee"**.
2.  **Choose an Example**: In "Data Point 1", select **"Stock: Apple Price"** from the "API Examples" dropdown. The URL will be filled in for you. *Note: This API requires a free key from alphavantage.co, which you can add to the URL.*
3.  **Analyze the API**: Click **"Analyze API"**. The wizard will fetch the data and show you its structure.
4.  **Map the Value**: In the results, find the line for the price (e.g., `05. price: "175.43"`) and click on it.
5.  **Set Prefix/Suffix**:
    * **Label**: The wizard will suggest `APL`.
    * **Prefix**: Enter `$ ` (a dollar sign followed by a space).
    * **Suffix**: Leave this blank.
    * The live preview will now show "**APL**" in the static part and "**$ 175.43**" in the scrolling part.
6.  **Engage Time Circuits!**: Click the main save button. Your clock will now display the live stock price.

---
### 💡 Using the MQTT Data Link

For real-time applications, the Data Link can connect to an **MQTT broker**. This is ideal for integrating with smart home platforms like Home Assistant, as it relies on data being pushed to the clock instead of the clock polling a web server.

1.  **Global Broker Configuration**: In the "Data Link" tab, enter your MQTT broker's address, port, and credentials.
2.  **Configure a Data Point**: For any data point, change the "Data Source" dropdown to **"MQTT Broker"**.
3.  **Set the Topic**: Enter the MQTT topic you want to subscribe to (e.g., `/home/livingroom/temperature`).
4.  **Data Handling**:
    * If the message is **JSON** (e.g., `{"temp": 72}`), you can map the values using the path fields (e.g., `temp`).
    * If the message is **plain text** (e.g., `72`), it will automatically be displayed in the `TIME` field.

---

### 💡 20 API Ideas for the Time Circuits Display

Here are 20 diverse API data examples, categorized for finance, weather, space, productivity, and fun, showing how each segment of the display line could be used effectively.

| # | Use Case | Month (3) | Day (2) | Year (4) | Time (4) | Example Display |
|:---:|:---|:---:|:---:|:---:|:---:|:---|
| 1 | **Stock Price** | `APL` | `$ ` | `175` | `.43` | `APL  $   175.43` |
| 2 | **Stock % Change** | `APL` | `CH` | `+1.2` | `5%` | `APL  CH  +1.25%` |
| 3 | **Crypto Price** | `BTC` | `K$` | `68.5` | ` ` | `BTC  K$  68.5` |
| 4 | **Market Index** | `SPX` | ` ` | `5465` | `.3` | `SPX      5465.3` |
| 5 | **Temperature (°F)** | `NYC` | `F ` | `72` | `DEG` | `NYC  F   72  DEG` |
| 6 | **"Feels Like"** | `NYC` | `FL` | `75` | `DEG` | `NYC  FL  75  DEG` |
| 7 | **Humidity** | `NYC` | `HM` | `55` | `%` | `NYC  HM  55%` |
| 8 | **Wind Speed** | `NYC` | `WND` | `10` | `MPH` | `NYC  WND  10  MPH` |
| 9 | **ISS Position** | `ISS` | `POS` | `48.8N`| `2.3E` | `ISS  POS 48.8N 2.3E` |
| 10 | **People in Space** | `PPL` | `IN` | `SPCE` | `10` | `PPL  IN  SPCE  10` |
| 11 | **Mars Rover Sol** | `ROV` | `SOL` | `1234` | | `ROV  SOL 1234` |
| 12 | **Sun Distance** | `SUN` | `DST` | `93M` | `MI` | `SUN  DST 93M  MI` |
| 13 | **Public IP** | `NET` | `IP` | *[scroll]* | *`ing`* | `NET  IP  192.168.1.100`|
| 14 | **Day / Week** | `DATE`| `D/W` | `225` | `/32` | `DATE D/W 225  /32` |
| 15 | **Network Speed**| `NET` | `D/L` | `250` | `MBPS` | `NET  D/L 250 MBPS` |
| 16 | **GitHub Commits** | `GIT` | `CMT` | `12` | `TDY` | `GIT  CMT 12  TDY` |
| 17 | **YT Subscribers** | `SUBS`| `K ` | `[scroll]`| | `SUBS K   1250K` |
| 18 | **Twitch Viewers** | `LIVE`| `VW` | `12.5` | `K` | `LIVE VW  12.5K` |
| 19 | **Holiday Countdown** | `XMAS`| ` ` | `135` | `DAYS` | `XMAS     135 DAYS` |
| 20 | **Game Server Users**| `CS2` | ` ` | `750K` | ` ` | `CS2      750K` |

---
## 🏗️ Project Structure
For those looking to understand or modify the code, here is a brief overview of the main firmware modules:

* **`back-to-the-future-timecircuits.ino`**: The main entry point of the application. It contains the `setup()` and `loop()` functions and coordinates all other modules.
* **`HardwareControl.cpp / .h`**: The hardware abstraction layer. All code for direct interaction with the displays, LEDs, and the MP3 player lives here.
* **`AnimationManager.cpp / .h`**: Contains all the logic for the complex, multi-stage animations like the time travel sequence, boot-up, and glitch effects.
* **`DisplayManager.cpp / .h`**: Responsible for what is shown on the displays during normal operation, including the standard clock, weather, and Data Link marquee.
* **`DataManager.cpp / .h`**: Handles all networking tasks for fetching and parsing data from external web APIs for the weather and Data Link modes.
* **`MqttManager.cpp / .h`**: Manages the connection to the MQTT broker and handles all communication for the Home Assistant integration.
* **`web_server.cpp / .h`**: Sets up all the API endpoints and serves the web interface files to the user's browser.
* **`EventManager.h`**: A central header that defines global state variables and data structures used across the entire project.

---
## 🔬 Technical Deep Dive

This section provides a deeper look into the project's architecture, particularly how it handles the complex requirements of a real-time, network-connected prop.

### System Architecture Overview
The project is built on a modular, event-driven architecture that is well-suited for a real-time embedded system. It effectively separates concerns into three primary layers:
1.  **Hardware Abstraction Layer (`HardwareControl`)**: This layer is responsible for all direct interaction with the physical components, such as the 12-segment displays, LEDs, and the MP3 player module. It provides a clean API for the rest of the application to control the hardware without needing to know the low-level details of I2C addresses or GPIO pins.
2.  **Application Logic/Event Management (`EventManager`)**: This is the core of the system. It contains the state machines that manage the clock's behavior, including animations, sleep schedules, data fetching, and visual effects like glitches and malfunctions. It acts as the orchestrator, responding to events and calling the appropriate functions in the other layers.
3.  **Web Interface Layer (`web_server` and `data/` directory)**: This layer provides the user interface. It consists of a backend running on the ESP32 that serves web pages and provides a RESTful API, and a frontend (HTML, CSS, JavaScript) that runs in the user's browser.

### Asynchronous, Non-Blocking by Design
The core of this project is a fully asynchronous, event-driven architecture. This is crucial for a device with complex visual elements like animations and real-time display updates.
* **The Problem with "Blocking" Code:** A simple approach to fetching web data is to make a request and wait for the response. On a microcontroller like the ESP32, this can be disastrous. If the remote server is slow to respond, the entire device will freeze—animations will stutter, sounds will be delayed, and the device will feel unresponsive.
* **The Event-Driven Solution:** This project uses an asynchronous model built on the foundational **`AsyncTCP`** and **`ESPAsyncWebServer`** libraries.
    * **Web Server:** The web server never blocks. It handles multiple connected clients simultaneously and uses callback functions to respond to requests.
    * **WebSocket Communication:** Real-time communication with the web UI is handled via WebSockets, which allows for a persistent, two-way channel without the overhead of repeated HTTP requests.
    * **API Data Fetching:** Outbound requests to external APIs are also handled in a non-blocking way. Each request is spawned in its own dedicated FreeRTOS task. This isolates the slow network operation from the main application loop, ensuring that even a 10-second API timeout will have **zero impact** on the smoothness of the display animations.

### Hardware & Display Management
* **Dual I2C Bus:** The HT16K33 display driver chip only allows for 8 unique addresses on a single bus (0x70 to 0x77). To control all 12 displays, the project cleverly splits them: 8 displays (Destination and Present rows) are on one I2C bus, and the remaining 4 (Last Time Departed row) are on a second I2C bus. This is an elegant solution that avoids the need for a more complex I2C multiplexer.
* **State Machine Logic:** The application's state is managed through several `enum` types (e.g., `AnimationPhase`, `MalfunctionPhase`, `BootSequenceState`) and handler functions in the main loop (`handleDisplayAnimation`, `handleMalfunction`, etc.). This creates a robust state machine where only one major display mode can be active at a time, preventing conflicting animations and ensuring smooth transitions between states like "animating," "malfunctioning," and "normal operation."
* **LittleFS Filesystem**: The web interface is not stored directly in the program's memory. Instead, all the HTML, CSS, and JavaScript files from the `data` directory are uploaded to the ESP32's onboard flash memory using the LittleFS filesystem. The asynchronous web server then reads these files from the flash and serves them to the user's browser on demand.

### Handling SSL/TLS on the ESP32
Securely connecting to modern APIs via HTTPS (SSL/TLS) is one of the most memory-intensive operations a microcontroller can perform.
* **The Memory Challenge:** The ESP32 has limited RAM. Loading and validating a server's full SSL certificate chain can consume a significant amount of this memory, leading to instability or crashes.
* **The Solution: `setInsecure()`:** This project uses `client.setInsecure()` before making an HTTPS connection.
    * **What It Does**: It instructs the SSL/TLS engine to **skip the certificate validation step**. It does **not** disable encryption. The connection to the server is still fully encrypted with TLS.
    * **Why It Works**: By skipping validation, the client avoids loading the large root certificate into its limited RAM. This eliminates a common source of memory-related errors and greatly improves reliability.
    * **Is It Safe?** For this project's purpose—fetching non-sensitive public data like weather or stock prices—this is a very common and acceptable practice in the embedded world. It prioritizes reliability and performance on a memory-constrained device.

---

## ❓ Frequently Asked Questions (FAQ)

* **Can I use a different type of ESP32 board?**
  * Yes, but this project was designed for a **38-pin** development board. If you use a different board (like a 30-pin version), you will need to carefully re-map the GPIO pins in `HardwareControl.h` to match your board's layout.

* **How much power does the project consume?**
  * At full brightness, the 12 displays and the ESP32 can draw a significant amount of current, potentially over 1.5A. It is highly recommended to use a **5V power supply rated for at least 2A**. A standard computer USB port is often insufficient and can lead to display flickering or instability.

* **Can I add more than 5 data points to the marquee?**
  * The firmware is currently hardcoded for a maximum of 5 data points. Increasing this would require modifying the `DataPoint` array size in the `ClockSettings` struct in `HardwareControl.h` and adjusting the loops in `DataManager.cpp` and the web interface files.

* **Why is my API key not working?**
  * Many free APIs require you to enable them in your account's cloud console before they can be used. Also, some APIs have usage limits. Double-check your API key and ensure it's correctly placed in the URL or authentication headers. Use the "Test" button in the web UI to see the raw error message from the API.

---
## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](https://github.com/your-username/your-repo/issues).

---
## 📜 License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.