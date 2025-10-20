# 🔌 Hardware & Pinout Guide

This document provides a complete reference for the GPIO pin assignments used in this project.

![schematic diagram](../images/bttf_bb.png)

## Component Wiring Table (ESP32-S3 Safe Pinout)

> **Note:** The following pinout is specifically for ESP32-S3 boards to avoid hardware conflicts with the built-in USB controller.

| Component | ESP32 Pin | Suggested Wire Color | Connection / Notes |
| :--- | :--- | :--- | :--- |
| **I2C Bus 1 (SDA)** | `GPIO 8` | Yellow | Connects to the SDA pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 1 (SCL)** | `GPIO 9` | Green | Connects to the SCL pin of the 8 "Destination" and "Present" row displays. |
| **I2C Bus 2 (SDA)** | `GPIO 10` | Blue | Connects to the SDA pin of the 4 "Last Time Departed" row displays. |
| **I2C Bus 2 (SCL)** | `GPIO 11` | White | Connects to the SCL pin of the 4 "Last Time Departed" row displays. |
| **I2S DIN (Data)** | `GPIO 17` | Gray | Connects to the **DIN** pin of the MAX98357A. |
| **I2S BCLK (Bit Clock)**| `GPIO 16` | Orange | Connects to the **BCLK** pin of the MAX98357A. |
| **I2S LRC (Word Select)**|`GPIO 15` | Purple | Connects to the **LRC** pin of the MAX98357A. |
| **I2S SD (Shutdown)** | `GPIO 18` | Brown | Connects to the **SD** pin of the MAX98357A. |
| **Destination AM LED**| `GPIO 13` | | Connects to the anode (+) of the AM LED for the Destination row. |
| **Destination PM LED**| `GPIO 14` | | Connects to the anode (+) of the PM LED for the Destination row. |
| **Present AM LED** | `GPIO 38` | | Connects to the anode (+) of the AM LED for the Present row. |
| **Present PM LED** | `GPIO 39` | | Connects to the anode (+) of the PM LED for the Present row. |
| **Last Dept. AM LED** | `GPIO 1` | | Connects to the anode (+) of the Last Departed row LED. |
| **Last Dept. PM LED** | `GPIO 2` | | Connects to the anode (+) of the Last Departed row LED. |
| **Power (+5V)** | `5V` | Red | Connects to the VCC/VIN pin of all components. |
| **Ground (GND)** | `GND` | Black | Connects all GND pins to a common ground rail. |
