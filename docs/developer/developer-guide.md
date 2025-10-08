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
*   **Use Case**: All new animations and effects should be built using the sequencer. It is more powerful, more flexible, and easier to debug. **The complete guide and API reference for the sequencer can be found below.**

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
3.  **Document**: Add the new command to the **Command Reference** table in this document.

### Adding a New Built-in Named Animation

There are two ways to add a new named animation, depending on its complexity. After implementing, be sure to add it to the **Built-in Animations** list at the end of this document.

#### 1. Add a C++ Generated Animation

This is the preferred method for complex animations requiring logic, randomness, or high performance.

1.  **Create Generator Function**: In `AnimationSequences.cpp`, create a new C++ function (e.g., `generateMyCoolAnimation(SequencerTrack tracks[3])`) that uses the `add_step` helper to build your sequence.
2.  **Define `AnimationType`**: Add a new entry to the `AnimationType` enum in `AnimationSequences.h`. This gives your animation a unique ID.
3.  **Register Animation**: In `AnimationSequences.cpp`, add a `case` to the `switch` statement inside the `generateAnimationSequence()` function. This `case` should match your new `AnimationType` and call your generator function.
4.  **Add to `sequences.json`**: Add an entry for your new animation in the `data/sequences.json` file. The `value` should be the string name you want to use (e.g., "My Cool Animation"), and the `name` should be a user-friendly label for the UI. The firmware will automatically map this string name to the `AnimationType` enum you created.

#### 2. Add a JSON-Defined Animation

This method is ideal for simple, declarative sequences that don't require complex C++ logic.

1.  **Define `AnimationType`**: Add a new entry to the `AnimationType` enum in `AnimationSequences.h`.
2.  **Add JSON Case**: In `AnimationSequences.cpp`, add a `case` to the `switch` statement inside `generateAnimationSequence()`. In this case, call `parseSequenceFromJson(tracks, "...")` with your complete JSON sequence as a raw string literal.
3.  **Add to `sequences.json`**: Add an entry for your new animation in `data/sequences.json`.

---
---

# 🤖 Command Sequencer API Reference

The command sequencer is one of the most powerful features of the Time Circuits clock, allowing you to script complex, multi-step, and even parallel animations. You can create custom alerts, intricate visual effects, and timed sequences to integrate the clock into your smart home in creative ways.

All built-in animation logic is centralized in the `AnimationSequences.cpp` file for consistency and easier maintenance.

### **MQTT API Endpoint**

*   **Topic**: `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload**: A JSON object defining a sequence, or a string with a [Built-in Animation](#built-in-animations) name.

---

### **Payload Structure: Custom JSON Sequences**

To create a custom sequence, you send a JSON payload to the MQTT topic. The root of the payload is an object that must contain a `tracks` key. The value is an array `[]` of one or more *track objects*. Each track object defines a sequence of commands that will run on a specific display row. Because each track runs independently, you can use them to create parallel animations on different rows.

A track object has the following structure:

```json
{
  "tracks": [
    {
      "targetRow": "TOP",
      "commands": [
        { "command": "SET_TEXT", "stringParam": "HELLO WORLD" },
        { "command": "WAIT", "intParam": 2000 },
        { "command": "CLEAR_SEGMENT", "targetSegment": -1 }
      ]
    }
  ]
}
```

*   `targetRow`: **(Required)** A number `0-2` or string `"TOP"`, `"MIDDLE"`, `"BOTTOM"` specifying the display row.
*   `commands`: **(Required)** An array of command objects that will be executed in order on the `targetRow`.

---

### **Command Reference**

This table details every command available in the sequencer.

#### **Parameters**

*   `targetRow`: Specified in the parent track object. Determines which display row the command runs on.
*   `targetSegment`: An integer specifying which segment of the row to target. `0`=Month, `1`=Day, `2`=Year, `3`=Time. A value of `-1` targets the **entire row**.
*   `stringParam`, `stringParam2`: A string value used for text, MQTT topics/payloads, or sound file paths.
*   `intParam`, `intParam2`: An integer value, typically used for durations (in milliseconds), speeds, counts, or brightness levels.

---

### **Understanding Blocking vs. Non-Blocking Commands**

It is critical to understand the difference between **blocking** and **non-blocking** commands when creating sequences. This determines whether the sequencer waits for a command to finish before moving to the next step.

*   **Non-Blocking Commands**: These commands execute instantly and the sequencer immediately moves to the next command in the track.
    *   **Commands**: `SET_TEXT`, `CLEAR_SEGMENT`, `SET_BRIGHTNESS`, `RESTORE_ROW`, `RESTORE_ALL_ROWS`, `SOUND`, `MQTT_PUBLISH`, `LOOP_START`, `LOOP_END`.
    *   **Behavior**: If you want a non-blocking command's effect to be visible for a certain duration, you **must** follow it with a `WAIT` command. Without a `WAIT`, the effect might be immediately replaced by the next command in the sequence.
    *   **Example**: To show "HELLO" for 2 seconds, you need two steps: `{"command":"SET_Text", "stringParam":"HELLO"}` followed by `{"command":"WAIT", "intParam":2000}`.

*   **Blocking Commands**: These commands run for a specific duration, and the sequencer will **not** execute the next command until the current one is complete.
    *   **Commands**: `WAIT`, `FADE_IN`, `FADE_OUT`, `PULSE`, `FLASH`, `MARQUEE`, `SCANNER`, `COUNTDOWN`, `TYPEWRITER`, `WIPE`, `SCROLL_IN`, `CROSSFADE_TEXT`, `RANDOM_FLICKER_TEXT`, `SCRAMBLE_TEXT`, `BAR_GRAPH`, `DISPLAY_HA_SENSOR`.
    *   **Behavior**: The duration of these effects is built into the command itself, typically using the `intParam` and/or `intParam2`. You do **not** need to add a separate `WAIT` command after them for their own duration.
    *   **Example**: `{"command":"PULSE", "intParam":5000}` will pulse the display for 5 seconds. The sequencer will automatically wait for those 5 seconds before proceeding.

---

#### **Text and Display Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `SET_TEXT` | **(Non-Blocking)** Instantly displays static text on a segment or the full row. | `stringParam`, `targetSegment` (optional, default: -1) | `{"command":"SET_TEXT", "stringParam":"SYSTEM READY"}` |
| `MARQUEE` | **(Blocking)** Scrolls text across the target row. After scrolling, it leaves the original text centered on the display. | `stringParam` | `{"command":"MARQUEE", "stringParam":"A VERY LONG MESSAGE"}` |
| `SCRAMBLE_TEXT` | **(Blocking)** Reveals text one character at a time with a scrambling effect. | `stringParam`, `intParam` (flicker speed ms), `intParam2` (reveal delay ms) | `{"command":"SCRAMBLE_TEXT", "stringParam":"ACCESSING", "intParam":50, "intParam2":150}` |
| `TYPEWRITER` | **(Blocking)** Reveals text one character at a time, like a typewriter. | `stringParam`, `intParam` (delay ms) | `{"command":"TYPEWRITER", "stringParam":"LOADING...", "intParam":100}` |
| `WIPE` | **(Blocking)** Reveals text with a wipe effect from left to right. | `stringParam`, `intParam` (delay ms) | `{"command":"WIPE", "stringParam":"AUTHORIZED", "intParam":75}` |
| `SCROLL_IN` | **(Blocking)** Scrolls text in from the right and stops with the text justified to the right. | `stringParam`, `intParam` (delay ms) | `{"command":"SCROLL_IN", "stringParam":"WELCOME", "intParam":60}` |
| `CROSSFADE_TEXT` | **(Blocking)** Fades from the current text to new text. | `stringParam`, `intParam` (duration ms) | `{"command":"CROSSFADE_TEXT", "stringParam":"NEW TEXT", "intParam":1500}` |
| `RANDOM_FLICKER_TEXT` | **(Blocking)** Fills the display with random characters that flicker rapidly. If `stringParam` is empty, it intelligently flickers the existing display text. | `stringParam` (char set), `intParam` (flicker speed ms), `intParam2` (duration ms) | `{"command":"RANDOM_FLICKER_TEXT", "intParam":50, "intParam2":5000}` |
| `BAR_GRAPH` | **(Blocking)** Displays a "charging" bar that fills over time. Can have a centered label overlaid. | `stringParam` (label), `intParam` (start percentage 0-100), `intParam2` (duration ms) | `{"command":"BAR_GRAPH", "stringParam":"LOAD", "intParam":0, "intParam2":3000}` |
| `SCANNER` | **(Blocking)** Creates a "Knight Rider" style scanning effect. | `stringParam` (character), `intParam` (duration ms), `intParam2` (speed ms) | `{"command":"SCANNER", "stringParam":"-", "intParam":10000, "intParam2":50}` |
| `COUNTDOWN` | **(Blocking)** Displays a countdown. For numbers > 20, it shows digits. For 20-0, it spells out the word (e.g., "TWENTY"). | `intParam` (start number), `intParam2` (delay per number ms) | `{"command":"COUNTDOWN", "intParam":10, "intParam2":1000}` |
| `CLEAR_SEGMENT` | **(Non-Blocking)** Clears the text from a specific segment or the entire row. | `targetSegment` (optional, default: -1) | `{"command":"CLEAR_SEGMENT", "targetSegment": 1}` |
| `RESTORE_ROW` | **(Non-Blocking)** Restores the target row to its normal display (clock, weather, etc.). | (none) | `{"command":"RESTORE_ROW"}` |
| `RESTORE_ALL_ROWS` | **(Non-Blocking)** Restores all three display rows to their normal function. | (none) | `{"command":"RESTORE_ALL_ROWS"}` |

---

#### **Effects and Utility Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `FADE_IN` | **(Blocking)** Fades the display brightness from 0 to the current setting. | `intParam` (duration ms) | `{"command":"FADE_IN", "intParam":2000}` |
| `FADE_OUT`| **(Blocking)** Fades the display brightness from the current setting to 0. | `intParam` (duration ms) | `{"command":"FADE_OUT", "intParam":2000}` |
| `PULSE` | **(Blocking)** Makes a segment (or row) blink slowly (750ms interval). | `targetSegment`, `intParam` (duration ms) | `{"command":"PULSE", "targetSegment":-1, "intParam":5000}` |
| `FLASH` | **(Blocking)** Makes a segment (or row) flash brightly and rapidly (75ms interval). | `targetSegment`, `intParam` (duration ms) | `{"command":"FLASH", "targetSegment":2, "intParam":1000}` |
| `SET_BRIGHTNESS` | **(Non-Blocking)** Instantly sets the global display brightness. | `intParam` (0-7) | `{"command":"SET_BRIGHTNESS", "intParam":7}` |
| `SOUND` | **(Non-Blocking)** Plays a sound effect from the device's filesystem. | `stringParam` (path, e.g., `/REMOTE.mp3`) | `{"command":"SOUND", "stringParam":"/CONFIRM_ON.mp3"}` |
| `WAIT` | **(Blocking)** Pauses the current track for a set amount of time. | `intParam` (duration ms) | `{"command":"WAIT", "intParam":500}` |
| `LOOP_START` | **(Non-Blocking)** Marks the beginning of a loop. | `intParam` (number of loops) | `{"command":"LOOP_START", "intParam":5}` |
| `LOOP_END` | **(Non-Blocking)** Marks the end of a loop, jumping back to `LOOP_START`. | (none) | `{"command":"LOOP_END"}` |

---

#### **Advanced & Integration Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `TRIGGER_ANIMATION` | **(Global Takeover)** Stops ALL current sequences and runs a new built-in animation by its numeric `AnimationType` ID. Any steps after this command on the same track will not be executed. | `intParam` (AnimationType ID) | `{"command":"TRIGGER_ANIMATION", "intParam": 2}` |
| `MQTT_PUBLISH` | **(Non-Blocking)** Publishes a payload to a specific MQTT topic. | `stringParam` (topic), `stringParam2` (payload) | `{"command":"MQTT_PUBLISH", "stringParam":"home/alarm", "stringParam2":"DISARMED"}` |
| `DISPLAY_HA_SENSOR` | **(Blocking)** Fetches and displays the state of a Home Assistant entity. The device must be subscribed to the entity's state topic. Times out after 5 seconds if no value is received. | `stringParam` (entity_id), `targetSegment` | `{"command":"DISPLAY_HA_SENSOR", "stringParam":"sensor.outside_temp", "targetSegment":0}` |

---

### **Built-in Animations**

The firmware includes a collection of pre-programmed animations that can be triggered by sending their name as a plain string payload to the `.../sequencer/command` MQTT topic.

*   **Example Payload**: `"Time Travel"`

All built-in animations are defined and generated within **`AnimationSequences.cpp`**. This file contains a master `generateAnimationSequence` function that acts as a central dispatcher. There are two types of built-in animations:

*   **C++ Generated Animations**: These are complex, often multi-track animations generated by dedicated C++ functions (e.g., `generateLightning()`). They offer the highest degree of flexibility and performance.
*   **JSON-Defined Animations**: These are simpler animations defined as hardcoded JSON strings directly within `AnimationSequences.cpp`. They are parsed at runtime and are useful for straightforward, declarative sequences.

> **Note:** The `Randomize All` animation, available in the Web UI, will pick a random animation from a curated list of C++ generated animations.

---

#### **1. C++ Generated Animations**
These animations are generated by C++ functions in `AnimationSequences.cpp`.

| Animation Name | Description |
| :--- | :--- |
| `All Displays Random` | The classic BTTF effect. All three rows simultaneously scramble and lock in the current time. Also known as `Time Circuits Lock-In`. |
| `Capacitor Charge-Up` | All three rows fill with a bar graph effect. |
| `Character Scanline` | Reveals the current time with a typewriter effect on all rows. |
| `Code Breaker` | A slower, more deliberate version of the `Time Circuits Lock-In` effect. |
| `Countdown` | A 10-second countdown on the middle row, spelling out the numbers, and ending with a "LIFTOFF!" marquee. |
| `Counting Up` | All three displays rapidly count up. |
| `Digital Rain` | All displays fill with continuously flickering random characters (Matrix-style). |
| `Digit Cascade` | Reveals the time one character at a time, cascading down the displays. |
| `Electric Surge` | A rapid series of bright flashes that cascade down the displays. |
| `Fire Trails` | "Burns" the current time onto the display with a fiery `WIPE` effect on all three rows. |
| `Flip-Disc Display` | Simulates an old-school flip-disc board with a wipe effect. |
| `Flux Capacitor Overload` | All three display rows pulse with energy, simulating an overloaded Flux Capacitor. |
| `Focus In` | Reveals the current time on each row sequentially with a scramble effect. |
| `Glitchy Jump-Cut` | A chaotic loop of random flickering and flashing. |
| `Interference Pattern` | The middle row flickers with the current time while the outer rows show random symbols. |
| `Lightning` | A chaotic, multi-stage lightning storm effect with crackling sounds and intense, random flashes. |
| `Plasma Warm-Up` | A slow fade-in and fade-out effect. |
| `Random Flicker` | A continuous loop of random characters glitching on a random display row. |
| `Scanner` | A Cylon-style red scanner that sweeps back and forth across all three display rows. |
| `Sequential Flicker` | Reveals the current time segment by segment. |
| `Sparkle Reveal` | The time appears out of a field of sparkling lights. |
| `System Error` | A two-row error message with "ERROR" scrambling on top and "SYSTEM MALFUNCTION" scrolling in the middle. |
| `Temporal Desync` | All three displays count up at different, unsynchronized speeds. |
| `Temporal Paradox` | The top and middle rows swap their text while the bottom row flickers. |
| `Timeline Skim` | Displays flicker randomly, then reveal the time with a typewriter effect. |
| `Time Travel Tunnel` | Simulates a time vortex by repeatedly scrolling the current time in from the right. |
| `Time Warp Streaks` | The current time scrolls in from the right on all three rows. |
| `Tornado Flicker` | Random characters flicker up and down the display columns. |
| `Wave Flicker` | Displays a flickering wave pattern. |
| `Waveform Collapse` | A symmetrical waveform pattern collapses and expands. |

---

#### **2. JSON-Defined Animations**
These animations are defined as hardcoded JSON strings in `AnimationSequences.cpp`.

| Animation Name | Description |
| :--- | :--- |
| `Data Stream` | All three rows flicker with random characters for 10 seconds. |
| `Error` | A simple error message with scrambling text and a marquee. |
| `Flux Charge` | A charging bar graph on the bottom row with flashing on the top two. |
| `Intruder Alert` | A three-row alert with marquees, sound, and scrambling text. |
| `Knight Rider` | A scanner effect on the bottom row. |
| `Loading` | A sequential "loading" message across all three rows. |
| `Party Mode` | A looping animation with marquees and pulsing lights for a party atmosphere. |
| `Tachyons` | A scrambling message ("TACHYONS ON") on the middle row with a sound effect. |
| `Time Travel` | A classic 88MPH sequence with a bar graph, marquee, and flashing lights. |
| `Wormhole Collapse` | All three rows flicker and then fade out sequentially. |