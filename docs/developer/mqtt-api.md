# 🤖 MQTT API Reference

The MQTT API is the primary method for controlling the Time Circuits clock programmatically and monitoring its state.

## Topic Structure

All topics follow a consistent structure. Replace `YOUR_DEVICE_ID` with the actual ID of your device (e.g., its MAC address).

*   **Command Topics**: `bttf-time-circuits/YOUR_DEVICE_ID/COMMAND_NAME/command`
    *   Used to **send instructions to** the device.
*   **State Topics**: `bttf-time-circuits/YOUR_DEVICE_ID/STATE_NAME/state`
    *   Used to **receive status updates from** the device.

## State Topics

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

## General Command Topics

These topics provide direct control over specific settings and actions.

| Command Topic | Payload | Description |
| :--- | :--- | :--- |
| `display/command` | String | Instantly displays a line of text on a specific row. Format: `"ROW:TEXT"`, where `ROW` is `TOP`, `MIDDLE`, or `BOTTOM`. Example: `"MIDDLE:HELLO WORLD"`. |
| `display_mode/command` | String | Sets the main operating mode. Accepts `"Normal Clock"`, `"Stock Ticker"`, `"Weather"`, or `"Data Link"`. |
| `brightness/command` | Number (0-7) | Sets the display brightness. |
| `volume/command` | Number (0-21) | Sets the audio volume. |
| `reboot_device/command`| `PRESS` | Reboots the ESP32. |
| `force_ntp_sync/command`| `PRESS` | Manually forces a time sync with NTP servers. |

## Audio Command Topics

| Command Topic | Payload | Description |
| :--- | :--- | :--- |
| `radio/command` | String | Controls the internet radio. Accepts `play_favorite_radio` or `stop_radio`. |
| `sound/command` | String | Plays a built-in sound effect by its filename (e.g., `sys_beep.mp3`). |
| `tts/command` | String (URL or JSON) | Plays audio from a URL. Can be a raw URL or a JSON object from Home Assistant's `tts.google_translate_say` service (`{"media_id": "URL"}`). |

## Animation & Sequencer Commands

The command sequencer is one of the most powerful features of the clock. You can script complex, multi-step, and even parallel animations.

*   **Topic**: `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload**: A JSON object defining a sequence, or a string with a [Built-in Animation](#built-in-animations) name.

### **Payload Structure: Custom JSON Sequences**

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

### **Command Reference**

This table details every command available in the sequencer.

| Command | Description & Parameters |
| :--- | :--- |
| `SET_TEXT` | **(Non-Blocking)** Instantly displays static text.<br/>`stringParam`: Text to display.<br/>`targetSegment`: (Optional) `0`-`3` or `-1` for full row. |
| `MARQUEE` | **(Blocking)** Scrolls text across the target row.<br/>`stringParam`: Text to scroll. |
| `SCRAMBLE_TEXT` | **(Blocking)** Reveals text with a scrambling effect.<br/>`stringParam`: Final text.<br/>`intParam`: Flicker speed (ms).<br/>`intParam2`: Total duration (ms). |
| `TYPEWRITER` | **(Blocking)** Reveals text one character at a time.<br/>`stringParam`: Text to type.<br/>`intParam`: Delay between characters (ms). |
| `WIPE` | **(Blocking)** Reveals text with a wipe effect.<br/>`stringParam`: Text to wipe.<br/>`intParam`: Delay between characters (ms). |
| `CROSSFADE_TEXT`| **(Blocking)** Fades from the current text to new text.<br/>`stringParam`: New text to display.<br/>`intParam`: Duration of the fade (ms). |
| `BAR_GRAPH` | **(Blocking)** Displays a "charging" bar.<br/>`stringParam`: (Optional) Text label to overlay.<br/>`intParam`: Starting percentage (0-100).<br/>`intParam2`: Duration to fill the bar (ms). |
| `SCANNER` | **(Blocking)** Creates a "Knight Rider" style scanning light.<br/>`stringParam`: Character for the scanner light.<br/>`intParam`: Total duration of the effect (ms).<br/>`intParam2`: Delay between steps (ms). |
| `COUNTDOWN` | **(Blocking)** Displays a numeric countdown.<br/>`intParam`: Number to start from.<br/>`intParam2`: Delay between each number (ms). |
| `CLEAR_SEGMENT` | **(Non-Blocking)** Clears text from a segment or the full row.<br/>`targetSegment`: (Optional) `0`-`3` or `-1` for full row. |
| `RESTORE_ROW` | **(Non-Blocking)** Restores the target row to its normal display state (clock, etc.). |
| `FADE_IN` | **(Blocking)** Fades the display in from black.<br/>`intParam`: Duration of the fade (ms). |
| `PULSE` | **(Blocking)** Makes a segment or row blink slowly.<br/>`intParam2`: Total duration of the effect (ms).<br/>`targetSegment`: (Optional) `0`-`3` or `-1` for full row. |
| `FLASH` | **(Blocking)** Makes a segment or row flash rapidly.<br/>`intParam2`: Total duration of the effect (ms).<br/>`targetSegment`: (Optional) `0`-`3` or `-1` for full row. |
| `SOUND` | **(Non-Blocking)** Plays a sound effect from the device's filesystem.<br/>`stringParam`: Full path to the sound file (e.g., `/sys_beep.mp3`). |
| `WAIT` | **(Blocking)** Pauses the current animation track.<br/>`intParam`: Duration of the pause (ms). |
| `LOOP_START` | **(Non-Blocking)** Marks the beginning of a loop.<br/>`intParam`: Number of times to repeat the loop. |
| `LOOP_END` | **(Non-Blocking)** Marks the end of a loop block. |
| `MQTT_PUBLISH` | **(Non-Blocking)** Publishes a message to an MQTT topic.<br/>`stringParam`: MQTT topic.<br/>`stringParam2`: Payload to publish. |
| `DISPLAY_HA_SENSOR`| **(Blocking)** Displays the value of a Home Assistant sensor.<br/>`stringParam`: The `entity_id` of the sensor. |
