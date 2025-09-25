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

> **Great Scott!** You've found the schematics for a fully-functional, WiFi-enabled Time Circuits display. This project brings the iconic look, feel, and sounds of the DeLorean's dashboard right to your desk, complete with a slick web interface and deep integration with Home Assistant.

---

### 📚 Overview

This project is a faithful, DIY replica of the iconic Time Circuits from the *Back to the Future* movies. Built using a powerful ESP32 microcontroller, it’s not just a static prop—it's a fully functional, network-connected clock that brings the magic of the DeLorean's dashboard to your desk.

At its core, the project runs on a single firmware file, **`back-to-the-future-timecircuits.ino`**, which manages everything from the vibrant, screen-accurate display animations to the web-based interface that gives you complete control. Whether you're setting the Destination Time, checking the current weather, or tracking your stocks, it’s all handled by the ESP32.

The firmware is designed to be both powerful and flexible, with key features like:

*   **Real-Time Clock:** Keeps perfect time by syncing with network time servers, with full support for time zones.
*   **Dynamic Data Displays:** The bottom "Last Time Departed" display can be transformed to show live data from the web, including weather forecasts and a real-time stock ticker.
*   **Cinematic Sequences:** A built-in sequencer allows for scripted events, recreating the famous time-travel effects from the movies with coordinated lights, sounds, and animations.
*   **Persistent Memory:** All your settings—from WiFi credentials to your favorite destination times—are saved on the device, so it remembers everything even after being unplugged.

---

## 🌟 Features

This project is more than just a clock; it's a feature-packed, interactive prop designed for fans and makers alike.

* **Authentic Three-Row Display**: Destination Time, Present Time, and Last Time Departed.
* **Accurate & Automatic Time**: Full time zone support with automatic NTP synchronization.
* **Cinematic Animations & Sound**: Screen-accurate time travel sequences, boot-up animations, and random glitch effects with iconic movie sounds.
* **Advanced Web Interface**: A mobile-friendly UI for full configuration and control.
* **Deep Home Assistant Integration**: "Headless" control of all features, from display text to animations, plus powerful blueprints to get you started.
* **Data Link**: Display real-time data from any web API or your smart home. This feature takes over the bottom display row, which normally shows the "Last Time Departed" information.
* **Stock Ticker Mode**: A dedicated mode for displaying real-time stock information on the bottom display row.
* **Weather Display Mode**: A separate mode to show live weather conditions on the bottom display row.

---

## 📚 Documentation

This project's documentation is organized into categories to help you find what you need, whether you're building the clock for the first time or diving into advanced customizations.

### **Getting Started**
| Document | Description |
| :--- | :--- |
| **[🚀 Installation Guide](docs/INSTALLATION.md)** | **Start here!** A complete, soup-to-nuts guide to building the clock. It includes a full bill of materials, wiring diagrams, 3D printing files, and firmware flashing instructions. |
| **[💡 Usage Guide](docs/USAGE.md)** | Once you've built the clock, this guide explains how to connect to its web interface, configure WiFi, set your time zone, and use all of the day-to-day features. |
| **[🔄 Updating Guide](docs/UPDATING.md)** | Instructions on how to update the clock's firmware and web interface files to get the latest features and bug fixes. |

### **User Guides**
| Document | Description |
| :--- | :--- |
| **[📈 Data Link Guide](docs/guides/DATA_LINK.md)** | Transform the "Last Time Departed" display into a real-time data feed. This guide covers the built-in Weather and Stock modes and explains how to create your own custom data integrations. |
| **[🏠 Home Assistant Guide](docs/HOME_ASSISTANT.md)** | For smart home enthusiasts, this guide details how to integrate the clock with Home Assistant. It covers the MQTT API, included blueprints, and provides examples for creating powerful automations. |

### **Troubleshooting**
| Document | Description |
| :--- | :--- |
| **[🔬 Troubleshooting Guide](docs/TROUBLESHOOTING.md)** | Solutions to common hardware, software, and connectivity problems. |

### **Advanced & Technical Reference**
| Document | Description |
| :--- | :--- |
| **[🔬 Developer's Guide](docs/DEVELOPMENT.md)** | A technical deep-dive into the project's architecture, code, and file system. This is the place to start if you're interested in contributing to the project or creating a custom fork. |
| **[🔌 Home Assistant Advanced Guide](docs/guides/HOME_ASSISTANT_ADVANCED.md)** | An advanced guide for Home Assistant users. It covers creating complex automations, custom dashboard cards, and using template sensors to display custom data. |
| **[✉️ MQTT Technical Reference](docs/reference/MQTT_ENTITIES.md)** | A comprehensive reference for all MQTT topics and payloads used by the clock. Essential for advanced integrations and troubleshooting. |
| **[⚙️ Configuration Reference](docs/reference/CONFIGURATION.md)** | A complete reference for all firmware (compile-time) and web UI configuration options. |
| **[🔌 Pinout Reference](docs/reference/PINOUT.md)** | A detailed guide to the ESP32 GPIO pin assignments and instructions for customizing them. |

---

## 🚀 Getting Started

This project is designed for makers and hobbyists who are comfortable with basic electronics and soldering. The assembly process is straightforward, but it requires careful attention to detail, especially when wiring the 12 individual displays.

For a successful build, you will need to gather all the required components, 3D print the case, and flash the firmware. The entire process is detailed in the guides below, but here’s a high-level overview of what to expect.

### **Prerequisites**

Before you begin, make sure you have the following:

*   **Hardware & Tools:**
    *   All components listed in the **[Bill of Materials](docs/INSTALLATION.md#-️-bill-of-materials-bom)**.
    *   A soldering iron and basic soldering tools.
    *   Access to a 3D printer for the enclosure.
*   **Software:**
    *   The latest version of the **[Arduino IDE](https.www.arduino.cc/en/software)** installed.
    *   The ESP32 Board Manager configured in the Arduino IDE.
    *   A Git client to clone this repository.

### **Process at a Glance**

1.  **Assemble the Electronics**: Following the **[Wiring & Schematics](docs/INSTALLATION.md#-wiring--schematics)**, you’ll connect the ESP32, I2S amplifier, and all 12 of the 14-segment displays. This is the most time-consuming part of the build, so take your time and double-check your connections.

2.  **Flash the Firmware**: You’ll need to install several libraries in the Arduino IDE, set the correct partition scheme, and upload the main `.ino` firmware file. This step brings the clock to life.

3.  **Upload the Data Files**: The project's web interface and sound files must be uploaded to the ESP32’s internal storage. This is done using a special plugin for the Arduino IDE.

4.  **Final Assembly**: With the electronics wired and the firmware flashed, the final step is to mount everything inside the **[3D Printed Case](docs/INSTALLATION.md#-3d-printed-case--assembly)**.

Ready to get started? The **[Installation Guide](docs/INSTALLATION.md)** provides a complete, soup-to-nuts walkthrough of this entire process.

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](../../issues).

### How to Contribute

1.  Fork the repository.
2.  Create a new branch (`git checkout -b feature/your-feature`).
3.  Commit your changes (`git commit -am 'Add some feature'`).
4.  Push to the branch (`git push origin feature/your-feature`).
5.  Open a new Pull Request.

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.