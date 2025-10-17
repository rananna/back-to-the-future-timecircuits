# MQTT Sequencer API Reference

This document provides a detailed guide to creating custom animations and sequences using the low-level MQTT Sequencer API. This API offers powerful, direct control over the display hardware and is intended for developers and advanced users.

**Prerequisite:** It is highly recommended that you first read the main [MQTT API Reference](./mqtt-api.md) to understand the basic topic structure and how to send commands to your device.

## The Sequencer Topic

All sequencer commands are sent to a single MQTT topic:

`bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`

The payload sent to this topic can be one of two things:
1.  A **String Payload**: The name of a built-in, C++ defined animation (e.g., `"Lightning"`).
2.  A **JSON Payload**: A JSON object or array that defines a custom animation sequence.

This document focuses exclusively on the **JSON Payload**.

## JSON Payload Structure

The firmware is flexible and can accept a JSON payload in two formats: a single track object or an array of track objects for parallel execution.

### Single-Track Sequence

For simple, single-threaded animations, you can send a single JSON object. This is useful for a sequence of effects that should happen one after another on a single display row.

**Structure:**
```json
{
  "targetRow": "TOP" | "MIDDLE" | "BOTTOM" | 0 | 1 | 2,
  "commands": [
    { "command": "COMMAND_NAME", "param": "value", ... },
    { "command": "COMMAND_NAME", "param": "value", ... }
  ]
}
```

### Multi-Track (Parallel) Sequence

For complex, multi-layered animations, you can send a JSON array where each object represents a track that will run in parallel with the others. This is the key to creating rich effects, like having a progress bar on one row, a status message on another, and a visual effect on the third, all running simultaneously.

**Important:** When using parallel tracks, each track **must** target a unique `targetRow`. The firmware does not support running two parallel sequences on the same row.

**Structure:**
```json
[
  {
    "targetRow": "TOP",
    "commands": [
      { "command": "COMMAND_1", ... },
      { "command": "COMMAND_2", ... }
    ]
  },
  {
    "targetRow": "MIDDLE",
    "commands": [
      { "command": "COMMAND_A", ... },
      { "command": "COMMAND_B", ... }
    ]
  }
]
```

---

## Command Reference

Each command in the `commands` array is a JSON object with a `command` key and optional parameters.

### Command Parameters

Commands can accept up to four optional parameters:
*   `targetSegment` (Integer): The display segment to target. `0`=Month, `1`=Day, `2`=Year, `3`=Time. If omitted or set to `-1`, the command applies to the entire row.
*   `intParam` (Integer): The first integer parameter. Its meaning depends on the command.
*   `intParam2` (Integer): The second integer parameter. Its meaning depends on the command.
*   `stringParam` (String): The first string parameter.
*   `stringParam2` (String): The second string parameter.

If a parameter is not applicable to a command, it can be omitted.

### Command Table

| Command | `intParam` | `intParam2` | `stringParam` | `stringParam2` | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **STATE & TEXT** |
| `SET_TEXT` | - | - | Text to display | - | Sets static text on a segment or row. |
| `CLEAR_SEGMENT`| - | - | - | - | Clears the text from a segment or row. |
| `RESTORE_SEGMENT`| - | - | - | - | Restores a segment to its pre-animation state. |
| `CLEAR_ALL_ROWS`| - | - | - | - | Clears text from all three display rows. |
| `RESTORE_ALL_ROWS`| - | - | - | - | Restores all rows to their pre-animation state. |
| `RESTORE_ROW` | - | - | - | - | Restores a single row to its pre-animation state. |
| **LOGIC & FLOW** |
| `WAIT` | Duration (ms) | - | - | - | Pauses the sequence on this track for a duration. |
| `LOOP_START`| Loop Count | - | - | - | Starts a loop. Commands between this and `LOOP_END` will repeat. |
| `LOOP_END` | - | - | - | - | Marks the end of a `LOOP_START` block. |
| `TRIGGER_ANIMATION`| - | - | Animation Name | - | **Global action.** Stops all tracks and starts a new built-in animation. |
| **VISUAL EFFECTS** |
| `PULSE` | Cycle (ms) | Duration (ms)| Text to pulse | - | Makes text blink on and off. `intParam` is the cycle time (on+off), `intParam2` is the total duration. |
| `FLASH` | - | Duration (ms)| Text to flash | - | A high-intensity, rapid blink effect. |
| `FADE_IN` | Duration (ms) | - | Text to fade in | - | Fades the text in by increasing brightness over time. |
| `FADE_OUT` | Duration (ms) | - | - | - | Fades the current text out by decreasing brightness. |
| `SCRAMBLE_TEXT`| Flicker (ms) | Reveal (ms) | Final Text | - | Shows random characters, then reveals the final text one character at a time. `intParam` is flicker speed, `intParam2` is reveal speed per character. |
| `RANDOM_FLICKER_TEXT`| Duration (ms) | Interval (ms)| Char Set | - | Flickers the display with random characters. `stringParam` provides an optional set of characters to use. |
| `TYPEWRITER`| Delay (ms) | - | Text to type | - | Reveals the text one character at a time. |
| `WIPE` | Delay (ms) | - | Text to wipe | - | Reveals text with a left-to-right wipe effect. |
| `SCROLL_IN`| Delay (ms) | - | Text to scroll| - | Scrolls text in from the right side of the display. |
| `CROSSFADE_TEXT`| Duration (ms) | - | New Text | - | Smoothly crossfades from the current text to the new text. |
| `SCANNER` | Duration (ms) | Delay (ms) | Scanner Text | - | Creates a back-and-forth scanner light. `stringParam` is the character(s) for the light. |
| `BAR_GRAPH`| End % | Duration (ms)| Overlay Text | - | Draws a progress bar that fills to `intParam` percent over `intParam2` duration. |
| `MARQUEE` | Speed (ms) | Duration (ms)| Text to scroll| - | Scrolls text continuously from right to left. |
| **SYSTEM & INTEGRATION** |
| `SOUND` | - | - | Filename | - | Plays one of the built-in sound effects (e.g., `alarm.mp3`). |
| `SET_BRIGHTNESS`| Brightness (0-7)| - | - | - | Sets the global display brightness. |
| `MQTT_PUBLISH`| - | - | Topic | Payload | Publishes a custom MQTT message. |
| `DISPLAY_HA_SENSOR`| - | - | `entity_id` | - | Displays the state of a Home Assistant sensor. The device must be subscribed to the sensor's state topic. |

---

## Practical Examples

### Example 1: Simple "Warning" Alert

This example shows a single-track sequence that flashes a warning on the middle row and plays a sound.

```json
{
  "targetRow": "MIDDLE",
  "commands": [
    {
      "command": "SOUND",
      "stringParam": "error.mp3"
    },
    {
      "command": "SET_TEXT",
      "stringParam": "WARNING"
    },
    {
      "command": "PULSE",
      "intParam": 500,
      "intParam2": 5000
    },
    {
      "command": "RESTORE_ROW"
    }
  ]
}
```
**Breakdown:**
1.  It targets the `MIDDLE` row.
2.  It immediately plays `error.mp3`.
3.  It sets the text to "WARNING".
4.  It pulses that text with a 500ms cycle (250ms on, 250ms off) for a total of 5 seconds.
5.  It restores the middle row to whatever was on it before the animation started.

### Example 2: Multi-Track "System Boot" Sequence

This example demonstrates a more complex, parallel animation that simulates a system booting up.

```json
[
  {
    "targetRow": "TOP",
    "commands": [
      { "command": "TYPEWRITER", "stringParam": "SYSTEM BOOT...", "intParam": 100 },
      { "command": "WAIT", "intParam": 4000 },
      { "command": "SET_TEXT", "stringParam": "SYSTEM READY" },
      { "command": "FLASH", "intParam2": 1000 }
    ]
  },
  {
    "targetRow": "MIDDLE",
    "commands": [
      { "command": "WAIT", "intParam": 1500 },
      { "command": "BAR_GRAPH", "stringParam": "LOADING", "intParam": 100, "intParam2": 4000 }
    ]
  },
  {
    "targetRow": "BOTTOM",
    "commands": [
      { "command": "SOUND", "stringParam": "hum.mp3" },
      { "command": "WAIT", "intParam": 5500 },
      { "command": "SOUND", "stringParam": "arrival_chime.mp3" }
    ]
  }
]
```
**Breakdown:**
*   **Track 0 (Top):** Types out "SYSTEM BOOT...", waits, then displays "SYSTEM READY" with a celebratory flash.
*   **Track 1 (Middle):** Waits 1.5 seconds, then shows a progress bar filling up over 4 seconds with the text "LOADING" overlaid.
*   **Track 2 (Bottom):** Plays a humming sound at the start, waits for the other tracks to finish, and then plays a "chime" sound to signal completion.

These examples can be sent directly to the `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command` topic using an MQTT client like MQTT Explorer or a script.