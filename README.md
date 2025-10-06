# Back to the Future Time Circuits Replica

Welcome to the Back to the Future Time Circuits Replica project! This repository contains everything you need to build your own fully-functional, Wi-Fi connected, ESP32-powered replica of the iconic Time Circuits from the DeLorean Time Machine.

This isn't just a clock; it's a meticulously crafted, open-source project that blends nostalgia with modern technology. It's designed for hobbyists, enthusiasts, and anyone who's ever dreamed of hitting 88 mph.

![Time Circuits](images/bttf_bb.png)

## Features

- **Iconic Design:** A faithful replica of the Time Circuits display, with three individual rows of 14-segment displays.
- **Cinematic Animations:** Includes the full boot-up sequence, time travel effects, and other iconic animations.
- **Real-Time Clock:** Keeps accurate time, synchronized over the internet.
- **Powerful Sequencer:** A flexible, built-in animation engine that allows you to create complex, multi-track animations.
- **Web Interface:** Easy-to-use web-based configuration for Wi-Fi, MQTT, and other settings.
- **MQTT Control:** Full control over the device via MQTT, allowing integration with any smart home system.
- **Home Assistant Integration:** Deep integration with Home Assistant, including auto-discovery and a rich set of script blueprints for creating custom automations and effects.
- **Sound Effects:** Optional support for an external speaker to play sound effects synchronized with animations.

## Getting Started

For complete, step-by-step instructions on building and installing the project, please see the **[Full Installation Guide](docs/getting-started/installation.md)**.

1.  **Hardware & Assembly:** Start with the [Bill of Materials & Hardware Guide](docs/getting-started/hardware.md).
2.  **Firmware Installation:** Follow the [Firmware Installation Guide](docs/getting-started/installation.md).
3.  **Firmware Updates:** Learn how to [Update Your Firmware](docs/getting-started/updating.md).

## Documentation

This project is extensively documented. Once you're set up, explore the guides to get the most out of your Time Circuits.

### User Guide
- **[Web Interface](docs/user-guide/web-interface.md)**: Learn how to use the web UI for configuration.
- **[Sound System & TTS](docs/user-guide/sound-system.md)**: A guide to the sound system and Text-to-Speech features.
- **[Home Assistant Integration](docs/user-guide/home-assistant.md)**: A detailed guide on integrating with Home Assistant.
- **[Troubleshooting](docs/user-guide/troubleshooting.md)**: Solutions to common problems.

### Developer & Advanced Topics
- **[Sequencer API](docs/developer/sequencer-api.md)**: Create custom animations using the powerful sequencer.
- **[MQTT API](docs/developer/mqtt-api.md)**: A complete reference for the MQTT API.
- **[Configuration Files](docs/developer/configuration.md)**: Details on advanced configuration files.
- **[Contributing Guide](docs/developer/contributing.md)**: Information for developers who want to contribute.

## Home Assistant Integration

This project has deep integration with Home Assistant, allowing you to control the device, display sensor data, and run complex animation sequences.

### Sequencer Blueprints

To make creating custom animations as easy as possible, this project includes a comprehensive library of Home Assistant script blueprints. These blueprints provide a user-friendly UI to generate complex animation sequences without needing to write any code.

**[Click here to view the full list of blueprints and their documentation.](./home_assistant/blueprints/README.md)**

We highly recommend using these blueprints to get the most out of your device.