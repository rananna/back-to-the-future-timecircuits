# Back to the Future Time Circuits Replica

Welcome to the Back to the Future Time Circuits Replica project! This repository contains everything you need to build your own fully-functional, Wi-Fi connected, ESP32-powered replica of the iconic Time Circuits from the DeLorean Time Machine.

This isn't just a clock; it's a meticulously crafted, open-source project that blends nostalgia with modern technology. It's designed for hobbyists, enthusiasts, and anyone who's ever dreamed of hitting 88 mph.

![Time Circuits](images/bttf_bb.png)

## Features

-   **Screen-Accurate Replica**: A true-to-the-movies design with three rows of 14-segment displays, iconic AM/PM indicators, and a cinematic, multi-stage boot sequence with synchronized sound effects.
-   **Powerful Animation Engine**: A flexible, built-in sequencer that allows you to create complex, multi-track animations with over 25 different commands. Trigger one of the many built-in animations or create your own.
-   **Full Smart Home Integration**: Native Home Assistant integration with powerful **Blueprints** makes creating custom automations and notifications incredibly simple. A low-level MQTT API is also available for full control from any smart home system.
-   **Dynamic Data Displays**: Go beyond the clock and display real-time stock quotes, live weather forecasts from a custom API, or your own custom messages using the "Data Link" marquee mode.
-   **Comprehensive Web Interface**: An easy-to-use, mobile-friendly web UI for configuration, preset management, sound control, and triggering animations.
-   **Integrated Audio System**: Plays built-in sound effects, streams internet radio, and works with Home Assistant's Text-to-Speech (TTS) services to announce notifications.
-   **Modern Tech Stack**: Built on a fully asynchronous, non-blocking architecture using an ESP32, with support for Over-the-Air (OTA) firmware updates directly from the web interface or Home Assistant.

---

## Documentation

This project is extensively documented. The guides are structured to walk you from initial setup to advanced customization.

### **1. Getting Started**
First-time setup and installation guides.

-   **[Hardware & Pinout Guide](docs/getting-started/hardware.md)**: A list of required components and wiring diagrams.
-   **[Firmware Installation Guide](docs/getting-started/installation.md)**: Step-by-step instructions for flashing the firmware.
-   **[Updating Guide](docs/getting-started/updating.md)**: Learn how to update the firmware and filesystem.

### **2. User Guides**
Learn how to use the features of your Time Circuits.

-   **[Web Interface Guide](docs/user-guide/web-interface.md)**: A complete tour of the web UI for configuration.
-   **[Home Assistant Integration](docs/user-guide/home-assistant.md)**: A detailed guide on integrating with Home Assistant.
-   **[Home Assistant Blueprints](home_assistant/blueprints/README.md)**: **(Recommended for HA users)** Get started quickly with our easy-to-use automation blueprints.
-   **[Sound System Guide](docs/user-guide/sound-system.md)**: A guide to the sound system and Text-to-Speech features.
-   **[Troubleshooting Guide](docs/user-guide/troubleshooting.md)**: Solutions to common problems.

### **3. Developer & Advanced Topics**
For those who want to contribute or create advanced custom integrations.

-   **[Developer Guide](docs/developer/developer-guide.md)**: A technical deep-dive into the firmware architecture.
-   **[Sequencer API Reference](docs/developer/sequencer-api.md)**: Create custom animations using the powerful sequencer.
-   **[Raw MQTT API Reference](docs/developer/mqtt-api.md)**: A complete reference for the low-level MQTT API.

---

## 🏠 Home Assistant Integration

This project is designed for a seamless and powerful Home Assistant experience. The easiest way to get started is by using our **pre-built blueprints**, which provide a simple UI for creating custom automations without any code.

The integration also creates a device with dozens of entities, a media player for all audio, and custom notification services for more advanced control.

**➡️ To get started, see the [Home Assistant Integration Guide](docs/user-guide/home-assistant.md) and the [Blueprints README](home_assistant/blueprints/README.md).**

---
## 💬 Community & Support

Have a question, found a bug, or have a great idea for a new feature? We'd love to hear from you!

-   **Report a Bug:** If you've found a bug, please [open an issue](https://github.com/rananna/back-to-the-future-timecircuits/issues/new?template=bug_report.md) and provide as much detail as possible.
-   **Request a Feature:** If you have an idea for a new feature, please [open an issue](https://github.com/rananna/back-to-the-future-timecircuits/issues/new?template=feature_request.md) to start a discussion.
-   **Ask a Question:** For general questions and support, please [start a discussion](https://github.com/rananna/back-to-the-future-timecircuits/discussions) on our community forum.

---
## 🤝 Contributing

Contributions are welcome! Whether you're fixing a bug, adding a new feature, or improving the documentation, your help is appreciated.

Please read the **[Contributing Guide](CONTRIBUTING.md)** for details on how to get started.