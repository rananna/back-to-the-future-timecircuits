# 🤖 Advanced MQTT Control Guide

Beyond the basic controls exposed in Home Assistant, the Time Circuits clock has a powerful, low-level MQTT API that allows for advanced scripting and direct control over the display. This guide covers two key features: the **Command Sequencer** for creating complex, timed animations, and the **Manual Display Override** for directly writing text to the displays.

These features are intended for advanced users who are comfortable with MQTT and want to create custom automations or integrate the clock with systems other than Home Assistant.

---

### **Command Sequencer**

The command sequencer is a powerful engine that allows you to script a series of actions for the clock to perform in order. With the latest update, the sequencer now supports **parallel tracks**, allowing you to run independent animations on different display rows simultaneously.

*   **MQTT Topic**: `bttf-time-circuits/[DEVICE_ID]/sequence/command`
*   **Payload**: A JSON array of *track objects*.

Each track object in the array defines a sequence for a specific row and must contain:
*   `targetRow`: The display row to run this sequence on (0 = Top, 1 = Middle, 2 = Bottom).
*   `commands`: An array of command objects, which will be executed in order on that row.

> **⚠️ Breaking Change:** The payload format has changed. You must now wrap your command arrays inside a track object with a `targetRow`. See the example below.

#### **Available Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `MARQUEE` | Scrolls a string of text across the entire `targetRow`. | `stringParam` | `{"command":"MARQUEE", "stringParam":"SYSTEMS ONLINE"}` |
| `FADE_IN` | Smoothly fades the brightness of all displays from off to full. | `intParam` (duration in ms) | `{"command":"FADE_IN", "intParam":2000}` |
| `FADE_OUT`| Smoothly fades the brightness of all displays from full to off. | `intParam` (duration in ms) | `{"command":"FADE_OUT", "intParam":2000}` |
| `PULSE` | Causes a specific display segment on the `targetRow` to blink slowly. | `targetSegment`, `intParam` (duration in ms) | `{"command":"PULSE", "targetSegment":1, "intParam":5000}` |
| `SOUND` | Plays a sound effect from the device's internal storage. | `stringParam` (filename) | `{"command":"SOUND", "stringParam":"/REMOTE.mp3"}` |
| `FLASH` | Triggers a bright, flashing effect on a specific segment of the `targetRow`. | `targetSegment`, `intParam` (duration in ms) | `{"command":"FLASH", "targetSegment":2, "intParam":500}` |
| `WAIT` | Pauses this track for a specified duration. | `intParam` (duration in ms) | `{"command":"WAIT", "intParam":1000}` |

#### **Parameter Details**

*   `targetRow`: **(Required in track object)** The display row to target (0 = Top, 1 = Middle, 2 = Bottom).
*   `targetSegment`: The segment of the row to target (0-3, left to right). Used by `PULSE`, `FLASH`.
*   `stringParam`: A string value, used for text (`MARQUEE`) or filenames (`SOUND`).
*   `intParam`: An integer value, used for durations in milliseconds.

#### **Example: Parallel Sequences**

Here is an example of a payload that runs two sequences at the same time:
1.  On the top row (`targetRow: 0`), it will pulse the "day" segment for 5 seconds.
2.  On the bottom row (`targetRow: 2`), it will scroll a message and then play a sound.

```json
[
  {
    "targetRow": 0,
    "commands": [
      {"command": "PULSE", "targetSegment": 1, "intParam": 5000}
    ]
  },
  {
    "targetRow": 2,
    "commands": [
      {"command": "MARQUEE", "stringParam": "PARALLEL SEQUENCING ENABLED"},
      {"command": "WAIT", "intParam": 500},
      {"command": "SOUND", "stringParam": "/CONFIRM_ON.mp3"}
    ]
  }
]
```

---

### **Manual Display Override**

This feature gives you direct, granular control over the text shown on each of the 12 display segments. When you send a command to this endpoint, it will override whatever is currently being shown on that segment (e.g., the time, weather, or stock data) and display your custom text instead.

This override is **persistent** until you clear it by sending an empty string.

*   **MQTT Topic**: `bttf-time-circuits/[DEVICE_ID]/display/manual/command`
*   **Payload**: A JSON object specifying the target and the text.

The JSON payload must contain three fields:
*   `row`: The display row to target (0-2).
*   `segment`: The segment of the row to target (0-3).
*   `text`: The string to display. The text will be automatically converted to uppercase and truncated to fit the segment. To clear an override, send an empty string (`""`).

#### **Example Override**

This example will write the text "FAIL" to the segment that normally shows the current year (middle row, third segment).

*   **Topic**: `bttf-time-circuits/ab12cd34ef56/display/manual/command`
*   **Payload**:
    ```json
    {"row":1, "segment":2, "text":"FAIL"}
    ```

To clear this override and return the segment to its normal function, you would send:

*   **Topic**: `bttf-time-circuits/ab12cd34ef56/display/manual/command`
*   **Payload**:
    ```json
    {"row":1, "segment":2, "text":""}
    ```