# 🔬 Developer's Guide & Technical Deep Dive

This document provides a deeper look into the project's architecture, code structure, and development details for those looking to understand or modify the firmware.

## 🏗️ Project Structure

The firmware is organized into a modular structure to separate concerns and improve maintainability.

* **`back-to-the-future-timecircuits.ino`**: The main entry point of the application. Contains `setup()` and `loop()` and coordinates all other modules.
* **`HardwareControl.h`**: The hardware abstraction layer and central data definition file. It defines the core data structures for the project (e.g., `ClockSettings`, `WeatherData`) and all functions for direct interaction with the displays and LEDs.
* **`HardwareControl.cpp`**: The implementation file for the hardware abstraction layer. It utilizes the **Adafruit_LEDBackpack** and **Adafruit_GFX** libraries.
* **`EventManager.h`**: A global access header. It uses `extern` declarations to make global variables (like the `currentSettings` object) and system-wide objects (like the `mqttClient`) available to all other modules. This prevents circular dependency issues.
* **`AnimationManager.cpp / .h`**: Contains the logic for complex, multi-stage animations like the time travel sequence, boot-up, and glitch effects.
* **`DisplayManager.cpp / .h`**: Responsible for what is shown on the displays during normal operation (standard clock, weather, Data Link marquee).
* **`DataManager.cpp / .h`**: Handles all networking tasks for fetching and parsing data from external web APIs. Functions within this module are often run in dedicated FreeRTOS tasks to prevent blocking the main loop.
* **`MqttManager.cpp / .h`**: Manages the MQTT connection and all communication for the Home Assistant integration. It handles publishing states and subscribing to commands.
* **`web_server.cpp / .h`**: Sets up all the API endpoints and serves the web interface files using an asynchronous web server.

***

## ⚙️ Development Environment & Tooling

To get started with development, you will need the following:

* **Arduino IDE or PlatformIO**: The firmware is built for the ESP32 platform.
* **ESP32 Board Manager**: Add the ESP32 board manager to your IDE.
* **Libraries**: The following third-party libraries are required:
    * `Adafruit HT16K33 LED Backpack`
    * `Adafruit GFX Library`
    * `ArduinoJson`
    * `ESPAsyncWebServer`
    * AsyncTCP
    * `WiFiManager`
    * `PubSubClient`
    * `Preferences`
> **Note on Audio Library:** This project uses a forked version of the `ESP8266Audio` library that has been modified for this project's specific needs. It is included in this repository and does not need to be installed separately.

### Partitioning

The project uses a custom partition scheme to allocate more space for the filesystem (`LittleFS`), which stores web assets, sound files, and configuration data. The partition table is defined in `partitions.csv`.

### Over-The-Air (OTA) Updates

This project supports two methods for updating the firmware over the network, catering to different needs.

*   **Web UI Method (Recommended for Users)**: The web interface includes a secure endpoint for firmware updates. This allows you to flash new code to the ESP32 without a physical connection. Simply compile the new binary (`.bin` file), navigate to the `/update` endpoint in a browser, and upload the file. This method is password-protected for security.

*   **ArduinoOTA Method (Recommended for Developers)**: For faster development cycles, the project also supports `ArduinoOTA`. This allows you to upload new firmware directly from the Arduino IDE over the network. Once your computer is on the same WiFi network as the device, a network port will appear in the IDE, allowing for one-click uploads.

***

## 🔬 Technical Deep Dive

### Asynchronous, Non-Blocking by Design

The core of this project is a fully asynchronous, event-driven architecture, built on the ESP32's FreeRTOS operating system. This is crucial for a device with complex visual elements and real-time display updates.

* **The Problem with "Blocking" Code:** A simple approach to fetching web data is to make a request and wait for the response. On a microcontroller like the ESP32, this can be disastrous. If a remote server is slow, the entire device will freeze—animations will stutter, sounds will be delayed, and the device will feel unresponsive.
* **The Event-Driven Solution:** This project leverages the `ESPAsyncWebServer` library for non-blocking network operations and utilizes FreeRTOS tasks to offload time-consuming processes.
    * **Web Server:** The web server never blocks. It handles multiple connected clients simultaneously and uses callback functions to respond to requests.
    * **WebSocket Communication:** Real-time communication with the web UI is handled via WebSockets, allowing for a persistent, two-way channel without the overhead of repeated HTTP requests.
    * **Concurrency:** Outbound requests to external APIs (e.g., weather data, stock prices) are spawned in their own dedicated FreeRTOS tasks using `xTaskCreatePinnedToCore`. This isolates slow network operations from the main application loop, ensuring that even a 10-second API timeout has no impact on the smoothness of the display animations. A `Semaphore` (`xDisplayDataMutex`) is used to protect shared resources, such as the `currentWeatherData` and `displayPages` structs, from being corrupted by concurrent access from different tasks.

### Hardware & Display Management

* **Dual I2C Bus:** The HT16K33 display driver chip only allows for 8 unique addresses on a single bus. To control all 12 displays, the project cleverly splits them: 8 displays are on one I2C bus (`I2C_1`), and the remaining 4 are on a second I2C bus (`I2C_2`), avoiding the need for a more complex I2C multiplexer.
* **State Machine Logic:** The application's state is managed through several `enum` types (e.g., `AnimationPhase`, `MalfunctionPhase`) and handler functions in the main loop (`handleDisplayAnimation`, `handleMalfunction`, etc.). This creates a robust state machine where only one major display mode can be active at a time.
* **Audio Output:** Audio playback is managed by a dedicated FreeRTOS task to prevent stuttering. It uses the I2S peripheral and the custom-modified audio library included in this project to play MP3 files from the LittleFS filesystem. The `I2S_SD_PIN` is used to enable/disable the external amplifier to save power when no sound is playing.

### Handling SSL/TLS on the ESP32

Securely connecting to modern APIs via HTTPS (SSL/TLS) is one of the most memory-intensive operations a microcontroller can perform.

* **The Memory Challenge:** The ESP32 has limited RAM. Loading a server's full SSL certificate chain can consume a significant amount of this memory, which can lead to crashes.
* **The Solution: `client.setInsecure()`:** This project uses `client.setInsecure()` before making an HTTPS connection.
    * **What It Does**: It instructs the SSL/TLS engine to **skip the certificate validation step**. It does **not** disable encryption. The connection is still fully encrypted.
    * **Why It Works**: By skipping validation, the client avoids loading large root certificates into its limited RAM. This eliminates a common source of memory-related errors and greatly improves reliability for this application's purpose of fetching non-sensitive public data.

***

## 🤝 Contribution Guidelines

We welcome contributions to this project! Here is a simple workflow to get started:

1.  **Fork the Repository**: Create your own fork of the project on GitHub.
2.  **Create a Branch**: Create a new branch for your feature or bug fix: `git checkout -b feature/my-new-feature` or `bugfix/my-bug`.
3.  **Code and Commit**: Write your code and commit your changes with a clear and concise message.
4.  **Push to your Fork**: Push your new branch to your fork: `git push origin feature/my-new-feature`.
5.  **Create a Pull Request**: Open a pull request from your branch to the main repository's `main` branch. Provide a detailed description of your changes.