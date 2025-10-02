# Back to the Future Time Circuits Replica

Welcome to the Back to the Future Time Circuits Replica project! This repository contains everything you need to build your own fully-functional, Wi-Fi connected, ESP32-powered replica of the iconic Time Circuits from the DeLorean Time Machine.

This isn't just a clock; it's a meticulously crafted, open-source project that blends nostalgia with modern technology. It's designed for hobbyists, enthusiasts, and anyone who's ever dreamed of hitting 88 mph.

![Time Circuits](images/bttf_bb.png)

## Getting Started

To build your own Time Circuits replica, follow these steps:

1.  **Gather the hardware:** See the [Bill of Materials](docs/reference/PINOUT.md) for the list of required components.
2.  **Assemble the circuit:** Follow the [hardware assembly instructions](docs/reference/PINOUT.md).
3.  **Install the firmware:** Use the [Installation Guide](docs/INSTALLATION.md) to flash the ESP32.
4.  **Configure your device:** Connect to the web interface to set up Wi-Fi and other options.

For complete, step-by-step instructions, please refer to the [**Full Installation Guide**](docs/INSTALLATION.md).

## Documentation

This project is extensively documented to help you from installation to advanced customization.

### User Guides

- **[Installation Guide](docs/INSTALLATION.md)**: Step-by-step instructions to get your Time Circuits up and running.
- **[Usage Guide](docs/USAGE.md)**: Learn how to use the web interface and basic features.
- **[Home Assistant Integration](docs/home-assistant.md)**: Detailed guide on integrating with Home Assistant.
- **[Updating Guide](docs/UPDATING.md)**: How to update your device to the latest firmware.
- **[Troubleshooting](docs/TROUBLESHOOTING.md)**: Solutions to common problems.

### Technical Reference

- **[Configuration](docs/reference/CONFIGURATION.md)**: Advanced configuration options.
- **[Sequencer API](docs/reference/SEQUENCER.md)**: Create custom animations using the powerful sequencer.
- **[Advanced MQTT](docs/reference/ADVANCED_MQTT.md)**: For advanced users who want to integrate with other systems.
- **[Hardware & Pinout](docs/reference/PINOUT.md)**: Detailed hardware specifications and pinout diagrams.
- **[Development Guide](docs/DEVELOPMENT.md)**: Information for developers who want to contribute to the project.

## Features

- **Iconic Design:** A faithful replica of the Time Circuits display, with three individual rows of 14-segment displays.
- **Cinematic Animations:** Includes the full boot-up sequence, time travel effects, and other iconic animations.
- **Real-Time Clock:** Keeps accurate time, synchronized over the internet.
- **Powerful Sequencer:** A flexible, built-in animation engine that allows you to create complex, multi-track animations.
- **Web Interface:** Easy-to-use web-based configuration for Wi-Fi, MQTT, and other settings.
- **MQTT Control:** Full control over the device via MQTT, allowing integration with any smart home system.
- **Home Assistant Integration:** Deep integration with Home Assistant, including auto-discovery and a rich set of script blueprints for creating custom automations and effects.
- **Sound Effects:** Optional support for an external speaker to play sound effects synchronized with animations.

## Home Assistant Integration

This project has deep integration with Home Assistant, allowing you to control the device, display sensor data, and run complex animation sequences.

### Sequencer Blueprints

To make creating custom animations as easy as possible, this project now includes a comprehensive library of Home Assistant script blueprints. These blueprints provide a user-friendly UI to generate animation sequences without needing to write any code.

**[Click here to view the documentation and available blueprints](./home_assistant/blueprints/README.md)**

This library includes blueprints for:
- Simple text and marquee effects
- Displaying sensor values and dynamic text from helpers
- Triggering built-in cinematic animations
- Advanced multi-track sequences

We highly recommend using these blueprints to get the most out of your device.