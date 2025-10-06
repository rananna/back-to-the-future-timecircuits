# Back to the Future Time Circuits Replica

Welcome to the Back to the Future Time Circuits Replica project! This repository contains everything you need to build your own fully-functional, Wi-Fi connected, ESP32-powered replica of the iconic Time Circuits from the DeLorean Time Machine.

This isn't just a clock; it's a meticulously crafted, open-source project that blends nostalgia with modern technology. It's designed for hobbyists, enthusiasts, and anyone who's ever dreamed of hitting 88 mph.

![Time Circuits](images/bttf_bb.png)

## Features

-   **Faithful Replica**: A true-to-the-movies design with three rows of 14-segment displays and iconic AM/PM indicators.
-   **Cinematic Boot Sequence**: A full, multi-stage boot-up sequence with synchronized sounds and visuals.
-   **Powerful Animation Sequencer**: A flexible, built-in animation engine that allows you to create complex, multi-track animations with over 20 different commands.
-   **Comprehensive Web UI**: An easy-to-use web interface for configuration, preset management, sound control, and triggering animations.
-   **Full MQTT Control**: A low-level MQTT API that exposes every feature for integration with any smart home system.
-   **Native Home Assistant Integration**: Seamless integration with Home Assistant, featuring powerful **Blueprints** that make creating custom animations and notifications incredibly simple.
-   **Integrated Sound System**: Plays built-in sound effects, streams internet radio, and works with Home Assistant's Text-to-Speech (TTS) services.
-   **Data Display Modes**: Display real-time stock quotes, live weather forecasts, or custom marquee messages on the displays.
-   **Over-the-Air (OTA) Updates**: Update the firmware directly from the web interface or Home Assistant.

---

## Getting Started

For complete, step-by-step instructions on building and installing the project, please see the **[Full Installation Guide](docs/getting-started/installation.md)**.

1.  **Hardware & Assembly:** Start with the [Hardware & Pinout Guide](docs/getting-started/hardware.md).
2.  **Firmware Installation:** Follow the [Firmware Installation Guide](docs/getting-started/installation.md).
3.  **Firmware Updates:** Learn how to [Update Your Firmware](docs/getting-started/updating.md).

---

## Documentation

This project is extensively documented. Once you're set up, explore the guides to get the most out of your Time Circuits.

### **User Guides**
-   **[Web Interface Guide](docs/user-guide/web-interface.md)**: Learn how to use the web UI for configuration.
-   **[Home Assistant Integration](docs/user-guide/home-assistant.md)**: A detailed guide on integrating with Home Assistant.
-   **[Home Assistant Blueprints](home_assistant/blueprints/README.md)**: Get started quickly with our easy-to-use automation blueprints.
-   **[Sound System Guide](docs/user-guide/sound-system.md)**: A guide to the sound system and Text-to-Speech features.
-   **[Troubleshooting Guide](docs/user-guide/troubleshooting.md)**: Solutions to common problems.

### **Developer & Advanced Topics**
-   **[Developer Guide](docs/developer/developer-guide.md)**: A technical deep-dive into the firmware architecture.
-   **[Sequencer API Reference](docs/developer/sequencer-api.md)**: Create custom animations using the powerful sequencer.
-   **[Raw MQTT API Reference](docs/developer/mqtt-api.md)**: A complete reference for the low-level MQTT API.

---

## 🏠 Home Assistant Integration

This project is designed for a seamless and powerful Home Assistant experience. The easiest way to get started is by using our **pre-built blueprints**, which provide a simple UI for creating custom automations without any code.

The integration also creates a device with dozens of entities, a media player for all audio, and custom notification services for more advanced control.

**➡️ To get started, see the [Home Assistant Integration Guide](docs/user-guide/home-assistant.md).**

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