### `back-to-the-future-timecircuits/readme.md`

# Back to the Future - ESP32 Time Circuits Display

<p align="center">
  <img alt="License" src="https://img.shields.io/badge/License-MIT-yellow.svg">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-ESP32-purple.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-00979D.svg">
  <img alt="Power" src="https://img.shields.io/badge/Power-1.21_Gigawatts!-orange.svg">
</p>

<p align="center">
  <img src="20250722_134713.jpg" alt="BTTF Clock Prop" width="800">
</p>

> **Great Scott!** It appears you've stumbled upon the schematics for a temporal displacement unit. While this device can't actually travel through time (flux capacitor technology is still a bit tricky), it brings the iconic look and feel of the DeLorean's time circuits right to your desk. Using the power of an ESP32 and a little bit of 1.21-gigawatt... I mean, 5-volt... ingenuity, this display connects to your local WiFi network to show three distinct temporal coordinates with authentic sound effects: Destination Time, Present Time, and Last Time Departed. Everything is fully configurable from a web interface, so fire it up and get ready to see some serious stuff!

---

### Table of Contents
- [Quick Start](#quick-start)
- [Features](#features)
- [Doc's Component Checklist (BOM)](#docs-component-checklist-bom)
- [Required Lab Software (Libraries)](#required-lab-software-libraries)
- [Time Circuit Schematics (Wiring)](#time-circuit-schematics-wiring)
- [Installation & Setup](#installation--setup)
- [Understanding the LittleFS Partition](#understanding-the-littlefs-partition)
- [Operating the Time Circuits](#operating-the-time-circuits)
  - [The Web-Based Temporal Control Panel](#the-web-based-temporal-control-panel)
    - [Time Circuits Tab](#time-circuits-tab)
    - [Temporal Controls Tab](#temporal-controls-tab)
    - [Onboard Systems Tab](#onboard-systems-tab)
- [When Things Go Wrong...](#when-things-go-wrong)
- [Example Usage](#example-usage)
- [Developer Notes](#developer-notes)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

---

## Quick Start

For experienced makers in a hurry to get back to the future:
1.  **Gather Components**: Check [Doc's Component Checklist (BOM)](#docs-component-checklist-bom).
2.  **Flash Firmware & Web Files**: Follow the detailed steps in [Installation & Setup](#installation--setup) to upload the sketch and web UI files via the Arduino IDE.
3.  **Assemble Hardware**: Follow the [Time Circuit Schematics (Wiring)](#time-circuit-schematics-wiring) and perform all wiring.
4.  **Flash Firmware**:
    * Open the project (`back-to-the-future-timecircuits.ino`) in the Arduino IDE and upload it.
5.  **Configure WiFi**: Connect to the `timecircuits` WiFi network; a captive portal will appear to connect the device to your home network.
6.  **Take Control**: Access the web interface at `http://timecircuits.local` to configure your time circuits.

---

## Features

<p align="center">
  <img src="webui.png" alt="Web UI Screenshot" width="800">
</p>
*[Image: A screenshot of the web interface showing the three time circuit displays and various settings tabs.]*

* **Three-Row BTTF Display Layout**: Three full rows of displays work in concert to show the iconic time circuits panel.
    * **Destination Time**: Shows the present time/date in a user-selected timezone, with a configurable destination year.
    * **Present Time**: Shows the current time and date in your local timezone.
    * **Sleep Schedule**: The displays can automatically turn off during a user-defined sleep schedule.
    * **Last Time Departed**: Displays a famous date from the movies or a custom date set by you.
* **Accurate & Automatic Time**:
    * **NTP Synchronization**: Automatically fetches time from the internet for perfect accuracy. Displays "NTP ERR" if synchronization is lost. The firmware cycles through a list of public NTP servers including `pool.ntp.org`, `time.google.com`, and `time.nist.gov`.
    * **Full Time Zone Support**: Includes automatic Daylight Saving Time adjustments based on a predefined list of timezones, which are configurable from the web UI.
* **Complete Web Interface**:
    * **Thematic Header**: The web UI now features a screen-accurate header that displays all three times, just like in the movies.
    * **Tabbed Interface**: Settings are neatly organized into three tabs: "Time Circuits," "Temporal Controls," and "Onboard Systems."
    * **Live Preview Mode**: A "Live Preview" toggle allows you to see changes on the physical clock in real-time as you adjust settings.
    * **WiFi Manager**: Simple initial WiFi setup using a captive portal.
    * **Improved UI Spacing and Responsiveness**: The web interface uses CSS Grid for adaptive layouts on wider screens and is optimized for responsive display on different devices.
* **Authentic Experience**:
    * **Physical Time Travel Animations**: Trigger a physical animation where all three displays flicker with random dates and times.
    * **Sound Effects**: Plays iconic sounds from the movie for animations and events using an integrated DFPlayer Mini MP3 module. The firmware dynamically scans for sound files in the `/mp3` folder on the LittleFS partition, allowing files to be named descriptively (e.g., `time_travel.mp3`).
    * **Multiple Animation Styles**: Choose from several animation styles, including "Sequential", "Random", "Wave", and a "Glitch" effect that adds realism by occasionally flickering a random character on the display during normal operation.
    * **Thematic Easter Eggs**: Discover hidden themes and sounds.
* **Customization & Convenience**:
    * **Live Speedometer Mode**: Switch the "Last Time Departed" row into a real-time speedometer, displaying the current wind speed for your location fetched from a live weather API.
    * **Power Saving "Sleep" Mode**: Displays automatically turn off during user-defined hours.
    * **Over-the-Air (OTA) Updates**: Update the firmware wirelessly over your WiFi network.
    * **Web UI Themes**: Change the color scheme of the web interface, with themes including 'Time Circuits', 'OUTATIME', '88 MPH', 'Plutonium Glow', 'Mr. Fusion', and 'Clock Tower'.

---

## Doc's Component Checklist (BOM)

| Category | Component | Qty |
| :------------- | :------------------------------------------------------------- | :--: |
| **Microcontroller** | [ESP32 Dev Module](https://www.aliexpress.com/item/1005006212080137.html) | 1 |
| **Audio** | [DFPlayer Mini MP3 Module](https://www.aliexpress.com/item/1005008228039985.html) | 1 |
| | [MicroSD Card (1GB+)](https://www.aliexpress.com/item/1005008978876553.html) | 1 |
| | [Small 8 Ohm Speaker](https://www.aliexpress.com/item/1005006682079525.html) | 1 |
| **Displays** | **Adafruit HT16K33 14-Segment Alphanumeric Displays** | **12** |
| **Indicators** | [5mm LEDs (Any Color)](https://www.aliexpress.com/item/1005003912454852.html) | 6 |
| **Passive Comp.** | [220-330Ω Resistors](https://www.aliexpress.com/item/1005002091320103.html) | 6 |
| **Prototyping** | [Dupont Jumper Wires](https://www.aliexpress.com/item/1005003641187997.html) | 1 set |
| | 5V Power Supply (2A+) | 1 |

---

## Required Lab Software (Libraries)

Install these libraries via the Arduino IDE Library Manager.

| Library | Author | Purpose |
| :------------------------- | :------------ | :--------------------------- |
| `WiFiManager` | tzapu | WiFi connection portal. |
| `Adafruit GFX Library` | Adafruit | Core graphics library. |
| `Adafruit LED Backpack Library`| Adafruit | Drives the 7-segment displays. |
| `DFRobotDFPlayerMini` | DFRobot | Controls the MP3 player module. |
| `ESPAsyncWebServer` | me-no-dev | Hosts the web interface. |
| `AsyncTCP` | me-no-dev | Required by ESPAsyncWebServer. |
| `arduinoJson` | bblanchon | Handles data for the web API. |

---

## Time Circuit Schematics (Wiring)

**NOTE:** This project now uses an all-I2C display architecture. This significantly simplifies wiring by eliminating the need for many individual data pins, but requires careful I2C address configuration.

The project utilizes **two separate I2C buses** on the ESP32 to support all 12 display modules. All displays on a single bus share the same SDA and SCL pins but must have a unique I2C address set via solder jumpers on the back of the display backpack.

### Power Distribution
A stable 5V source is crucial.
* **5V Power**: Connect the **VIN** pin of the ESP32, all display VCC pins, and the DFPlayer Mini VCC pin to your 5V/2A power source.
* **3.3V Power (Logic Level)**: Connect the **3.3V** output pin from the ESP32 to the **VI2C** pin on **all 12 of the Adafruit HT16K33 display backpacks**. This ensures that the I2C logic levels are matched correctly to the ESP32's 3.3V logic, which is essential for stable communication.
* **GND**: Create a common ground rail. Connect a **GND** pin from the ESP32 and all component GND pins to this rail.

### I2C Bus 1 (Destination & Present Displays)

This bus controls all eight displays for the Destination and Present rows.
* **SDA**: All eight displays connect to ESP32 **GPIO 21**.
* **SCL**: All eight displays connect to ESP32 **GPIO 22**.
* **I2C Addresses**: Set each display's address using the solder jumpers according to this table.
    * **Destination Row**:
        * Month: `0x70`
        * Day: `0x71`
        * Year: `0x72`
        * Time: `0x73`
    * **Present Row**:
        * Month: `0x74`
        * Day: `0x75`
        * Year: `0x76`
        * Time: `0x77`

### I2C Bus 2 (Last Departed Displays)

This bus controls the four displays for the Last Departed row.
* **SDA**: All four displays connect to ESP32 **GPIO 25**.
* **SCL**: All four displays connect to ESP32 **GPIO 26**.
* **I2C Addresses**: Set each display's address using the solder jumpers according to this table.
    * **Last Departed Row**:
        * Month: `0x70`
        * Day: `0x71`
        * Year: `0x72`
        * Time: `0x73`

**IMPORTANT WIRING NOTE:** It is intentional that the I2C addresses (0x70, 0x71, etc.) are reused between Bus 1 and Bus 2. This works **only because the buses use physically separate SDA/SCL pins** on the ESP32 (GPIO 21/22 vs. GPIO 25/26). You cannot have two devices with the same address on the same bus. Please double-check that your wiring for the two buses goes to the correct, distinct sets of GPIO pins.

### AM/PM Indicators

Each of the six LEDs needs a current-limiting resistor (220-330Ω) on its connection to Ground.

| Row | LED | ESP32 GPIO |
| :------------ | :--: | :--------: |
| Destination | AM | 13 |
| | PM | 14 |
| Present | AM | 32 |
| | PM | 27 |
| Last Departed | AM | 2 |
| | PM | 4 |

### Audio Module (DFPlayer Mini)

The DFPlayer Mini uses a serial connection to receive commands from the ESP32. This example uses the ESP32's `Serial2` port.
* **DFPlayer RX** → **ESP32 GPIO 17 (TX2)**
* **DFPlayer TX** → **ESP32 GPIO 16 (RX2)**
* Connect your speaker wires to the **SPK_1** and **SPK_2** pins.

---

## Installation & Setup

### 1. Arduino IDE Setup

Before you begin, you need to configure the Arduino IDE to work with the ESP32 and install the necessary libraries and tools.

1.  **Install ESP32 Board Manager**: Go to `File > Preferences` and add the following URL to the "Additional Board Manager URLs" field: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`. Then go to `Tools > Board > Boards Manager`, search for "esp32," and click install.
2.  **Install Required Libraries**: Go to `Sketch > Include Library > Manage Libraries...` and install all the libraries listed in the [Required Lab Software (Libraries)](#required-lab-software-libraries) section.
3.  **Install ESP32 Sketch Data Upload Tool**: This tool is required to upload the web interface files (`HTML`, `CSS`, `JS`) and sounds to the ESP32's internal storage.
    * Navigate to the [ESP32-FS tool's GitHub releases page](https://github.com/espressif/arduino-esp32fs/releases).
    * Download the `ESP32FS-1.0.zip` file.
    * Go to your Arduino sketchbook directory (you can find the path in `File > Preferences`).
    * Create a new directory named `tools` if it doesn't already exist.
    * Unzip the downloaded file into the `tools` directory.
    * Restart the Arduino IDE. You should now see "ESP32 Sketch Data Upload" under the `Tools` menu.

### 2. 3D Print the Housing

The housing for this project can be 3D printed. You may need to print three of the single-circuit models and arrange them in a custom enclosure. You can find an excellent base model [here](https://makerworld.com/en/models/1154106-back-to-the-future-single-time-circuit#profileId-1537667).

### 3. Hardware Assembly

Solder all components together as per the schematics. **Pay close attention to the I2C bus wiring and jumper settings for each display.** Double-check all connections before applying power.

### 4. Prepare and Upload the Web Files (LittleFS)

1.  Place your MP3 sound files inside the `data/mp3` directory in the project folder. The firmware will automatically find them on boot.
2.  With your ESP32 connected, go to `Tools > ESP32 Sketch Data Upload`. This will upload the entire `data` folder (containing the web UI and sounds) to the ESP32's LittleFS partition.

### 5. Flash the Firmware

Upload the main sketch (`back-to-the-future-timecircuits.ino`) to your ESP32 using the standard "Upload" button in the Arduino IDE.

### 6. WiFi Configuration

1.  On first boot, connect your computer or phone to the `timecircuits` WiFi network.
2.  A captive portal window should automatically appear. If it doesn't, manually navigate to `http://192.168.4.1` in your browser.
3.  Use the portal to connect the device to your home WiFi network.

---

## Understanding the LittleFS Partition

This project makes use of the ESP32's internal flash memory organized into a dedicated **LittleFS partition** for storing essential project assets.

**Why is a dedicated file system partition needed?**

* **Storage for Web Interface Files:** The web-based control panel, accessible via your browser, is composed of HTML (`index.html`), CSS (`style.css`), and JavaScript (`script.js`) files. These files are stored on the LittleFS partition and served by the ESP32's web server.
* **Storage for Sound Effect Files:** To provide an authentic auditory experience, the project includes various MP3 sound effects. These audio files are also stored within the `/mp3` directory on the LittleFS partition.
* **Efficient Flash Memory Management:** By dedicating a specific partition for these data assets, the overall flash memory is utilized efficiently, preventing these files from consuming space allocated for the main firmware.
* **Simplified Upload Workflow:** Development environments like the Arduino IDE, with tools like "ESP32 Sketch Data Upload," are designed to seamlessly transfer the contents of your project's `data` folder directly to the LittleFS partition.

In essence, the LittleFS partition acts as a miniature hard drive on your ESP32, providing a robust and organized way to store and access all the non-firmware assets required for the time circuits display to function.

---

## Operating the Time Circuits

### The Web-Based Temporal Control Panel

Once connected to your WiFi, access the full configuration panel at **http://timecircuits.local/**. If that doesn't work, find the device's IP address from your router or the Serial Monitor and use it directly.

<p align="center">
  <img src="webui.png" alt="Web UI Screenshot" width="800">
</p>
*[Image: A screenshot of the web interface showing the three time circuit displays and various settings tabs.]*

The interface is organized into three tabs:

#### Time Circuits Tab

* **Last Time Departed (Custom Presets)**: When you select a custom preset from the "Custom Jumps" group, the "Add or Edit" form fields (`Preset Name`, `Date`, and `Time`) will automatically populate. The `Add to Presets` button will change to `Update Preset` to signal that you are editing the selected entry.
* **Live Wind Speedometer**: A toggle switch allows you to change the "Last Time Departed" row into a live speedometer that shows the current wind speed for the configured latitude and longitude.
* **Destination Year**: Directly type in the four-digit year for the top "Destination Time" display.
* **Destination Time Zone**: Select a timezone for the "Destination Time" display.
* **Last Time Departed**: Use the dropdown to select a famous date or a custom preset. Manage your custom presets here.
* **Cycle Presets**: Set an interval for the "Last Time Departed" display to automatically cycle through presets.

#### Temporal Controls Tab

* **Live Preview**: When enabled, changes to settings like brightness and volume will be sent to the physical clock in real-time, with updates visible in the Serial Monitor.
* **Departure/Arrival (Sleep) Times**: These settings define a "sleep" period where the displays will turn off.
* **Display Brightness**: Adjusts the brightness of all displays.
* **Notification Volume**: Sets the volume for all sound effects.
* **Time Travel Animation Every**: Sets how often the animation plays.
* **Time Travel Sound FX On**: Enables or disables sound effects.
* **Total Animation Duration**: Control the overall length of the time travel animation sequence in milliseconds.
* **Animation Style**: Choose between:
    * **Sequential Flicker**: Displays flicker one after another.
    * **Random Flicker**: Displays flicker chaotically and simultaneously.
    * **All Displays Random Flicker**: All displays flicker with random values.
    * **Counting Up**: Displays rapidly count up with increasing numbers.
    * **Wave Flicker**: The flicker effect travels from the top display down to the bottom.
    * **Glitch Effect**: Enables an occasional, random single-digit flicker on the displays during normal operation for added realism.

#### Onboard Systems Tab

* **Present Time**: Configure the timezone for the "Present Time" display and toggle 24-hour format.
* **Network Status**: View the current WiFi network and signal strength.
* **Device Actions**: Reset WiFi credentials, reset all settings to their defaults, and trigger the "Great Scott, This Is Heavy!" easter egg sound.

---

## When Things Go Wrong (Troubleshooting Guide)

* **My displays aren't working or are flickering.**
    * Ensure all power connections (5V and GND) are securely connected to **every individual display module**. Weak power can cause displays to flicker or not light up at all. A 5V/2A power supply is recommended.
    * Verify that each display has a unique I2C address set via the solder jumpers and that the address in the code matches the physical hardware.
* **I can't connect to the captive portal, or `timecircuits.local` doesn't work.**
    * After flashing the firmware, the ESP32 will create a temporary WiFi access point named `timecircuits`. Connect to this network on your computer or phone.
    * A captive portal window should automatically appear. If it doesn't, manually navigate to `http://192.168.4.1` in your browser.
    * If you've already configured your WiFi but can't access the web UI, check your router's client list for the device's IP address. You can also monitor the Arduino IDE's Serial Monitor on boot, where the device will log its assigned IP address.
    * If all else fails, use the "Network Re-route (Reset WiFi Credentials)" button in the `Onboard Systems` tab to erase the saved WiFi settings and return to the captive portal mode.
* **There is no sound from the DFPlayer Mini.**
    * The DFPlayer uses a serial connection. Ensure the **TX** pin from the DFPlayer is connected to the ESP32's **RX2** (GPIO 16) and the DFPlayer **RX** is connected to the ESP32's **TX2** (GPIO 17).
    * Verify that your sound files are in an `mp3` folder in the root of the LittleFS partition. The new firmware will log which sound files it finds on boot in the Serial Monitor.
    * Check that the speaker is correctly wired to the DFPlayer's `SPK_1` and `SPK_2` pins.
* **The time is incorrect.**
    * The clock gets its time from NTP servers. The displays will show "NTP ERR" if it can't synchronize. Check your internet connection and router settings to ensure the ESP32 has access to the internet.
    * Verify that you have selected the correct timezone in the `Onboard Systems` tab of the web interface.
* **My custom presets form fields are not working as expected.**
    * The new feature for editing presets only works for items in the "Custom Jumps" section of the dropdown. It will not work for the hardcoded movie presets.

---

## Example Usage

Let's configure the displays for a classic BTTF scenario.

**Goal:**
* Set the **Destination Time** to **November 5, 1955**.
* Set the **Present Time** to your local time in **New York (Eastern Time)**.
* Set the **Last Time Departed** to the "Future Arrival" date from the movies.
* Have the displays turn off between **11:00 PM** and **7:00 AM**.
* Use the **Random Flicker** animation style for time travel.
* Set the animation duration to **5 seconds**.

Here’s how you would configure this using the web interface:

1.  **Navigate to the "Time Circuits" Tab**:
    * **Set the Destination Year**: In the "Destination Time & Year" section, type `1955` into the **YEAR** input field.
    * **Set the Last Time Departed**: In the "Last Time Departed" section, select `Future Arrival (2015)` from the dropdown.
2.  **Navigate to the "Temporal Controls" Tab**:
    * **Set the Sleep Period**: In "Departure/Arrival," set Departure to `23:00` and Arrival to `07:00`.
    * **Set Animation Duration**: In "Time Travel Animations," set "Total Animation Duration" to `5000` (for 5 seconds).
    * **Set Animation Style**: In "Time Travel Animations," select `Random Flicker` from the "Animation Style" dropdown.
3.  **Navigate to the "Onboard Systems" Tab**:
    * **Set the Present Timezone**: In "Present Time," select `Americas/New_York`.
4.  **Save Your Settings**: Click the glowing **Engage Time Circuits (Save All Settings)** button at the bottom of the page. Your physical displays will now reflect this classic configuration, and time travel animations will feature the 88 MPH countdown, random flicker, and dynamic sound effects!

---

## Developer Notes

For those looking to dive deeper into the code or contribute to the project, here are some technical considerations:

### **Display Architecture (All-I2C)**
The project now exclusively uses Adafruit HT16K33 14-segment alphanumeric displays for all time circuit displays (month, day, year, and time). This was a significant refactoring that simplifies the wiring by leveraging the I2C protocol. Instead of a mix of I2C and individual GPIO pins, the project now utilizes two I2C buses on the ESP32. This approach greatly reduces the number of data pins required, making the hardware assembly cleaner and more robust. The old `TM1637` library has been completely removed.

### Firmware Logging
The firmware (`back-to-the-future-timecircuits.ino`) extensively uses `ESP_LOG` macros (e.g., `ESP_LOGI`, `ESP_LOGD`, `ESP_LOGW`, `ESP_LOGE`) instead of `Serial.print`. This allows for runtime control of logging verbosity. Additionally, live preview updates from the web UI and a comprehensive status report of current settings are logged to the Serial Monitor every 30 seconds for easy monitoring.

### Web Server Endpoints
The `setupWebRoutes()` function in `back-to-the-future-timecircuits.ino` defines several API endpoints for interaction with the web interface.
* `/` (HTTP GET): Serves `index.html`.
* `/api/time` (HTTP GET): Returns a JSON object with the current time, sync status, and NTP server information.
* `/api/settings` (HTTP GET): Returns all current clock settings in a JSON format.
* `/api/saveSettings` (HTTP POST): Receives and saves new settings from the web interface.
* `/api/previewSetting` (HTTP GET): Allows live previewing of individual settings like brightness and animation style.
* `/api/timezones` (HTTP GET): Provides a JSON list of available time zones for the dropdown menus.
* `/api/timeTravel` (HTTP POST): Initiates the time travel animation sequence.

---

## Contributing

Contributions are greatly appreciated. Please fork the repo and create a pull request, or open an issue.

### How to Contribute
* **Reporting Bugs**: If you find a bug, please open an issue and include steps to reproduce, expected behavior, and actual behavior.
* **Feature Requests**: Open an issue to suggest new features or enhancements.
* **Code Contributions**: Fork the repository, create a new branch for your feature/fix, and submit a pull request. Provide clear commit messages and a detailed description of your changes.

---

## License

```text
MIT License

Copyright (c) 2025 Randall North

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.