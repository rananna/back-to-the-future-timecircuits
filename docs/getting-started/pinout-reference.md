# 🔌 Pinout Reference

This document provides a complete reference for the GPIO pin assignments used in this project.

## Official ESP32-S3 Pinout

The following pinout is the official, recommended configuration for this project, specifically designed to be safe for **ESP32-S3** boards. The S3 is recommended because its flexible pin mapping avoids conflicts with the built-in USB/JTAG controller.

| Component | ESP32 Pin | Bus / Group | Notes |
| :--- | :--- | :--- | :--- |
| **I2C Bus 1 (SDA)** | `GPIO 8` | I2C Bus 1 | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 1 (SCL)** | `GPIO 9` | I2C Bus 1 | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 2 (SDA)** | `GPIO 10` | I2C Bus 2 | Connects to the SDA pin of the 4 "Last Time Departed" row displays. |
| **I2C Bus 2 (SCL)** | `GPIO 11` | I2C Bus 2 | Connects to the SCL pin of the 4 "Last Time Departed" row displays. |
| **I2S DIN (Data)** | `GPIO 17` | I2S Audio | Connects to the **DIN** pin of the MAX98357A amplifier. |
| **I2S BCLK (Bit Clock)**| `GPIO 16` | I2S Audio | Connects to the **BCLK** pin of the MAX98357A. |
| **I2S LRC (Word Select)**|`GPIO 15` | I2S Audio | Connects to the **LRC** pin of the MAX98357A. |
| **I2S SD (Shutdown)** | `GPIO 18` | I2S Audio | Connects to the **SD** (Shutdown) pin of the MAX98357A. |
| **Destination AM LED**| `GPIO 13` | AM/PM LEDs | Connects to the anode (+) of the Destination row "AM" LED. |
| **Destination PM LED**| `GPIO 14` | AM/PM LEDs | Connects to the anode (+) of the Destination row "PM" LED. |
| **Present AM LED** | `GPIO 38` | AM/PM LEDs | Connects to the anode (+) of the Present row "AM" LED. |
| **Present PM LED** | `GPIO 39` | AM/PM LEDs | Connects to the anode (+) of the Present row "PM" LED. |
| **Last Dept. AM LED** | `GPIO 4` | AM/PM LEDs | Connects to the anode (+) of the Last Departed row "AM" LED. |
| **Last Dept. PM LED** | `GPIO 6` | AM/PM LEDs | Connects to the anode (+) of the Last Departed row "PM" LED. |
| **Power (+5V)** | `5V` | Power | Connects to the VCC/VIN pin of all components. |
| **Ground (GND)** | `GND` | Power | Connects all GND pins to a common ground rail. |

## Using Other ESP32 Models

> [!NOTE]
> While the **ESP32-S3 is strongly recommended** for a trouble-free build, it is possible to use other ESP32 models (like the original ESP32-WROOM-32).
>
> If you choose to use a different board, you **must** review and change the pin assignments defined at the top of the `HardwareControl.h` firmware file. You will need to select pins that are safe to use on your specific board and do not conflict with other hardware functions (like ADC2 pins when WiFi is active). This is an advanced modification and is not recommended for beginners.