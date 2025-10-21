# Back to the Future Time Circuits Replica

Welcome to the Back to the Future Time Circuits Replica project! This repository contains everything you need to build your own fully-functional, Wi-Fi connected, ESP32-powered replica of the iconic Time Circuits from the DeLorean Time Machine.

This isn't just a clock; it's a meticulously crafted, open-source project that blends nostalgia with modern technology. It's designed for hobbyists, enthusiasts, and anyone who's ever dreamed of hitting 88 mph.

![Time Circuits](images/bttf_bb.png)

## Documentation

**For all documentation, including build guides, user manuals, and developer information, please visit our central documentation hub:**

## ➡️ [View the Full Documentation](docs/README.md)

## Features

-   **Screen-Accurate Replica**: A true-to-the-movies design that captures the look and feel of the original, complete with a cinematic, multi-stage boot sequence with synchronized sound effects.

-   **Powerful Animation Engine**: A flexible, three-track sequencer that allows you to create complex, parallel animations. It supports a rich set of commands for text effects, visualizers, sound, and logic control. Animations can be triggered from the web UI, MQTT, or Home Assistant.

-   **Full Smart Home Integration**: Native Home Assistant support is at the core of this project.
    -   **Powerful Blueprints**: A suite of script-based blueprints for creating custom alerts, countdowns, and notifications without writing any code.
    -   **Unified `media_player`**: A single entity for controlling all audio—play sound effects, stream internet radio, or use it as a TTS target.
    -   **`notify` Service**: Send text directly to the displays from your automations.
    -   **One-Click Firmware Updates**: The `update` entity allows you to install the latest firmware directly from Home Assistant.
    -   **Dozens of Entities**: Full control over every device setting, including switches, selects, and number sliders.

-   **Integrated Audio System**: An onboard speaker and dedicated audio chip provide crisp, clear playback of built-in sound effects. The system can also stream audio directly from internet radio stations or other URLs.

-   **Dynamic Data Displays**: Don't just display the time—show what matters to you.
    -   **Stock Ticker**: Display real-time stock quotes from a public API.
    -   **Weather Station**: Show live local weather conditions and forecasts.
    -   **Custom Messages**: Create your own scrolling marquee messages.

-   **Comprehensive Web Interface**: An easy-to-use, mobile-friendly web UI for configuration, control, and real-time animation previews.

-   **Modern Tech Stack**: Built on a fully asynchronous, non-blocking architecture using an ESP32, C++, and modern JavaScript. It's designed for stability and performance, with support for Over-the-Air (OTA) firmware updates.

---
## 💬 Community & Support

Have a question, found a bug, or have a great idea for a new feature? We'd love to hear from you!

-   **Report a Bug:** If you've found a bug, please [open an issue](https://github.com/rananna/back-to-the-future-timecircuits/issues/new?template=bug_report.md) and provide as much detail as possible.
-   **Request a Feature:** If you have an idea for a new feature, please [open an issue](https://github.com/rananna/back-to-the-future-timecircuits/issues/new?template=feature_request.md) to start a discussion.

---
## 🤝 Contributing

Contributions are welcome! Whether you're fixing a bug, adding a new feature, or improving the documentation, your help is appreciated.

Please read the **[Contributing Guide](CONTRIBUTING.md)** for details on how to get started.