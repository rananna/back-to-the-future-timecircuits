# 🔬 Developer's Guide & Technical Deep Dive

This document provides a deeper look into the project's architecture, code structure, and development details for those looking to understand, modify, or contribute to the firmware.

## Table of Contents

*   [High-Level Architecture](#-high-level-architecture)
*   [The Animation Sequencer](#-the-animation-sequencer)
*   [MQTT API Reference](./mqtt-api.md)
*   [Frontend Web Interface Guide](./frontend-guide.md)
*   [Contributing New Animations](#-contributing-new-animations)
*   [Testing and Diagnostics](#-testing-and-diagnostics)

---

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

## 💡 The Animation Sequencer

It is crucial for developers to understand that the firmware contains a powerful, modern animation system called the **Sequencer**.

*   **Description**: This is the primary animation system. It is a command-based engine that can execute a series of steps in a sequence, even in parallel on different display rows. It is highly flexible and is the basis for all complex cinematic effects and Home Assistant blueprints.
*   **Location**: The core logic is in `handleSequencer()` in `AnimationManager.cpp`. The command definitions are in `Sequencer.h`. Pre-programmed sequences are created in `AnimationSequences.cpp`.
*   **Trigger**: Triggered by sending a JSON payload or a named animation string to the `.../sequencer/command` MQTT topic, which is handled by `handleSequencerCommand()` in `MqttManager.cpp`.
*   **Use Case**: All new animations and effects should be built using the sequencer. It is more powerful, more flexible, and easier to debug.

> [!NOTE]
> An older, deprecated legacy animation system also exists in the firmware. No new development should use this system.

---

## 🚀 External Guides

For detailed information on the MQTT API and Frontend development, please see the dedicated guides:

*   **[🤖 MQTT API Reference](./mqtt-api.md)**: A complete reference for all MQTT topics and commands.
*   **[🌐 Frontend Web Interface Guide](./frontend-guide.md)**: A guide to the web UI's architecture and how to add new settings.

---

## 🛠️ Contributing New Animations

The best way to contribute new visual effects is by using the modern sequencer system.

### Adding a New Sequencer Command

1.  **Define Enum**: Add a new `SEQ_CMD_...` to the `SequenceCommand` enum in `Sequencer.h`.
2.  **Implement Logic**: Add a `case` to the `switch` statement in `handleSequencer()` in `AnimationManager.cpp` to implement your command's logic.
3.  **Document**: Add the new command to the `Command Reference` section of the [MQTT API Guide](./mqtt-api.md).

### Adding a New Built-in Named Animation

1.  **Create Generator Function**: In `AnimationSequences.cpp`, create a new C++ function (e.g., `generateMyCoolAnimation(SequencerTrack tracks[3])`) that uses the `add_step` helper to build your sequence.
2.  **Define `AnimationType`**: Add a new entry to the `AnimationType` enum in `AnimationTypes.h`.
3.  **Register Animation**: In `AnimationSequences.cpp`, add a `case` to the `switch` statement inside the `generateAnimationSequence()` function to call your new generator.
4.  **Add to `sequences.json`**: Add an entry for your new animation in the `data/sequences.json` file.
5.  **Document**: Add the new animation to the `Built-in Animations` list in this guide.

---

## 🧪 Testing and Diagnostics

The firmware includes a built-in diagnostic test suite to help developers and advanced users verify that the hardware and core software systems are functioning correctly.

### The 'Test Suite' Animation

*   **How to Run**: Select "Test Suite" from the animation dropdown in the Web UI, or send the payload `"Test Suite"` to the `.../sequencer/command` MQTT topic.
*   **What it Tests**: A comprehensive, multi-track sequence designed to exercise all major components of the device simultaneously.
*   **Expected Outcome**: Upon successful completion, all three display rows will show the message **`TESTS: PASS`**. If the device crashes, hangs, or does not display this message, check the serial monitor output for detailed error messages.
