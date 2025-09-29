# 🔬 Developer's Guide & Technical Deep Dive

This document provides a deeper look into the project's architecture, code structure, and development details for those looking to understand or modify the firmware.

## 🏗️ Project Structure

The firmware is organized into a modular structure to separate concerns and improve maintainability.

*   **`back-to-the-future-timecircuits.ino`**: The main entry point of the application. Contains `setup()` and `loop()` and coordinates all other modules.
*   **`HardwareControl.h` / `.cpp`**: The hardware abstraction layer. Defines core data structures (`ClockSettings`, `WeatherData`) and all functions for direct interaction with the displays and LEDs using the **Adafruit_LEDBackpack** and **Adafruit_GFX** libraries.
*   **`DisplayManager.h` / `.cpp`**: Responsible for what is shown on the displays during normal operation (standard clock, weather, Data Link marquee).
*   **`AnimationManager.h` / `.cpp`**: Manages the execution of animations. It contains the handlers for the hardcoded cinematic time travel and boot sequences, and it also contains the main `handleSequencer` function, which drives all other animations.
*   **`AnimationSequences.h` / `.cpp`**: The animation library for the clock. This file contains the logic for generating sequencer scripts for all the pre-canned "styled animations" (e.g., "Tornado Flicker", "Code Breaker"). To create a new animation, you add its generator function here.
*   **`DataManager.h` / `.cpp`**: Handles all networking tasks for fetching and parsing data from external web APIs. Functions within this module are often run in dedicated FreeRTOS tasks to prevent blocking the main loop.
*   **`MqttManager.h` / `.cpp`**: Manages the MQTT connection and all communication for the Home Assistant integration. It handles publishing states and subscribing to commands.
*   **`StockManager.h` / `.cpp`**: A dedicated manager for fetching, parsing, and displaying real-time stock data.
*   **`Sequencer.h`**: Defines the data structures (`SequencerTrack`, `SequenceStep`) and all available commands (`SequenceCommand`) for the animation sequencer engine.
*   **`web_server.h` / `.cpp`**: Sets up all the API endpoints and serves the web interface files using an asynchronous web server.

***

## ⚙️ Development Environment & Tooling

To get started with development, you will need the following:

*   **Arduino IDE or PlatformIO**: The firmware is built for the ESP32 platform.
*   **ESP32 Board Manager**: Add the ESP32 board manager to your IDE.
*   **Libraries**: The following third-party libraries are required. Most can be installed via the Arduino Library Manager.
    *   `Adafruit GFX Library`
    *   `Adafruit LED Backpack`
    *   `WiFiManager` by tzapu
    *   `ArduinoJson` by Benoit Blanchon (v6.x or v7.x)
    *   `ESPAsyncWebServer` by ESP32-Community
    *   `AsyncTCP` by ESP32-Community
    *   `PubSubClient` by Nick O'Leary
    *   `Preferences` (built-in)
    *   `ESP32-audioI2S` by schreibfaul

### Partitioning

The project uses a custom partition scheme to allocate more space for the filesystem (`LittleFS`), which stores web assets, sound files, and configuration data. The partition table is defined in `partitions.csv`.

### Over-The-Air (OTA) Updates

This project supports multiple methods for updating the firmware and filesystem. For detailed, user-focused instructions on how to perform an update, please see the **[🚀 Updating Guide](UPDATING.md)**.

***

## 🔬 Technical Deep Dive

### Asynchronous, Non-Blocking by Design

The core of this project is a fully asynchronous, event-driven architecture, built on the ESP32's FreeRTOS operating system. This is crucial for a device with complex visual elements and real-time display updates.

*   **The Problem with "Blocking" Code:** A simple approach to fetching web data is to make a request and wait for the response. On a microcontroller like the ESP32, this can be disastrous. If a remote server is slow, the entire device will freeze—animations will stutter, sounds will be delayed, and the device will feel unresponsive.
*   **The Event-Driven Solution:** This project leverages the `ESPAsyncWebServer` library for non-blocking network operations and utilizes FreeRTOS tasks to offload time-consuming processes.
    *   **Web Server:** The web server never blocks. It handles multiple connected clients simultaneously and uses callback functions to respond to requests.
    *   **WebSocket Communication:** Real-time communication with the web UI is handled via WebSockets, allowing for a persistent, two-way channel without the overhead of repeated HTTP requests.
    *   **Concurrency:** Outbound requests to external APIs (e.g., weather data, stock prices) are spawned in their own dedicated FreeRTOS tasks using `xTaskCreatePinnedToCore`. This isolates slow network operations from the main application loop, ensuring that even a 10-second API timeout has no impact on the smoothness of the display animations. A `Semaphore` (`xDisplayDataMutex`) is used to protect shared resources, such as the `currentWeatherData` and `displayPages` structs, from being corrupted by concurrent access from different tasks.

### Hardware & Display Management

*   **Dual I2C Bus:** The HT16K33 display driver chip only allows for 8 unique addresses on a single bus. To control all 12 displays, the project cleverly splits them: 8 displays are on one I2C bus (`I2C_1`), and the remaining 4 are on a second I2C bus (`I2C_2`), avoiding the need for a more complex I2C multiplexer.
*   **Unified Animation Engine:** All complex animations, from the pre-canned "styled animations" selected in the UI to the MQTT-triggered sequences, are powered by a single, unified sequencer engine. The main loop no longer contains complex state machine logic for animations. Instead, it simply calls `handleSequencer()` on every iteration, which processes any active animation scripts.
*   **Audio Output:** Audio playback is managed by a dedicated FreeRTOS task to prevent stuttering. It uses the I2S peripheral and the **ESP32-audioI2S** library to play MP3 files from the LittleFS filesystem. The `I2S_SD_PIN` is used to enable/disable the external amplifier to save power when no sound is playing.

### Handling SSL/TLS on the ESP32

Securely connecting to modern APIs via HTTPS (SSL/TLS) is one of the most memory-intensive operations a microcontroller can perform.

*   **The Memory Challenge:** The ESP32 has limited RAM. Loading a server's full SSL certificate chain can consume a significant amount of this memory, which can lead to crashes.
*   **The Solution: `client.setInsecure()`:** This project uses `client.setInsecure()` before making an HTTPS connection.
    *   **What It Does**: It instructs the SSL/TLS engine to **skip the certificate validation step**. It does **not** disable encryption. The connection is still fully encrypted.
    *   **Why It Works**: By skipping validation, the client avoids loading large root certificates into its limited RAM. This eliminates a common source of memory-related errors and greatly improves reliability for this application's purpose of fetching non-sensitive public data.

#### The Unified Animation Sequencer

The firmware includes a powerful, command-driven sequencer for creating complex, timed animations and effects. This is now the **sole mechanism** for all configurable animations, including the "styled animations" selectable in the UI and the advanced automations available in Home Assistant.

The sequencer, managed by `handleSequencer()` in `AnimationManager.cpp`, processes an array of commands for up to three parallel tracks (one for each display row). This allows for highly complex and independent visual effects.

When an animation is triggered (either from the UI or via MQTT), a generator function in `AnimationSequences.cpp` builds the appropriate script and loads it into the sequencer tracks. The main loop then simply executes these commands via `handleSequencer`.

This entire system is exposed via the `bttf-time-circuits/[DEVICE_ID]/sequence/command` MQTT topic, allowing for precise, scripted control over the hardware. For a complete list of available commands and their parameters, refer to the **[🤖 Advanced MQTT Control Guide](reference/ADVANCED_MQTT.md)**.

***

## 🤝 Contribution Guidelines

We welcome contributions to this project! Please follow these guidelines:

1.  **Fork & Branch**: Fork the repository and create a new branch for your work (`feature/your-feature` or `bugfix/your-fix`).
2.  **Code Style**: Please adhere to the existing code style. The code uses a consistent, well-commented, and modular format.
3.  **Test Your Changes**: Before submitting, ensure your changes work as expected and do not introduce any new bugs.
4.  **Pull Request**: Open a pull request with a clear title and a detailed description of your changes. Explain the "what" and "why" of your contribution.

***

## 🌐 Frontend Web Interface

The web UI is a single-page application (SPA) served directly from the ESP32's LittleFS filesystem. It's built with vanilla JavaScript and communicates with the ESP32 via both a RESTful API and a persistent WebSocket connection.

### File Structure
All frontend files are located in the `data` directory of the project. When you use the "ESP32 Sketch Data Upload" tool, these files are uploaded to the ESP32's internal filesystem.

*   **`data/index.html`**: The main HTML file that defines the structure of the page.
*   **`data/style.css`**: Contains all the styling for the web interface.
*   **`data/main_ui.js`**: The main entry point for the UI. It initializes the application, populates the settings from the device, and attaches the primary event listeners.
*   **`data/data_handling.js`**: Manages all communication with the ESP32, including the initial REST API calls to fetch settings and the persistent WebSocket connection for real-time updates.
*   **`data/ui_functions.js`**: Contains helper functions for manipulating the DOM, such as updating specific UI elements, showing/hiding sections, and managing modal dialogs.

### Communication Flow
The frontend uses a hybrid communication model for efficiency:

1.  **Initial Load (REST API)**: When the page first loads, `initializeUI()` in `main_ui.js` makes a series of `fetch` requests to the `/api/settings/*` endpoints. This pulls all the current settings from the device in one go.
2.  **Real-Time Updates (WebSocket)**: After the initial load, `initWebSocket()` in `data_handling.js` establishes a persistent WebSocket connection to the `/ws` endpoint on the ESP32. This connection is used for:
    *   **ESP32 to UI**: The ESP32 can push real-time state updates to the UI (e.g., confirming a setting was saved, providing progress on a file upload).
    *   **UI to ESP32**: The UI can send commands to the ESP32 that require a real-time response, such as testing an API key or a stock symbol. The `ws.onmessage` handler in `data_handling.js` listens for these responses and updates the UI accordingly.

### How to Add a New UI Element

If you wanted to add a new setting to the web interface, you would typically follow these steps:

1.  **Add the HTML**: Add the new input field (e.g., a slider, a text box, a checkbox) to the appropriate settings group in `index.html`. Give it a unique `id`.
2.  **Update JavaScript Files**:
    *   **`main_ui.js`**: In `applySettings()`, add logic to set the value of your new HTML element from the settings object fetched from the ESP32. In `attachEventListeners()`, add an event listener (`onchange` or `onclick`) to your new element.
    *   **`data_handling.js`**: In `saveSettings()`, add a line to read the value from your new HTML element and add it to the `settings` object that gets sent to the ESP32.
    *   **`ui_functions.js`**: If your new element requires complex UI logic (e.g., showing/hiding other elements), add a new helper function here.
4.  **Update ESP32 Firmware**:
    *   Add the corresponding new setting to the `ClockSettings` struct in `HardwareControl.h`. For example, `int theme;`.
    *   If you are adding a complex data field (like for the Data Link), you may need to update the `DataPoint` struct. This struct includes fields for `url`, `authHeaderKey`, `authHeaderValue`, `httpMethod`, `requestBody`, and JSON paths.
    *   In `web_server.cpp`, update the `/api/saveSettings` handler to parse the new setting from the incoming JSON and save it.
    *   Update the appropriate `/api/settings/...` GET endpoint to include your new setting so it can be loaded by the frontend.
    *   Update `saveSettings()` and `loadSettings()` in the main `.ino` file to persist your new setting to the device's non-volatile storage.