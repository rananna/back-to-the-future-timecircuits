# 🔬 Developer's Guide & Technical Deep Dive

This document provides a deeper look into the project's architecture, code structure, and development details for those looking to understand, modify, or contribute to the firmware.

## 🏗️ High-Level Architecture

The firmware is organized into a modular C++ structure to separate concerns and improve maintainability. The core philosophy is to be **asynchronous and non-blocking** wherever possible to ensure smooth visual animations, even while performing slow network operations.

*   **Main Entry Point**: `back-to-the-future-timecircuits.ino` contains the primary `setup()` and `loop()` functions and acts as a coordinator for all other modules.
*   **Hardware Abstraction**: `HardwareControl.cpp` is the lowest-level module that interacts directly with the display driver chips.
*   **State & Display Management**: `DisplayManager.cpp` is responsible for what is shown on the displays during normal operation (e.g., the clock, weather, or stock data).
*   **Animation Systems**: The firmware contains two distinct animation systems, managed by `AnimationManager.cpp`. (See "Animation Systems" section below for details).
*   **Networking & Data**: `DataManager.cpp` handles fetching data from external web APIs. These operations are run in dedicated FreeRTOS tasks to prevent blocking the main loop.
*   **MQTT & Home Assistant**: `MqttManager.cpp` manages all communication with the MQTT broker for Home Assistant integration, including publishing device states and handling incoming commands.
*   **Web Server & UI**: `web_server.cpp` serves the frontend files from the `data/` directory and provides the backend REST API and WebSocket server that the web UI communicates with.

---

## 💡 Animation Systems: Legacy vs. Modern Sequencer

It is crucial for developers to understand that the firmware contains **two different animation systems**.

### **1. Legacy Animation System**

*   **Description**: This is the original animation system. It consists of C++ functions (e.g., `animateTornadoFlicker()`) that directly manipulate the display hardware in a loop.
*   **Location**: The functions are defined in `HardwareControl.cpp` and are called from `handleStyledAnimation()` in `AnimationManager.cpp`.
*   **Trigger**: These are triggered by setting the `currentSettings.animationStyle` enum and calling `startStyledAnimation()`. They are also exposed via string names in the MQTT `handleSequencerCommand` function for backward compatibility.
*   **Use Case**: These animations are generally simpler, full-screen effects. No new development should use this system.

### **2. Modern Sequencer System**

*   **Description**: This is the primary, more powerful animation system. It is a command-based engine that can execute a series of steps in a sequence, even in parallel on different display rows. It is highly flexible and is the basis for all complex cinematic effects and Home Assistant blueprints.
*   **Location**: The core logic is in `handleSequencer()` in `AnimationManager.cpp`. The command definitions are in `Sequencer.h`. Pre-programmed sequences are created in `AnimationSequences.cpp`.
*   **Trigger**: Triggered by sending a JSON payload or a named animation string to the `.../sequencer/command` MQTT topic, which is handled by `handleSequencerCommand()` in `MqttManager.cpp`.
*   **Use Case**: All new animations and effects should be built using the sequencer. It is more powerful, more flexible, and easier to debug. For a complete guide, see the **[Sequencer API Reference](sequencer-api.md)**.

---

## 🌐 Frontend Web Interface

The web UI is a single-page application (SPA) served directly from the ESP32's LittleFS filesystem. It's built with vanilla JavaScript and communicates with the ESP32 via a RESTful API and a persistent WebSocket connection.

### File Structure
All frontend files are located in the `data` directory.

*   `data/index.html`: The main HTML structure.
*   `data/style.css`: All CSS styles.
*   `data/main_ui.js`: The main entry point for the UI, handling initialization and primary event listeners.
*   `data/data_handling.js`: Manages all communication (REST and WebSocket) with the ESP32.
*   `data/ui_functions.js`: Contains helper functions for manipulating the DOM.

### How to Add a New UI Setting

1.  **Add the HTML**: Add a new input field with a unique `id` to `index.html`.
2.  **Update `data_handling.js`**: In the `saveSettings` function, read the value from your new HTML element and add it to the `settings` object that gets sent to the ESP32.
3.  **Update `main_ui.js`**: In the `applySettings` function, add logic to set the value of your new HTML element from the settings object fetched from the ESP32.
4.  **Update Firmware**:
    *   Add the new setting to the `ClockSettings` struct in `HardwareControl.h`.
    *   Update the `/api/saveSettings` handler in `web_server.cpp` to parse the new setting.
    *   Update the appropriate `/api/settings/...` GET endpoint in `web_server.cpp` to include your new setting.
    *   Update `saveSettings()` and `loadSettings()` in the main `.ino` file to persist the setting to non-volatile storage.

---

## 🛠️ Contributing New Animations

The best way to contribute new visual effects is by using the modern sequencer system.

### Adding a New Sequencer Command

1.  **Define Enum**: Add a new `SEQ_CMD_...` to the `SequenceCommand` enum in `Sequencer.h`.
2.  **Implement Logic**: Add a `case` to the `switch` statement in `handleSequencer()` in `AnimationManager.cpp` to implement your command's logic.
3.  **Document**: Add the new command to the reference table in `docs/developer/sequencer-api.md`.

### Adding a New Built-in Named Animation

1.  **Create Generator Function**: In `AnimationSequences.cpp`, create a new function (e.g., `generateMyCoolAnimation(SequencerTrack tracks[3])`) that uses the `add_step` helper to build your animation.
2.  **Define `AnimationType`**: Add a new entry to the `AnimationType` enum in `AnimationSequences.h`.
3.  **Register Animation**: Add a `case` to the `switch` statement in `generateAnimationSequence()` in `AnimationSequences.cpp` that calls your new generator function.
4.  **Expose via MQTT (Optional)**: To make the animation triggerable by a simple string, add an `else if` block to `handleSequencerCommand()` in `MqttManager.cpp` that calls `generateAnimationSequence` with your new `AnimationType`.
5.  **Document**: Add the new animation to the "Built-in Animations" list in `docs/developer/sequencer-api.md`.