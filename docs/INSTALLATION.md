# 🚀 Installation & Setup Guide

This guide provides all the necessary steps to build, wire, and flash the firmware for your ESP32 Time Circuits display.

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

![schematic diagram](../images/bttf_bb.png)

This project uses two separate I2C buses to manage all 12 displays without address conflicts.

#### Component Wiring Table

| Component | ESP32 Pin | Suggested Wire Color | Connection / Notes |
| :--- | :--- | :--- | :--- |
| **I2C Bus 1 (SDA)** | `GPIO 21` | Yellow | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 1 (SCL)** | `GPIO 22` | Green | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 2 (SDA)** | `GPIO 25` | Blue | Connects to the SDA pin of the 4 "Last Time Departed" row displays. |
| **I2C Bus 2 (SCL)** | `GPIO 26` | White | Connects to the SCL pin of the 4 "Last Time Departed" row displays. |
| **DFPlayer Mini (RX)** | `GPIO 16` | Purple | Connects to the **TX** pin of the DFPlayer. **Cross this connection!** |
| **DFPlayer Mini (TX)** | `GPIO 17` | Orange | Connects to the **RX** pin of the DFPlayer. **Cross this connection!** |
| **Destination AM LED** | `GPIO 13` | | Connects to the anode (+) of the AM LED for the Destination row. |
| **Destination PM LED** | `GPIO 14` | | Connects to the anode (+) of the PM LED for the Destination row. |
| **Present AM LED** | `GPIO 32` | | Connects to the anode (+) of the AM LED for the Present row. |
| **Present PM LED** | `GPIO 27` | | Connects to the anode (+) of the PM LED for the Present row. |
| **Last Dept. AM LED** | `GPIO 2` | | Connects to the anode (+) of the Last Departed row LED. |
| **Last Dept. PM LED** | `GPIO 4` | | Connects to the anode (+) of the Last Departed row LED. |
| **Power (+5V)** | `5V` | Red | Connects to the VCC/VIN pin of all components. |
| **Ground (GND)** | `GND` | Black | Connects all GND pins to a common ground rail. |

---

## 🔩 3D Printed Case & Assembly

A 3D printed enclosure is highly recommended for a professional finish.

**➡️ [Download 3D Print Files from Printables.com](https://www.printables.com/model/207536-back-to-the-future-time-circuits-display)**

**Printing & Assembly Tips:**
* **Filament**: PLA or PETG are suitable.
* **Resolution**: A layer height of 0.2mm provides a good balance between speed and quality.
* **Assembly Order**: It is highly recommended to install the LEDs into the case *first*, as they are difficult to access once the display modules are in place.

---

## 💾 Software Installation

1.  **Install Arduino IDE and ESP32 Core**:
    * Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
    * Follow [these instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) to add the ESP32 board manager.

2.  **Configure the Partition Scheme**:
    > ⚠️ **Critical Step:** You **must** change the Partition Scheme to accommodate the large application size.
    * In the Arduino IDE, navigate to `Tools` -> `Partition Scheme`.
    * Select **"Huge APP (3MB No OTA/1MB SPIFFS)"**.

3.  **Install Required Libraries**:
    * Open the Library Manager (`Sketch` -> `Include Library` -> `Manage Libraries...`).
    * Search for and install the latest version of each of the following libraries:
        * `Adafruit GFX Library`
        * `Adafruit LED Backpack`
        * `DFRobotDFPlayerMini` by DFRobot
        * `WiFiManager` by tzapu
        * `ArduinoJson` by Benoit Blanchon (v6.x or v7.x)
        * `ESPAsyncWebServer` by ESP32-Community
        * `AsyncTCP` by ESP32-Community
        * `PubSubClient` by Nick O'Leary

4.  **Configure I2C Display Addresses**:
    > ⚠️ **Critical Step:** Each of the 12 display modules must have a unique address on its I2C bus. Solder the address selection jumpers on the back of each board according to the table below.

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
    * Install the **Arduino ESP32 filesystem uploader** tool from [here](https://github.com/earlephilhower/arduino-littlefs-upload).
    * In the Arduino IDE, select `Tools` -> `ESP32 Sketch Data Upload`.

6.  **Prepare the SD Card**:
    * Format your MicroSD card to **FAT32**.
    * Create a folder named `mp3` in the root of the SD card.
    * Copy your sound files into this folder, named exactly as follows:
        * `TIME_TRAVEL.mp3`
        * `ACCELERATION.mp3`
        * `FLUX_CAPACITOR_CHARGE.mp3`
        * `ARRIVAL_THUD.mp3`
        * `CONFIRM_ON.mp3`
        * `SLEEP_ON.mp3`
        * `EASTER_EGG.mp3`

7.  **Upload the Main Code**:
    * Open the `.ino` file in the Arduino IDE, select your board and COM port, and click "Upload".