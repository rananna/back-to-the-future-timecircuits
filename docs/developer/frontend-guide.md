# 🌐 Frontend Web Interface Guide

The web UI is a single-page application (SPA) served directly from the ESP32's LittleFS filesystem. It's built with vanilla JavaScript and communicates with the ESP32 via a RESTful API and a persistent WebSocket connection.

## File Structure
All frontend files are located in the `data` directory.

*   `data/index.html`: The main HTML structure.
*   `data/style.css`: All CSS styles.
*   `data/main_ui.js`: The main entry point for the UI, handling initialization and primary event listeners.
*   `data/data_handling.js`: Manages all communication (REST and WebSocket) with the ESP32.
*   `data/ui_functions.js`: Contains helper functions for manipulating the DOM.

## How to Add a New UI Setting

1.  **Add the HTML**: Add a new input field with a unique `id` to `index.html`.
2.  **Update `data_handling.js`**: In the `saveSettings` function, read the value from your new HTML element and add it to the `settings` object that gets sent to the ESP32.
3.  **Update `main_ui.js`**: In the `applySettings` function, add logic to set the value of your new HTML element from the settings object fetched from the ESP32.
4.  **Update Firmware**:
    *   Add the new setting to the `ClockSettings` struct in `HardwareControl.h`.
    *   Update the `/api/saveSettings` handler in `web_server.cpp` to parse the new setting.
    *   Update the appropriate `/api/settings/...` GET endpoint in `web_server.cpp` to include your new setting.
    *   Update `saveSettings()` and `loadSettings()` in the main `.ino` file to persist the setting to non-volatile storage.
