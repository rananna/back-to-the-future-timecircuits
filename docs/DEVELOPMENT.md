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
    * `AsyncTCP`
    * `WiFiManager`
    * `PubSubClient`
    * `Preferences`
    * `ESP32-audioI2S` by schreibfaul1
> **Note on Audio Library:** The audio library for this project, `ESP32-audioI2S`, can be found on GitHub. Please install it manually by downloading the repository and adding it to your Arduino libraries folder.
> **➡️ [ESP32-audioI2S Library](https://github.com/schreibfaul1/ESP32-audioI2S)**

### Partitioning

The project uses a custom partition scheme to allocate more space for the filesystem (`LittleFS`), which stores web assets, sound files, and configuration data. The partition table is defined in `partitions.csv`.

### Over-The-Air (OTA) Updates

This project supports multiple methods for updating the firmware and filesystem. For detailed, user-focused instructions on how to perform an update, please see the **[🚀 Updating Your Device](USAGE.md#-updating-your-device)** section in the main Usage Guide.

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
* **Audio Output:** Audio playback is managed by a dedicated FreeRTOS task to prevent stuttering. It uses the I2S peripheral and the **ESP32-audioI2S** library to play MP3 files from the LittleFS filesystem. The `I2S_SD_PIN` is used to enable/disable the external amplifier to save power when no sound is playing.

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

***

## 🌐 Frontend Web Interface

The web UI is a single-page application (SPA) served directly from the ESP32's LittleFS filesystem. It's built with vanilla JavaScript and communicates with the ESP32 via both a RESTful API and a persistent WebSocket connection.

### File Structure
*   **`index.html`**: The main HTML file that defines the structure of the page, including all the tabs and settings groups.
*   **`style.css`**: Contains all the styling for the web interface, including the different color themes.
*   **`data_handling.js`**: This script manages all communication with the ESP32. It initializes the WebSocket, handles incoming messages, and contains the functions for fetching data from the REST API endpoints (e.g., `/api/settings/temporal`).
*   **`main_ui.js`**: This script controls the user interface itself. It's responsible for initializing the UI on page load, populating dropdowns, applying fetched settings to the form fields, and attaching all the event listeners to buttons, sliders, and inputs.

### Communication Flow
The frontend uses a hybrid communication model for efficiency:

1.  **Initial Load (REST API)**: When the page first loads, `initializeUI()` in `main_ui.js` makes a series of `fetch` requests to the `/api/settings/*` endpoints. This pulls all the current settings from the device in one go.
2.  **Real-Time Updates (WebSocket)**: After the initial load, `initWebSocket()` in `data_handling.js` establishes a persistent WebSocket connection to the `/ws` endpoint on the ESP32. This connection is used for:
    *   **ESP32 to UI**: The ESP32 can push real-time state updates to the UI (e.g., confirming a setting was saved, providing progress on a file upload).
    *   **UI to ESP32**: The UI can send commands to the ESP32 that require a real-time response, such as testing an API key or a stock symbol. The `ws.onmessage` handler in `data_handling.js` listens for these responses and updates the UI accordingly.

### How to Add a New UI Element

If you wanted to add a new setting to the web interface, you would typically follow these steps:

1.  **Add the HTML**: Add the new input field (e.g., a slider, a text box, a checkbox) to the appropriate settings group in `index.html`. Give it a unique `id`.
2.  **Update `main_ui.js`**:
    *   In `applySettings()`, add logic to set the value of your new HTML element from the settings object fetched from the ESP32.
    *   In `attachEventListeners()`, add an event listener (`onchange` or `onclick`) to your new element. This listener should call `setSettingsChanged(true)` to enable the main save button.
3.  **Update `data_handling.js`**:
    *   In `saveSettings()`, add a line to read the value from your new HTML element and add it to the `settings` object that gets sent to the ESP32.
4.  **Update ESP32 Firmware**:
    *   Add the corresponding new setting to the `ClockSettings` struct in `HardwareControl.h`.
    *   In `web_server.cpp`, update the `/api/saveSettings` handler to parse the new setting from the incoming JSON and save it.
    *   Update the appropriate `/api/settings/...` GET endpoint to include your new setting so it can be loaded by the frontend.
    *   Update `saveSettings()` and `loadSettings()` in the main `.ino` file to persist your new setting to the device's non-volatile storage.