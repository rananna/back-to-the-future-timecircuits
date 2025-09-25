# 🚀 Installation & Setup Guide

This guide provides all the necessary steps to build, wire, and flash the firmware for your ESP32 Time Circuits display.

### **Table of Contents**
1. [Bill of Materials (BOM)](#-bill-of-materials-bom)
2. [Wiring & Schematics](#-wiring--schematics)
3. [3D Printed Case & Assembly](#-3d-printed-case--assembly)
4. [Software Installation](#-software-installation)

---

## 🛠️ Bill of Materials (BOM)

| Qty | Component | Notes |
| :-: | :--- | :--- |
| 1 | [ESP32-S3 Dev Module](https://www.aliexpress.com/item/1005006212080137.html) | A **38-pin** module is required. An S3 model is recommended. ([Alternate](https://www.sparkfun.com/products/24408)) |
| 1 | [MAX98357A I2S DAC Amplifier](https://www.aliexpress.com/item/1005005929311653.html) | For playing sound effects directly from the ESP32. |
| 1 | [Small 8 Ohm Speaker](https://www.aliexpress.com/item/1005006682079525.html) | A 0.5W or 1W speaker is sufficient. |
| 12 | **Adafruit HT16K33 14-Segment Displays** | Ensure they are the **14-segment "Alphanumeric"** type. ([Adafruit](https://www.adafruit.com/product/1910), [Digi-Key](https://www.digikey.com/en/products/detail/adafruit-industries-llc/1910/5354394)) |
| 6 | [5mm LEDs (Any Color)](https://www.aliexpress.com/item/1005003912454852.html) | For the AM/PM indicators on each row. |
| 6 | [220-330Ω Resistors](https://www.aliexpress.com/item/1005002091320103.html) | Current-limiting resistors for the LEDs. |
| 1 set| [Dupont Jumper Wires](https://www.aliexpress.com/item/1005003641187997.html) | For connecting all components. |
| 1 | 5V Power Supply | A supply rated for at least **2A** is recommended. |

---
## 🔌 Wiring & Schematics

![schematic diagram](../images/bttf_bb.png)

This project uses two separate I2C buses to manage all 12 displays without address conflicts.

> #### 💡 ESP32-S3 Safe Pinout
> The following pinout is specifically for ESP32-S3 boards to avoid hardware conflicts with the built-in USB controller.

| Component | ESP32 Pin | Notes |
| :--- | :--- | :--- |
| **I2C Bus 1 (SDA)** | `GPIO 8` | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 1 (SCL)** | `GPIO 9` | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 2 (SDA)** | `GPIO 10` | Connects to the SDA pin of the 4 "Last Time Departed" row displays. |
| **I2C Bus 2 (SCL)** | `GPIO 11` | Connects to the SCL pin of the 4 "Last Time Departed" row displays. |
| **I2S DIN (Data)** | `GPIO 17` | Connects to the **DIN** pin of the MAX98357A. |
| **I2S BCLK (Bit Clock)**| `GPIO 16` | Connects to the **BCLK** pin of the MAX98357A. |
| **I2S LRC (Word Select)**|`GPIO 15` | Connects to the **LRC** pin of the MAX98357A. |
| **I2S SD (Shutdown)** | `GPIO 18` | Connects to the **SD** pin of the MAX98357A. |
| **Destination AM/PM**| `GPIO 13/14` | Connects to the anode (+) of the Destination row LEDs. |
| **Present AM/PM** | `GPIO 38/39` | Connects to the anode (+) of the Present row LEDs. |
| **Last Dept. AM/PM** | `GPIO 4/6` | Connects to the anode (+) of the Last Departed row LEDs. |
| **Power (+5V)** | `5V` | Connects to the VCC/VIN pin of all components. |
| **Ground (GND)** | `GND` | Connects all GND pins to a common ground rail. |

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

### **Step 1: Install Arduino IDE and ESP32 Core**
*   Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
*   Follow [these instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) to add the ESP32 board manager.

### **Step 2: Install Required Libraries**
*   **Install via Library Manager**: Open the Library Manager (`Sketch` > `Include Library` > `Manage Libraries...`). Search for and install the latest version of each of the following libraries:
    *   `Adafruit GFX Library`
    *   `Adafruit LED Backpack`
    *   `WiFiManager` by tzapu
    *   `ArduinoJson` by Benoit Blanchon (v6.x or v7.x)
    *   `ESPAsyncWebServer` by ESP32-Community
    *   `AsyncTCP` by ESP32-Community
    *   `PubSubClient` by Nick O'Leary

*   **Install Manually (Audio Library)**: The `ESP32-audioI2S` library is not available in the Library Manager and must be installed manually.
    1.  **[Download the library as a .zip file from the official repository](https://github.com/schreibfaul1/ESP32-audioI2S/archive/refs/heads/master.zip)**.
    2.  In the Arduino IDE, navigate to `Sketch` > `Include Library` > `Add .ZIP Library...`.
    3.  Select the downloaded `.zip` file. The library will be installed and ready to use.

### **Step 3: Set I2C Display Addresses**
> #### ⚠️ **Critical Step: Address Configuration**
> Each of the 12 display modules must have a unique I2C address. To set the address, you'll need to solder the address jumpers on the back of each display board. A "solder bridge" means connecting the two pads with a small blob of solder.
>
> **Note on I2C Buses**: This project uses two separate I2C buses. The "Destination" and "Present" displays are on one bus, and the "Last Time Departed" displays are on another. This is why some displays share the same address (e.g., 0x70) but don't conflict.
>
> Use the table below to configure each display. Failure to do this will result in displays not lighting up.
>
> > 💡 **Why can addresses be reused?**
> > I2C addresses only need to be unique *per bus*. Since the "Last Time Departed" displays are on a completely separate I2C bus from the other two rows, their addresses can safely overlap without causing conflicts.

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

### **Step 4: Set Partition Scheme**
> #### ⚠️ **Critical Step: Partition Scheme**
> This project requires a custom partition scheme to allocate enough space for the web interface and sound files. The necessary `partitions.csv` file is included in this repository.
>
> 1.  **Ensure `partitions.csv` is in the Sketch Directory**:
>     *   The `partitions.csv` file is located in the root of this repository.
>     *   This file **must** be in the same folder as the main `back-to-the-future-timecircuits.ino` file. The Arduino IDE will not detect it otherwise.
>
> 2.  **Select the Custom Scheme in Arduino IDE**:
>     *   If the Arduino IDE was already open, restart it.
>     *   Navigate to **Tools > Partition Scheme**.
>     *   Select **"Custom (partitions.csv)"** from the dropdown menu. If this option is not visible, the IDE cannot find the `partitions.csv` file in the sketch directory.
>
>     > 💡 **What this does**: This custom layout creates a large `littlefs` partition (aliased as `spiffs` for compatibility), which provides over 10MB of space for web files and sounds, while also allocating two large partitions for the main application. This enables both robust Over-the-Air (OTA) updates and ample storage.

### **Step 5: Upload Files to Filesystem**
> #### ⚠️ **Critical Step: Upload Data Files**
> The web interface and sound effects will not work unless you upload the contents of the `data` folder to the ESP32's filesystem. This process differs slightly between Arduino IDE v1.x and v2.x.
>
> ---
> #### **Instructions for Arduino IDE v2.x (Recommended)**
> 1.  Ensure the `data` folder is located in the same directory as your main `.ino` file.
> 2.  In the Arduino IDE, navigate to **Tools > ESP32 Sketch Data Upload**.
> 3.  The IDE will build and upload the filesystem image. No external plugin is required.
>
> ---
> #### **Instructions for Arduino IDE v1.x**
> 1.  **Install the Uploader Plugin**: You must first install the **ESP32 LittleFS Uploader** plugin.
>     *   Navigate to the [official plugin repository](https://github.com/lorol/arduino-esp32littlefs-plugin).
>     *   Follow the installation instructions to download and install the plugin into your Arduino IDE's `tools` directory.
> 2.  **Upload the Data**:
>     *   After installing the plugin, restart the Arduino IDE.
>     *   Open your sketch and navigate to **Tools > ESP32 Sketch Data Upload**.
>     *   This will upload the contents of your `data` folder to the ESP32.

### **Step 6: Upload the Main Code**
*   Open the `.ino` file in the Arduino IDE, select your board and COM port, and click "Upload".