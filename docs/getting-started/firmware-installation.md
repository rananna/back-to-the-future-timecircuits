# Firmware Installation Guide

This guide covers the complete software installation process for your Time Circuits clock.

## Software Installation
The software installation process is broken down into four main parts: downloading the code, setting up your environment, flashing the firmware, and initial device configuration. Follow these steps in order.

### **Part 1: Download the Project Files**

1.  **Go to the GitHub Repository**:
    *   Navigate to the main project page: [https://github.com/rananna/back-to-the-future-timecircuits](https://github.com/rananna/back-to-the-future-timecircuits)
2.  **Download the Code**:
    *   Click the green **`< > Code`** button.
    *   Select **`Download ZIP`**.
    *   Unzip the downloaded file to a location on your computer where you keep your Arduino projects.

### **Part 2: Set Up Your Environment**

<details>
<summary><b>Step 1: Install Arduino IDE and ESP32 Core</b></summary>

*   First, download and install the latest version of the [Arduino IDE](https://www.arduino.cc/en/software).
*   Next, add support for ESP32 microcontrollers by following the official [installation instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) from Espressif.

</details>

<details>
<summary><b>Step 2: Install Required Libraries</b></summary>

This project relies on several external libraries that can be installed directly from the Arduino Library Manager.

*   **How to Install**:
    1.  In the Arduino IDE, navigate to `Sketch` > `Include Library` > `Manage Libraries...`.
    2.  Use the search bar to find and install the latest version of each of the following libraries:

*   **Required Libraries**:
    *   `Adafruit GFX Library`
    *   `Adafruit LED Backpack`
    *   `WiFiManager` by tzapu
    *   `ArduinoJson` by Benoit Blanchon (**v7.x recommended**)
    *   `ESPAsyncWebServer` by ESP32-Community
    *   `AsyncTCP` by ESP32-Community
    *   `PubSubClient` by Nick O'Leary
    *   `ESP32-audioI2S` by schreibfaul

</details>

<details>
<summary><b>Step 3: Install the Filesystem Uploader Plugin</b></summary>

> [!WARNING]
> **Critical Step: Install Uploader Plugin**
> The web interface and sound effects will not work unless the contents of the `data` folder are uploaded to the ESP32's filesystem. This requires a special uploader plugin for the Arduino IDE.
>
> 1.  **Download the Correct Plugin**:
>     *   **For Arduino IDE v2.x (Recommended)**: Download the `arduino-littlefs-upload` plugin from its [official releases page](https://github.com/earlephilhower/arduino-littlefs-upload/releases).
>     *   **For Arduino IDE v1.x**: Download the `ESP32FS` plugin from its [official repository](https://github.com/me-no-dev/arduino-esp32fs-plugin).
>
> 2.  **Install the Plugin**:
>     *   Follow the installation instructions provided on the respective download pages to add the plugin to your Arduino IDE.
>     *   **Restart the Arduino IDE** after installing the plugin to ensure it is loaded correctly.

</details>

### **Part 3: Flash the Firmware**

<details>
<summary><b>Step 1: Set I2C Display Addresses</b></summary>

> [!WARNING]
> **Critical Step: Address Configuration**
> Each of the 12 display modules must be configured with a unique I2C address so the firmware can communicate with it. This is done by creating "solder bridges" on the address jumpers on the back of each display's circuit board. A solder bridge is simply a small blob of solder that connects the two pads.
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
<summary><b>Step 2: Configure Arduino IDE and Upload</b></summary>

1.  **Open the Project**:
    *   Open the `back-to-the-future-timecircuits.ino` file in the Arduino IDE.

2.  **Configure Board Settings**:
    *   Navigate to the **Tools** menu and configure the following:
        *   **Board**: Select your specific ESP32-S3 board model.
        *   **Port**: Select the COM port your ESP32 is connected to.
        *   **Partition Scheme**: Select **"Custom (partitions.csv)"**.

    > [!IMPORTANT]
    > If the "Custom (partitions.csv)" option is not visible, it means the IDE did not find the `partitions.csv` file. Ensure the file is in the same directory as the `.ino` file and that you have restarted the IDE.

3.  **Upload the Filesystem**:
    *   Navigate to **Tools > ESP32 LittleFS Data Upload** (the exact name may vary).
    *   This will build and upload the `data` folder to the ESP32.

4.  **Upload the Main Firmware**:
    *   Click the **Upload** button (the arrow icon) to flash the main firmware.

> ***Image Placeholder:*** *A screenshot of the Arduino IDE's Tools menu, highlighting the "Partition Scheme" and "ESP32 LittleFS Data Upload" options.*

</details>

---

## 🎉 First Boot & Configuration

After successfully flashing the firmware and filesystem, the device is ready for its first boot. For this process, please see the **[Wi-Fi Setup Guide](./wifi-setup.md)**.
