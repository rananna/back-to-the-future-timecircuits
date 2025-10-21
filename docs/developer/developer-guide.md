# 🔬 Developer's Guide & Technical Deep Dive

This document provides a deeper look into the project's architecture, code structure, and development details for those looking to understand, modify, or contribute to the firmware.

## Table of Contents

*   [High-Level Architecture](#-high-level-architecture)
*   [The Animation Sequencer](#-the-animation-sequencer)
*   [MQTT API Reference](#-mqtt-api-reference)
    *   [Topic Structure](#topic-structure)
    *   [State Topics](#state-topics)
    *   [General Command Topics](#general-command-topics)
    *   [Audio Command Topics](#audio-command-topics)
    *   [Animation & Sequencer Commands](#animation--sequencer-commands)
*   [Built-in Animations](#-built-in-animations)
    *   [C++ Generated Animations](#c-generated-animations)
    *   [JSON-Defined Animations](#json-defined-animations)
*   [Frontend Web Interface Guide](#-frontend-web-interface-guide)
    *   [File Structure](#file-structure)
    *   [How to Add a New UI Setting](#how-to-add-a-new-ui-setting)
*   [Contributing New Animations](#-contributing-new-animations)
    *   [Adding a New Sequencer Command](#adding-a-new-sequencer-command)
    *   [Adding a New Built-in Named Animation](#adding-a-new-built-in-named-animation)
*   [Testing and Diagnostics](#-testing-and-diagnostics)
    *   [The 'Test Suite' Animation](#the-test-suite-animation)

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

## 🤖 MQTT API Reference

The MQTT API is the primary method for controlling the Time Circuits clock programmatically and monitoring its state.

### Topic Structure

All topics follow a consistent structure. Replace `YOUR_DEVICE_ID` with the actual ID of your device (e.g., its MAC address).

*   **Command Topics**: `bttf_time_circuits/YOUR_DEVICE_ID/COMMAND_NAME/command`
    *   Used to **send instructions to** the device.
*   **State Topics**: `bttf_time_circuits/YOUR_DEVICE_ID/STATE_NAME/state`
    *   Used to **receive status updates from** the device.

### State Topics

These read-only topics allow you to monitor the device's status. They are updated in real-time.

| State Name | Value Type | Description |
| :--- | :--- | :--- |
| `availability` | String | `online` or `offline`. Used for Home Assistant's availability feature. |
| `device_name` | String | The friendly name of the device (e.g., "TimeCircuits-123456"). |
| `device_ip` | String | The current IP address of the device. |
| `device_mac` | String | The MAC address of the device. |
| `device_version`| String | The current firmware version (e.g., "v2.1.0"). |
| `display_mode` | String | The current operating mode. One of `"Normal Clock"`, `"Stock Ticker"`, `"Weather"`, `"Data Link"`. |
| `brightness` | Number (0-7) | The current display brightness level. |
| `volume` | Number (0-21) | The current audio volume level. |
| `animation` | String | The name of the currently running animation, or `"none"`. |
| `preset` | String | The name of the currently active preset. |
| `radio_station`| String | The name of the currently playing favorite radio station. |
| `dest_time` | String | The full "Destination Time" display string (e.g., "JAN 01 2025 12:00"). |
| `pres_time` | String | The full "Present Time" display string. |
| `last_time` | String | The full "Last Time Departed" display string. |

### General Command Topics

These topics provide direct control over specific settings and actions.

| Command Topic | Payload | Description |
| :--- | :--- | :--- |
| `display/command` | String | Instantly displays a line of text on a specific row. Format: `"ROW:TEXT"`, where `ROW` is `TOP`, `MIDDLE`, or `BOTTOM`. Example: `"MIDDLE:HELLO WORLD"`. |
| `display_mode/command` | String | Sets the main operating mode. Accepts `"Normal Clock"`, `"Stock Ticker"`, `"Weather"`, or `"Data Link"`. |
| `brightness/command` | Number (0-7) | Sets the display brightness. |
| `volume/command` | Number (0-21) | Sets the audio volume. |
| `reboot_device/command`| `PRESS` | Reboots the ESP32. |
| `force_ntp_sync/command`| `PRESS` | Manually forces a time sync with NTP servers. |

### Audio Command Topics

| Command Topic | Payload | Description |
| :--- | :--- | :--- |
| `radio/command` | String | Controls the internet radio. Accepts `play_favorite_radio` or `stop_radio`. |
| `sound/command` | String | Plays a built-in sound effect by its filename (e.g., `sys_beep.mp3`). |
| `tts/command` | String (URL or JSON) | Plays audio from a URL. Can be a raw URL or a JSON object from Home Assistant's `tts.google_translate_say` service (`{"media_id": "URL"}`). |

### Animation & Sequencer Commands

The command sequencer is one of the most powerful features of the clock. You can script complex, multi-step, and even parallel animations.

*   **Topic**: `bttf_time_circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload**: A JSON object defining a sequence, or a string with a [Built-in Animation](#-built-in-animations) name.

#### **Payload Structure: Custom JSON Sequences**

To create a custom sequence, you send a JSON payload to the MQTT topic. The root of the payload can be one of two formats:

1.  **Single Track**: A single JSON object `{}` that defines one sequence for one display row.
2.  **Parallel Tracks**: A JSON array `[]` of multiple track objects. Each track in the array will run in parallel on its specified display row.

> **⚠️ Important Rule for Parallel Tracks**
> When you provide a JSON array to run multiple tracks in parallel, **each track object in the array must target a unique display row**.

A track object has the following structure:

```json
{
  "targetRow": "TOP",
  "commands": [
    { "command": "SET_TEXT", "stringParam": "HELLO WORLD" },
    { "command": "WAIT", "intParam": 2000 },
    { "command": "CLEAR_SEGMENT", "targetSegment": -1 }
  ]
}
```

*   `targetRow`: **(Required)** A number `0-2` or string `"TOP"`, `"MIDDLE"`, `"BOTTOM"` specifying the display row for this track.
*   `commands`: **(Required)** An array of command objects that will be executed in order on the `targetRow`.

#### **Command Reference**

This table details every command available in the sequencer.

| Command | Description & Parameters |
| :--- | :--- |
| `SET_TEXT` | **(Non-Blocking)** Instantly displays static text.<br/>`stringParam`: Text to display.<br/>`targetSegment`: (Optional) `0`-`3` or `-1` for full row. |
| `SET_BRIGHTNESS` | **(Non-Blocking)** Sets the brightness for the target row.<br/>`intParam`: Brightness level (0-15). |
| `CLEAR_SEGMENT` | **(Non-Blocking)** Clears text from a segment or the full row.<br/>`targetSegment`: `0`-`3` or `-1` for full row. |
| `RESTORE_SEGMENT` | **(Non-Blocking)** Restores a segment to its pre-animation state.<br/>`targetSegment`: `0`-`3`. |
| `RESTORE_ROW` | **(Non-Blocking)** Restores the target row to its normal display state. |
| `CLEAR_ALL_ROWS` | **(Non-Blocking)** Clears the text from all three display rows. |
| `RESTORE_ALL_ROWS` | **(Non-Blocking)** Restores all three rows to their normal display state. |
| `MARQUEE` | **(Blocking)** Scrolls text across the target row.<br/>`stringParam`: Text to scroll.<br/>`intParam`: Speed (ms). |
| `SCRAMBLE_TEXT` | **(Blocking)** Reveals text with a scrambling effect.<br/>`stringParam`: Final text.<br/>`intParam`: Flicker speed (ms).<br/>`intParam2`: Total duration (ms). |
| `TYPEWRITER` | **(Blocking)** Reveals text one character at a time.<br/>`stringParam`: Text to type.<br/>`intParam`: Delay between characters (ms). |
| `WIPE` | **(Blocking)** Reveals text with a wipe effect.<br/>`stringParam`: Text to wipe.<br/>`intParam`: Delay between characters (ms). |
| `SCROLL_IN`| **(Blocking)** Scrolls text in from the side and stops.<br/>`stringParam`: Text to scroll.<br/>`intParam`: Speed (ms). |
| `CROSSFADE_TEXT`| **(Blocking)** Fades from the current text to new text.<br/>`stringParam`: New text.<br/>`intParam`: Duration of the fade (ms). |
| `BAR_GRAPH` | **(Blocking)** Displays a "charging" bar.<br/>`stringParam`: (Optional) Text label to overlay.<br/>`intParam`: Starting percentage (0-100).<br/>`intParam2`: Duration to fill the bar (ms). |
| `SCANNER` | **(Blocking)** Creates a "Knight Rider" style scanning light.<br/>`intParam`: Total duration (ms).<br/>`intParam2`: Delay between steps (ms). |
| `RANDOM_FLICKER_TEXT` | **(Blocking)** Flickers random characters.<br/>`stringParam`: (Optional) Character set to use.<br/>`intParam`: Flicker speed (ms).<br/>`intParam2`: Total duration (ms). |
| `COUNTDOWN` | **(Blocking)** Displays a numeric countdown.<br/>`intParam`: Number to start from. |
| `FADE_IN` | **(Blocking)** Fades the display in from black.<br/>`intParam`: Duration of the fade (ms). |
| `FADE_OUT` | **(Blocking)** Fades the display out to black.<br/>`intParam`: Duration of the fade (ms). |
| `PULSE` | **(Blocking)** Makes a segment or row blink slowly.<br/>`intParam2`: Total duration of the effect (ms).<br/>`targetSegment`: (Optional) `0`-`3` or `-1` for full row. |
| `FLASH` | **(Blocking)** Makes a segment or row flash rapidly.<br/>`intParam2`: Total duration of the effect (ms).<br/>`targetSegment`: (Optional) `0`-`3` or `-1` for full row. |
| `SOUND` | **(Non-Blocking)** Plays a sound effect from the device's filesystem.<br/>`stringParam`: Full path to the sound file (e.g., `/sys_beep.mp3`). |
| `WAIT` | **(Blocking)** Pauses the current animation track.<br/>`intParam`: Duration of the pause (ms). |
| `LOOP_START` | **(Non-Blocking)** Marks the beginning of a loop.<br/>`intParam`: Number of times to repeat the loop. |
| `LOOP_END` | **(Non-Blocking)** Marks the end of a loop block. |
| `TRIGGER_ANIMATION`| **(Global)** Stops all tracks and starts a new global built-in animation.<br/>`intParam`: The `AnimationType` enum value. |
| `MQTT_PUBLISH` | **(Non-Blocking)** Publishes a message to an MQTT topic.<br/>`stringParam`: MQTT topic.<br/>`stringParam2`: Payload to publish. |
| `DISPLAY_HA_SENSOR`| **(Blocking)** Displays the value of a Home Assistant sensor.<br/>`stringParam`: The `entity_id` of the sensor. |

---

## 🎨 Built-in Animations

You can trigger any of the built-in animations by sending its name as a string payload to the `.../sequencer/command` MQTT topic.

### C++ Generated Animations
These animations are generated by dedicated C++ functions in the firmware, allowing for dynamic and complex multi-track effects.

| Animation Name | Description |
| :--- | :--- |
| `All Displays Random` | All three rows scramble and resolve to the correct time in parallel. |
| `Capacitor Charge-Up`| Simulates a capacitor charging with a bar graph, crackling energy, and a final flash. |
| `Character Scanline` | Reveals text with a typewriter effect, accompanied by a synchronized scanner light. |
| `Code Breaker` | A code-cracking sequence with a scrambling text reveal and a progress bar. |
| `Counting Up` | A number rapidly counts up on the middle row with a synchronized progress bar. |
| `Countdown` | A 10-second countdown with a progress bar and status text. |
| `Data Stream` | Simulates a data transfer with a scrolling hex feed, status updates, and a progress bar. |
| `Digital Rain` | A "Matrix"-style digital rain effect with multiple layers of falling characters. |
| `Digit Cascade` | Reveals each row's text in parallel with a typewriter effect. |
| `Electric Surge` | Simulates a building and discharging electric surge with parallel effects. |
| `Fire Trails` | Wipes the time text onto all three displays in parallel. |
| `Flip-Disc Display` | Simulates a mechanical flip-disc display with varied, parallel wipes. |
| `Flux Capacitor Overload`| A rapid, chaotic pulsing effect on all three rows. |
| `Focus In` | Reveals each row's text sequentially with a scramble effect. |
| `Glitchy Jump-Cut` | A chaotic, desynchronized glitch effect that is randomly generated on each run. |
| `Interference Pattern`| Creates a visual conflict with opposing wipe effects and a pulsing center row. |
| `Intruder Alert` | A multi-track alert sequence with flashing text, a scanner, and a progress bar. |
| `Knight Rider` | A multi-stage KITT-style sequence with activation, scanning, and shutdown phases. |
| `Lightning` | A chaotic lightning storm with intense, random flashes and crackles. |
| `Party Mode` | An energetic sequence with pulsing text, a fast scanner, and flashing lights. |
| `Plasma Warm-Up` | A multi-stage system activation sequence, from ignition to stabilization. |
| `Random Flicker` | A dynamic, multi-stage random flicker and glitch animation. |
| `Randomize All` | Triggers one of the other C++ generated animations at random. |
| `Scanner` | A classic KITT-style scanner that sweeps across all three rows. |
| `Sequential Flicker` | Reveals the time, one segment at a time, across all rows. |
| `Sparkle Reveal` | A twinkling starfield effect that smoothly resolves into the final time text. |
| `System Boot` | A multi-stage system boot-up sequence with diagnostics and loading bars. |
| `System Error` | Displays a scrambled "ERROR" message with a "SYSTEM MALFUNCTION" marquee. |
| `Temporal Desync` | Creates a feeling of temporal instability with conflicting, parallel timelines. |
| `Temporal Paradox` | A chaotic animation with conflicting past, present, and future timelines. |
| `Test Suite` | A diagnostic tool that tests all major hardware and software subsystems. |
| `Time Circuits Lock-In`| The iconic effect where all displays scramble and rapidly resolve to the correct time. |
| `Time Travel` | A multi-stage sequence that tells the story of a time jump, from power-up to arrival. |
| `Time Travel Tunnel` | Simulates traveling through a time tunnel by scrolling text in rapidly. |
| `Timeline Skim` | Simulates rapidly cycling through time by scrambling through random date strings. |
| `Time Warp Streaks` | Simulates a time warp with high-speed, multi-directional streaks of random dates. |
| `Tornado Flicker` | A multi-stage animation of a tornado forming, intensifying, and dissipating. |
| `Wave Flicker` | A dynamic wave effect with patterns moving in opposite directions. |
| `Waveform Collapse` | A symmetrical animation of a waveform collapsing and expanding. |

### JSON-Defined Animations
These animations are defined as JSON strings within the firmware. They are typically simpler, single-purpose effects.

| Animation Name | Description |
| :--- | :--- |
| `Error` | A simple error message with a sound and a marquee. |
| `Flux Capacitor Charge-Up`| A basic charge-up sequence using a bar graph and flashes. |
| `Tachyons Detected` | A simple text scramble that reveals "TACHYONS ON". |
| `Wormhole Collapse` | A random flicker effect on all rows that fades out. |

---

## 🌐 Frontend Web Interface Guide

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
3.  **Document**: Add the new command to the `Command Reference` section in this guide.

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
