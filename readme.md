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
| **[🚀 Installation Guide](docs/INSTALLATION.md)** | **Start here!** This guide provides a complete, soup-to-nuts walkthrough for building the clock. It includes a full bill of materials, detailed wiring diagrams, 3D printing files, and step-by-step instructions for flashing the firmware. |
| **[💡 Usage Guide](docs/USAGE.md)** | Once you've built the clock, this guide explains how to connect to its web interface, configure WiFi, set your time zone, and use all of the day-to-day features. |

### **User Guides**
| Document | Description |
| :--- | :--- |
| **[📈 Data Link Guide](docs/guides/DATA_LINK.md)** | Transform the "Last Time Departed" display into a real-time data feed. This guide covers the built-in Weather and Stock modes, and explains how to create your own custom data integrations. |
| **[🏠 Home Assistant Guide](docs/HOME_ASSISTANT.md)** | For smart home enthusiasts, this guide details how to integrate the clock with Home Assistant. It covers the MQTT API, included blueprints, and provides examples for creating powerful automations. |
| **[🔄 Updating Guide](docs/UPDATING.md)** | This guide provides instructions on how to update the clock's firmware and web interface files to get the latest features and bug fixes. |

### **Advanced & Technical Reference**
| Document | Description |
| :--- | :--- |
| **[🔬 Developer's Guide](docs/DEVELOPMENT.md)** | A technical deep-dive into the project's architecture, code, and file system. This is the place to start if you're interested in contributing to the project or creating a custom fork. |

---

## 🚀 Getting Started

### Prerequisites

* Basic knowledge of soldering and electronics.
* Arduino IDE installed and configured for the ESP32.
* All components listed in the [Bill of Materials](docs/INSTALLATION.md#-️-bill-of-materials-bom).

### Installation

1.  **Hardware Assembly**: Follow the [Wiring & Schematics](docs/INSTALLATION.md#-wiring--schematics) to connect all the components.
2.  **Software Setup**: Flash the ESP32 with the firmware and upload the web interface files by following the [Software Installation](docs/INSTALLATION.md#-software-installation) guide.
3.  **Configuration**: Access the web interface to connect the device to your WiFi and start customizing!

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](https://github.com/your-username/your-repo/issues).

### How to Contribute

1.  Fork the repository.
2.  Create a new branch (`git checkout -b feature/your-feature`).
3.  Commit your changes (`git commit -am 'Add some feature'`).
4.  Push to the branch (`git push origin feature/your-feature`).
5.  Open a new Pull Request.

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.