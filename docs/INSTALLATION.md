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
| 1 | [ESP32-S3 Dev Module](https://www.aliexpress.com/item/1005006212080137.html) | A **38-pin** module is required. An S3 model is recommended for its safe pinout, which avoids conflicts with the built-in USB controller. ([Alternate](https://www.sparkfun.com/products/24408)) |
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
The software installation process involves six key steps, from setting up the Arduino IDE to uploading the final firmware. Follow the steps below in order.

<br>

<details>
<summary><b>Step 1: Install Arduino IDE and ESP32 Core</b></summary>

*   First, download and install the latest version of the [Arduino IDE](https://www.arduino.cc/en/software).
*   Next, add support for ESP32 microcontrollers by following the official [installation instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) from Espressif.

</details>

<details>
<summary><b>Step 2: Install Required Libraries</b></summary>

This project relies on several external libraries. Seven of them can be installed directly from the Arduino Library Manager, but one must be installed manually.

*   **Install via Library Manager**:
    *   Open the Library Manager by navigating to `Sketch` > `Include Library` > `Manage Libraries...`.
    *   Search for and install the latest version of each of the following libraries:
        *   `Adafruit GFX Library`
        *   `Adafruit LED Backpack`
        *   `WiFiManager` by tzapu
        *   `ArduinoJson` by Benoit Blanchon (v6.x or v7.x)
        *   `ESPAsyncWebServer` by ESP32-Community
        *   `AsyncTCP` by ESP32-Community
        *   `PubSubClient` by Nick O'Leary

*   **Install Manually (Audio Library)**:
    *   The `ESP32-audioI2S` library is not available in the Library Manager and must be installed from a `.zip` file.
    1.  **[Download the library from the official repository](https://github.com/schreibfaul1/ESP32-audioI2S/releases/tag/3.4.2)**.
    2.  In the Arduino IDE, navigate to `Sketch` > `Include Library` > `Add .ZIP Library...`.
    3.  Select the downloaded `.zip` file to complete the installation.

</details>

<details>
<summary><b>Step 3: Set I2C Display Addresses</b></summary>

> #### ⚠️ **Critical Step: Address Configuration**
> Each of the 12 display modules must be configured with a unique I2C address so the firmware can communicate with it. This is done by creating "solder bridges" on the address jumpers on the back of each display's circuit board. A solder bridge is simply a small blob of solder that connects the two pads.
>
> <img src="https://i.ytimg.com/vi/AOkdQ0txKpA/maxresdefault.jpg" width="400">
>
> **How it Works**: This project uses two separate I2C buses to avoid conflicts. The "Destination" and "Present" displays are on one bus, while the "Last Time Departed" displays are on another. Because they are on separate buses, their addresses can overlap without causing issues.
>
> > [!IMPORTANT]
> > This is the most critical step of the build. If the addresses are not set correctly, the displays will not work. Take your time and double-check your soldering.
>
> Use the table below to carefully configure the solder jumpers for each of the 12 displays.

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

</details>

<details>
<summary><b>Step 4: Set Partition Scheme</b></summary>

> #### ⚠️ **Critical Step: Partition Scheme**
> A custom partition scheme is required to allocate enough space for the web interface and sound files. The `partitions.csv` file, which defines this layout, is included in the repository.
>
> 1.  **Confirm File Location**:
>     *   The `partitions.csv` file **must** be in the same folder as the main `back-to-the-future-timecircuits.ino` file. The Arduino IDE will not detect it otherwise.
>
> 2.  **Select Custom Scheme**:
>     *   Restart the Arduino IDE to ensure it detects the new file.
>     *   Navigate to **Tools > Partition Scheme**.
>     *   Select **"Custom (partitions.csv)"** from the dropdown menu.
>
>     > [!IMPORTANT]
>     > If the "Custom (partitions.csv)" option is not visible, it means the IDE was not able to find the `partitions.csv` file. Ensure the file is in the same directory as the `.ino` file and that you have restarted the IDE.
>
>     > 💡 **What this does**: This custom layout creates a large `littlefs` partition (aliased as `spiffs` for compatibility), which provides over 10MB of space for web files and sounds. It also allocates two large partitions for the main application, enabling robust Over-the-Air (OTA) updates.

</details>

<details>
<summary><b>Step 5: Upload Files to Filesystem</b></summary>

> #### ⚠️ **Critical Step: Upload Data Files**
> The web interface and sound effects will not work unless the contents of the `data` folder are uploaded to the ESP32's filesystem. The process differs slightly between Arduino IDE versions.
>
> ---
> #### **Instructions for Arduino IDE v2.x (Recommended)**
> 1.  **Install the Uploader Plugin**:
>     *   Download the `arduino-littlefs-upload` plugin from the [official releases page](https://github.com/earlephilhower/arduino-littlefs-upload/releases).
>     *   Follow the installation instructions on that page to add the plugin to your Arduino IDE.
> 2.  **Upload the Data**:
>     *   Ensure the `data` folder is located in the same directory as your main `.ino` file.
>     *   In the Arduino IDE, navigate to **Tools > ESP32 LittleFS Data Upload**.
>     *   The IDE will build and upload the filesystem image automatically.
>
> ---
> #### **Instructions for Arduino IDE v1.x**
> 1.  **Install the Uploader Plugin**:
>     *   Download and install the **ESP32 LittleFS Uploader** from the [official plugin repository](https://github.com/lorol/arduino-esp32littlefs-plugin). Follow the installation instructions carefully.
> 2.  **Upload the Data**:
>     *   After installing the plugin, restart the Arduino IDE.
>     *   Open your sketch and navigate to **Tools > ESP32 Sketch Data Upload**.

</details>

<details>
<summary><b>Step 6: Upload the Main Code</b></summary>

*   With all the prerequisites in place, you can now upload the main firmware.
*   Open the `back-to-the-future-timecircuits.ino` file in the Arduino IDE.
*   Select your ESP32 board model and the correct COM port from the **Tools** menu.
*   Click the **Upload** button to flash the firmware.

</details>