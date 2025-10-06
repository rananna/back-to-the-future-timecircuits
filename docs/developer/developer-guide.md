# 🔬 Developer's Guide & Technical Deep Dive

This document provides a deeper look into the project's architecture, code structure, and development details for those looking to understand or modify the firmware.

## 🏗️ Project Structure

The firmware is organized into a modular structure to separate concerns and improve maintainability.

* **`back-to-the-future-timecircuits.ino`**: The main entry point of the application. Contains `setup()` and `loop()` and coordinates all other modules.
* **`HardwareControl.h` / `.cpp`**: The hardware abstraction layer. Defines core data structures (`ClockSettings`, `WeatherData`) and all functions for direct interaction with the displays and LEDs using the **Adafruit_LEDBackpack** and **Adafruit_GFX** libraries.
* **`DisplayManager.h` / `.cpp`**: Responsible for what is shown on the displays during normal operation (standard clock, weather, Data Link marquee).
* **`AnimationManager.h` / `.cpp`**: Contains the logic for complex, multi-stage animations like the time travel sequence and boot-up.
* **`DataManager.h` / `.cpp`**: Handles all networking tasks for fetching and parsing data from external web APIs. Functions within this module are often run in dedicated FreeRTOS tasks to prevent blocking the main loop.
* **`MqttManager.h` / `.cpp`**: Manages the MQTT connection and all communication for the Home Assistant integration. It handles publishing states and subscribing to commands.
* **`StockManager.h` / `.cpp`**: A dedicated manager for fetching, parsing, and displaying real-time stock data.
* **`Sequencer.h`**: Defines the data structures and commands for the cinematic effect sequencer.
* **`web_server.h` / `.cpp`**: Sets up all the API endpoints and serves the web interface files using an asynchronous web server.

***

## ⚙️ Development Environment & Tooling

To get started with development, you will need the following:

* **Arduino IDE**: The firmware is built for the ESP32 platform using the Arduino IDE. PlatformIO is not officially supported.
* **ESP32 Board Manager**: Add the ESP32 board manager to your IDE.
* **Libraries**: The following third-party libraries are required. Most can be installed via the Arduino Library Manager.
    * `Adafruit GFX Library`
    * `Adafruit LED Backpack`
    * `WiFiManager` by tzapu
    * `ArduinoJson` by Benoit Blanchon (v6.x or v7.x)
    * `ESPAsyncWebServer` by ESP32-Community
    * `AsyncTCP` by ESP32-Community
    * `PubSubClient` by Nick O'Leary
    * `Preferences` (built-in)
    * `ESP32-audioI2S` by schreibfaul

### Partitioning

The project uses a custom partition scheme to allocate more space for the filesystem (`LittleFS`), which stores web assets, sound files, and configuration data. The partition table is defined in `partitions.csv`.

### Over-The-Air (OTA) Updates

This project supports multiple methods for updating the firmware and filesystem. For detailed, user-focused instructions on how to perform an update, please see the **[🚀 Updating Guide](../getting-started/updating.md)**.

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

#### Sequencer for Cinematic Effects

The firmware includes a powerful, command-driven sequencer for creating complex, timed animations and effects. This is the mechanism behind the iconic time travel sequence and the advanced automations available in Home Assistant.

The sequencer is the powerful, command-driven engine behind all complex animations. Its main processing loop is the `handleSequencer()` function (located in `AnimationManager.cpp`), which is called on every iteration of the main `loop()` in the `.ino` file.

This system is exposed via the `bttf_time_circuits/sequencer/command` MQTT topic, allowing for precise, scripted control over the hardware. This is essential for creating screen-accurate cinematic moments and enables users to design their own complex notification sequences.

Each command in the sequence can perform an action like displaying text, flashing a segment, playing a sound, or pausing. For a complete list of available commands and their parameters, refer to the canonical **[Sequencer API Reference](sequencer-api.md)**.


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

***

## 💡 Adding New Sequencer Commands & Animations

The command sequencer is a core part of this project and a great place to contribute.

### Adding a New Sequencer Command

1.  **Define the Command Enum**: Add a new entry to the `SequenceCommand` enum in `Sequencer.h`.
2.  **Implement the Logic**: Add a new `case` to the `switch` statement in the `handleSequencer` function in `AnimationManager.cpp`. This is where you'll implement the logic for your new command.
3.  **Document the Command**: Add a new row to the command reference table in `docs/developer/sequencer-api.md`.

### Adding a New Built-in Animation

1.  **Create a Generator Function**: In `AnimationSequences.cpp`, create a new function (e.g., `generateMyCoolAnimation(SequencerTrack tracks[3])`) that uses the `add_step` helper to build your animation sequence across the three tracks.
2.  **Define the AnimationType**: Add a new entry to the `AnimationType` enum in `AnimationSequences.h`.
3.  **Register the Animation**: Add a new `case` to the `switch` statement in the `generateAnimationSequence` function in `AnimationSequences.cpp` that calls your new generator function.
4.  **Add to `Randomize All`**: If your animation is suitable for general use, consider adding its `AnimationType` to the `validAnimationStyles` array in `generateAnimationSequence` so it can be triggered by the "Randomize All" feature.
5.  **Document the Animation**: Add a new row to the "Available Animations" table in `docs/developer/sequencer-api.md`.