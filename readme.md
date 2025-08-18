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

> **Great Scott!** You've found the schematics for a fully-functional, WiFi-enabled Time Circuits display. While it can't *actually* travel through time (the flux capacitor technology is still a bit tricky), it brings the iconic look, feel, and sounds of the DeLorean's dashboard right to your desk. Using an ESP32, 12 alphanumeric displays, and a little bit of 1.21-gigawatt... I mean, 5-volt... ingenuity, this display connects to your network to show the Destination Time, Present Time, and Last Time Departed, all fully configurable from a slick, mobile-friendly web interface.

---

## Table of Contents
1.  [🌟 Demonstration](#-demonstration)
2.  [✨ Features](#-features)
3.  [📸 Gallery](#-gallery)
4.  [🛠️ Bill of Materials (BOM)](#️-bill-of-materials-bom)
5.  [🔌 Wiring & Schematics](#-wiring--schematics)
6.  [🚀 Installation & Setup](#-installation--setup)
7.  [💡 Configuration & Usage](#-configuration--usage)
    * [Adding Real-Time Data with the API Wizard (Example)](#-adding-real-time-data-with-the-api-wizard-example)
    * [Using the MQTT Data Link](#-using-the-mqtt-data-link)
    * [New! Scrolling Text Display Mode](#-new-scrolling-text-display-mode)
    * [20 API Ideas for the Time Circuits Display](#-20-api-ideas-for-the-time-circuits-display)
8.  [🔬 Theory of Operation](#-theory-of-operation)
9.  [❓ Troubleshooting](#-troubleshooting)
10. [🤝 Contributing](#-contributing)
11. [📜 License](#-license)

---

## 🌟 Demonstration

A picture is worth a thousand words, but a video is worth a million! Check out the video below for a full demonstration of the clock's features in action.

**[High-Quality GIF or Embedded YouTube Video Here]**

*This demonstration showcases:*
* The cinematic boot-up sequence with authentic sound effects.
* The full time travel animation, complete with flickering displays.
* A walkthrough of the mobile-friendly web interface, showing how to change the destination year, select themes, and trigger a time jump.

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
    * **NTP Synchronization**: Automatically fetches the current time from a pool of NTP servers (`pool.ntp.org`, `time.google.com`, `time.nist.gov`) to ensure the "Present Time" is always perfectly accurate.
    * **Full Time Zone Support**: Includes a comprehensive list of world time zones with automatic Daylight Saving Time adjustments. Both "Present Time" and "Destination Time" can be set to different time zones.
* **Audio-Visual Experience**:
    * **Dynamic Sound Effects**: An integrated DFPlayer Mini MP3 module plays iconic movie sounds for events like time travel, button confirmations, and power-ups.
    * **Physical Time Travel Animations**: Trigger a physical animation on the hardware where all displays flicker with random dates and times before settling on the new present time.
    * **Multiple Animation Styles**: Choose from 10 animation styles via the web UI, including "Sequential Flicker," "Random Flicker," "Tornado Flicker," "Capacitor Charge-Up," "Digital Rain," and "Timeline Skim".
    * **Random Glitch & Malfunction Effects**: A configurable "instability" setting allows for random, intermittent display glitches for an authentic feel. There's also a separately configurable chance for a more dramatic **"malfunction" sequence**, where displays go haywire, show an error message like "TIME CIRCUIT OVERLOAD," and simulate a full reboot.
    * **Cinematic Boot Sequence**: A non-blocking startup sequence plays on the displays, showing messages like "RECALIBRATING," and "CAPACITOR FULL".

#### **Advanced Web Interface & Data Link**
* **Live, Interactive Header**: The UI header is a screen-accurate, real-time replica of the physical display. Clicking on the **Destination, Present, or Last Time Departed** rows instantly navigates you to the corresponding settings section.
* **API Wizard**: An intuitive tool that fetches data from any JSON API, analyzes its structure, and allows you to map data points to display fields with a simple click, no coding required.
* **Robust Non-Blocking Data Link**: The configurable "Data Link" marquee uses non-blocking HTTPS requests in dedicated tasks, ensuring that slow API servers will never freeze the clock's animations or display updates.
* **MQTT Integration**: In addition to polling web APIs, data points can subscribe to an **MQTT broker** for efficient, real-time data pushes from smart home devices or other local services.
* **Dual Display Modes**: Each data point can be shown in the standard "Four Column" format or as a full-width "Scrolling Text" message for longer information.
* **WiFi Manager**: On first boot, the ESP32 creates a WiFi hotspot and captive portal named **BTTF-Clock-Setup** for easy initial network setup.
* **Customizable UI Themes**: Change the color scheme of the web interface to one of several included themes, such as "OUTATIME," "Plutonium Glow," "88 MPH," "Mr. Fusion," or "Clock Tower".

#### **Customization & Convenience**
* **Full Preset Management**: The web UI comes pre-loaded with famous dates from the movies. You can also **add, update, and delete** your own custom date presets, which are saved on the device's flash memory.
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
## 🛠️ Bill of Materials (BOM)

| Category          | Component                                                                  | Qty | Notes                                                                   |
| :---------------- | :------------------------------------------------------------------------- | :-: | :---------------------------------------------------------------------- |
| **Microcontroller** | [ESP32 Dev Module](https://www.aliexpress.com/item/1005006212080137.html)     |  1  | A standard 30-pin or 38-pin module will work.                           |
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

#### Wiring Best Practices
* **Use a Breadboard**: For initial setup, a breadboard is highly recommended to easily connect and test components.
* **Color-Coded Wires**: Using standard wire colors (e.g., **Red** for 5V, **Black** for GND, **Yellow** for SDA, **Green** for SCL) will make wiring and troubleshooting much easier.
* **Common Ground Rail**: It is crucial that all components share a common ground. Connect all GND pins to the same ground rail on your breadboard or a common wire.
* **Power Note**: A stable 5V power supply rated for **at least 2A** is highly recommended. Underpowering the device, especially from a standard computer USB port, is a common cause of instability, such as flickering displays or random resets, particularly when all 12 displays are at full brightness.

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
## 🚀 Installation & Setup

1.  **Install Arduino IDE and ESP32 Core**:
    * Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
    * Follow [these instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) to add the ESP32 board manager to your Arduino IDE.
    * **Update to the latest version of the ESP32 Core** via the Boards Manager. This is crucial as it contains important bug fixes for the SSL libraries.

2.  **Configure the Partition Scheme**:
    * **This is a critical step!** This project's code and the web interface files in the `data` folder require more space than the default Arduino partition scheme provides. You must change this setting to avoid upload errors.
    * In the Arduino IDE, navigate to `Tools` -> `Partition Scheme`.
    * Select **"Minimal SPIFFS (1.9MB APP with OTA/1.5MB SPIFFS)"** from the dropdown menu. This allocates enough space for both the main program and the web data.

3.  **Install Required Libraries**:
    * This project relies on several key libraries. All of them can be installed using the Arduino IDE's built-in Library Manager.
    * Open the Library Manager by navigating to **`Sketch` -> `Include Library` -> `Manage Libraries...`**.
    * Search for and install the latest version of each of the following libraries:
        * `Adafruit GFX Library`
        * `Adafruit LED Backpack`
        * `DFRobotDFPlayerMini` by DFRobot
        * `WiFiManager` by tzapu
        * `ArduinoJson` by Benoit Blanchon (v6.x or v7.x is recommended)
        * `ESPAsyncWebServer` by ESP32-Community
        * `AsyncTCP` by ESP32-Community
        * `PubSubClient` by Nick O'Leary

4.  **Configure I2C Display Addresses**:
    * **This is a critical step!** Each of the 12 display modules must have a unique address on its I2C bus. You must solder the address selection jumpers on the back of each board. Refer to the [Adafruit tutorial](https://learn.adafruit.com/adafruit-led-backpack/changing-i2c-address) for instructions on how to do this.

    You can change the address of a backpack very easily. Look on the back to find the two or three `A0`, `A1` or `A2` solder jumpers. Each one of these is used to hardcode in the address. If a jumper is shorted with solder, that sets the address. `A0` sets the lowest bit with a value of `1`, `A1` sets the middle bit with a value of `2` and `A2` sets the high bit with a value of `4`. The final address is `0x70 + A2 + A1 + A0`. So for example if `A2` is shorted and `A0` is shorted, the address is `0x70 + 4 + 1 = 0x75`. If only A1 is shorted, the address is `0x70 + 2 = 0x72`.

    #### **Soldering Instructions**

    Use the table below to configure the addresses for your displays. You will need to set addresses for two separate groups: one for I2C Bus 1 and one for I2C Bus 2.

    * **To bridge a jumper**: Use a soldering iron to apply a small amount of solder to connect the two pads. The connection should be a clean, solid bridge.

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
    * On the first boot, the ESP32 will create a WiFi network named **BTTF-Clock-Setup**.
    * Connect to this network with your phone or computer. A captive portal should automatically open.
    * Select your home WiFi network, enter the password, and save. The device will then connect to your network and restart.

2.  **Accessing the Web Interface**:
    * Once connected, the device will be accessible at `http://timecircuits.local/` from any device on the same network.
    * Use the tabs—**Time Circuits**, **Temporal Controls**, **Data Link**, and **Network & System**—to configure all aspects of the clock.

3.  **Key Settings to Configure**:
    * **Time Circuits Tab**: Set your destination year, select a "Famous Time Jump," and add, update, or delete your own custom presets.
    * **Temporal Controls Tab**: Adjust display brightness, sound volume, animation styles, and the frequency of the random glitch and malfunction effects.
    * **Data Link Tab**: Enable and configure the marquee feature. Select a target display row, refresh interval, and set up individual API data points.
    * **Network & System Tab**: Set your present time zone, sync with NTP servers, change the UI theme, and reset all settings to default.

4.  **Engage Time Circuits!**:
    * After making changes, the **"Engage Time Circuits (Save All Settings)"** button will become enabled. Click it to save your configuration to the device's permanent memory. A time travel animation will play on the physical display to confirm the save.

### 💡 Adding Real-Time Data with the API Wizard (Example)

The "Data Link" feature is one of the most powerful aspects of this project, allowing you to display real-time data from almost any online API directly on your time circuits. The **API Wizard** makes this process incredibly simple, even if you have no technical knowledge of APIs or JSON.

---

#### **Example: Displaying the Current Bitcoin Price**

Let's walk through setting up a data point to show the current price of Bitcoin.

#### **Step 1: Navigate to the "Data Link" Tab**

* In the web interface, click on the **Data Link** tab.
* Make sure the **"Enable Data Link Marquee"** toggle switch is turned on.
* Set the "Number of Data Points" slider to at least 1.

#### **Step 2: Choose an Example API**

* In the "Data Point 1" block, find the **"API Examples"** dropdown menu.
* Select **"Crypto: Bitcoin Price"**. The URL for the CoinGecko API will be automatically filled in for you.

#### **Step 3: Analyze the API Data**

* Click the **"Analyze API"** button.
* The clock will connect to the URL and fetch the live data. A new section will appear below, showing you the structure of the data it found. It will look something like this:

    ```
    Click a form field (Month, Day, etc.), then click a value below to map it.
    • bitcoin:
        • usd: 68500
    ```

#### **Step 4: Map the Data to the Display**

This is where the magic happens. We want to display "BTC" in the MONTH field and the price in the YEAR field.

1.  **Click the "YEAR" input field**. It will become highlighted, indicating it's the target for the next value you select.
2.  In the API results, **click on `usd: 68500`**.
3.  The text `bitcoin.usd` will instantly appear in the YEAR field.
4.  Now, type **`BTC`** directly into the **MONTH** input field.

The live preview at the bottom of the section will immediately update to show something like: `BTC DAY 68500 TIME`.

#### **Step 5: Engage Time Circuits!**

* Click the main **"Engage Time Circuits (Save All Settings)"** button at the bottom of the page.
* A time travel animation will play on your physical display, and your settings will be saved.

Your clock's Data Link marquee will now periodically fetch the live Bitcoin price and display it!

---
### 💡 Using the MQTT Data Link

For more advanced or real-time applications, the Data Link feature can connect to an **MQTT broker**. This is ideal for integrating with smart home platforms (like Home Assistant) or custom sensor projects, as it relies on data being pushed to the clock instead of the clock polling a web server.

#### How It Works

**1. Global Broker Configuration**
* **UI Location:** In the "Data Link" tab, you'll find a "Global MQTT Broker Settings" section.
* **Fields:** Set the IP address or hostname of your MQTT broker, along with the port and any required credentials.

When you save your settings, the ESP32 will use these credentials to establish a persistent connection to your broker.

**2. Configuring a Data Point for MQTT**
* **Data Source Selection:** For any data point, change the "Data Source" dropdown to **"MQTT Broker"**.
* **MQTT Topic:** Enter the exact MQTT topic you want the clock to subscribe to (e.g., `/home/livingroom/temperature`).

**3. How the Data Is Handled**
The system can handle two types of MQTT payloads automatically:

1.  **JSON Payload (Structured Data):** If the message is a JSON object (e.g., `{"temp": 72, "humidity": 45}`), you can use the path fields (`monthPath`, `dayPath`, etc.) to extract specific values, just like with a Web API.
2.  **Plain Text Payload (Simple Data):** If the payload is just text (e.g., `ONLINE`), the system automatically treats the entire message as the value for the `TIME` display field.

---

### 💡 New! Scrolling Text Display Mode

In addition to the standard "Four Column" data display, each Data Link point can be configured to use a **Scrolling Text** mode. This is perfect for displaying longer pieces of information like news headlines, song titles, or custom messages.

#### How to Configure It

1.  **Navigate to the "Data Link" Tab**: Find the Data Point you want to configure.
2.  **Select Display Mode**: Change the **"Display Mode"** dropdown to "**Scrolling Text**".
3.  **Enter Your Text**: A new "Scrolling Text" input field will appear. You can enter static text directly, or use the **API Wizard** to map a JSON path to this field to fetch dynamic text from a URL.
4.  **Preview**: A live 13-character preview will show you how your text will look on the display.
5.  **Engage Time Circuits**: Save your settings, and the text will begin scrolling across the entire width of the selected display row.

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
## 🔬 Theory of Operation

This section provides a deeper look into the project's architecture, particularly how it handles secure networking in the ESP32 environment.

### Asynchronous, Non-Blocking Architecture

The core of this project is a fully asynchronous, event-driven architecture. This is crucial for a device with complex visual elements like animations and real-time display updates.

* **The Problem with "Blocking" Code:** A simple approach to fetching web data is to make a request and wait for the response. This is called a "blocking" operation. On a microcontroller like the ESP32, this can be disastrous. If the remote server is slow to respond, the entire device will freeze—animations will stutter, sounds will be delayed, and the device will feel unresponsive.

* **The Event-Driven Solution:** This project uses an asynchronous model built on the foundational **`AsyncTCP`** and **`ESPAsyncWebServer`** libraries.
    * **Web Server:** The web server never blocks. It handles multiple connected clients simultaneously and uses callback functions to respond to requests.
    * **WebSocket Communication:** Real-time communication with the web UI is handled via WebSockets, which allows for a persistent, two-way channel without the overhead of repeated HTTP requests.
    * **API Data Fetching:** Outbound requests to external APIs are also handled in a non-blocking way. Each request is spawned in its own dedicated FreeRTOS task, which is like a lightweight background thread. This isolates the slow network operation from the main application loop, ensuring that even a 10-second API timeout will have **zero impact** on the smoothness of the display animations.

### Handling SSL/TLS on the ESP32

Securely connecting to modern APIs via HTTPS (SSL/TLS) is one of the most memory-intensive operations a microcontroller can perform. The debugging process for this project revealed several key challenges and led to the current robust implementation.

* **The Memory Corruption Challenge:** An early approach using standard certificate validation repeatedly failed with `PEM / BASE64 - Invalid character in input` errors. The root cause was not a bug in the code, but a subtle memory corruption issue. The ESP32's limited RAM caused the large certificate string to become garbled when copied from flash memory to RAM for the SSL handshake.

* **The Definitive Solution: `setInsecure()`:** While counterintuitive, the final and most reliable solution was to bypass the problematic certificate validation step.
    * **`client.setInsecure()`**: This function is called on the `WiFiClientSecure` object before making a connection.
    * **What It Does**: It instructs the SSL/TLS engine to **skip the certificate validation step**. It does *not* disable encryption. The connection to the server is still fully encrypted with TLS.
    * **Why It Works**: By skipping the validation, the client never needs to load the large, 2KB+ root certificate into its limited RAM. This completely eliminates the source of the memory corruption and the `PEM / BASE64` errors.
    * **Is It Safe?** For this project's purpose—fetching non-sensitive public data like weather or stock prices—this is a very common and acceptable practice in the embedded world. It prioritizes reliability and performance on a memory-constrained device. The data is still encrypted in transit, protecting it from casual eavesdropping.

This self-contained approach provides the most stable and reliable networking performance for the Time Circuits clock.

---

## ❓ Troubleshooting

* **Garbled or Flickering Displays**: This is almost always a power issue. Ensure you are using a 5V power supply that can provide at least 2A. A standard computer USB port is often insufficient. Also, double-check that all components share a common ground.
* **No Sound**:
    1.  Ensure your SD card is formatted as **FAT32**.
    2.  Check that there is a folder named `mp3` in the root of the SD card.
    3.  Verify that your audio files are named *exactly* as specified in the "Prepare the SD Card" section (e.g., `TIME_TRAVEL.mp3`).
    4.  Check the RX/TX wiring between the ESP32 and the DFPlayer Mini. They should be crossed (`ESP32 TX -> DFPlayer RX`, `ESP32 RX -> DFPlayer TX`).
* **Cannot Connect to `timecircuits.local`**:
    * Some network routers do not support mDNS, which is what makes `.local` addresses work.
    * Find the device's IP address by checking your router's client list or by monitoring the Serial Monitor output in the Arduino IDE when the device boots. You can then access the web UI by entering that IP address directly into your browser.
* **API Data Fails to Load**:
    * In the web UI, go to the "Network & System" tab and click the **"Calibrate Present Time"** button. The ESP32's internal clock must be accurate for HTTPS/SSL connections to work.
    * Double-check the API URL and any required authentication headers in the "Data Link" tab.

---
## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](https://github.com/your-username/your-repo/issues).

---
## 📜 License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.