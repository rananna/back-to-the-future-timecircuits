# 🤖 Command Sequencer Deep Dive

The command sequencer is one of the most powerful features of the Time Circuits clock, allowing you to script complex, multi-step, and even parallel animations. You can create custom alerts, intricate visual effects, and timed sequences to integrate the clock into your smart home in creative ways.

This guide provides a complete reference for the sequencer's capabilities, including the payload structure, a full list of commands, and the available built-in animations.

---

### **MQTT API Endpoint**

*   **Topic**: `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload**: A JSON object defining a sequence, or a string with a [Built-in Animation](#built-in-animations) name.

---

### **Using the Sequencer with Home Assistant**

The real power of the sequencer is unlocked when you integrate it with Home Assistant. You can make the clock react to any event, sensor, or trigger in your smart home.

#### **1. The Easy Way: Using Blueprints**

For the vast majority of users, the easiest and recommended way to use the sequencer is with our pre-built **Home Assistant Blueprints**. These provide a user-friendly interface for common tasks like displaying text, showing sensor values, and running countdowns, without needing to write any code.

*   **To get started, see the [Home Assistant Blueprints README](../../home_assistant/blueprints/README.md).**

#### **2. The Powerful Way: Crafting Custom Sequences**

For ultimate flexibility, you can bypass the blueprints and send commands directly to the MQTT API. This allows you to build complex, multi-track animations and use Home Assistant templates to include dynamic data from your entities right in the display.

There are two ways to do this:
*   **Trigger a Built-in Animation**: Send the name of a pre-programmed animation as a simple string.
*   **Send a Custom JSON Payload**: Construct a detailed JSON object to control animations with precision.

##### **Advanced Example: Triggering a Built-in Animation**

This is perfect for common alerts and effects. You simply send the animation's name as the payload.

```yaml
alias: Trigger Intruder Alert on Break-in
trigger:
  - platform: state
    entity_id: binary_sensor.front_door_contact
    to: 'on'
condition:
  - condition: state
    entity_id: alarm_control_panel.home_alarm
    state: armed_away
action:
  - service: mqtt.publish
    data:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command"
      payload: "Intruder Alert"
```

##### **Advanced Example: Crafting a Custom JSON Sequence**
You can build your own sequences directly in your automation's YAML.

```yaml
alias: Display Freezing Temperature Alert
trigger:
  - platform: numeric_state
    entity_id: sensor.outside_temperature
    below: 0
action:
  - service: mqtt.publish
    data_template:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command"
      payload: >
        {
          "tracks": [
            {
              "targetRow": "TOP",
              "commands": [
                {
                  "command": "MARQUEE",
                  "stringParam": "FREEZING TEMP: {{ states('sensor.outside_temperature') }}°C"
                },
                {
                  "command": "PULSE",
                  "targetSegment": -1,
                  "intParam": 5000
                }
              ]
            }
          ]
        }
```

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
    *   **Commands**: `SET_TEXT`, `MARQUEE`, `SOUND`
    *   **Behavior**: If you want a non-blocking command's effect to be visible for a certain duration, you **must** follow it with a `WAIT` command. Without a `WAIT`, the effect might be immediately replaced by the next command in the sequence.
    *   **Example**: To show "HELLO" for 2 seconds, you need two steps: `{"command":"SET_TEXT", "stringParam":"HELLO"}` followed by `{"command":"WAIT", "intParam":2000}`.

*   **Blocking Commands**: These commands run for a specific duration, and the sequencer will **not** execute the next command until the current one is complete.
    *   **Commands**: `PULSE`, `FLASH`, `SCANNER`, `COUNTDOWN`, `FADE_IN`, `FADE_OUT`.
    *   **Behavior**: The duration of these effects is built into the command itself, typically using the `intParam`. You do **not** need to add a separate `WAIT` command after them for their own duration.
    *   **Example**: `{"command":"PULSE", "intParam":5000}` will pulse the display for 5 seconds. The sequencer will automatically wait for those 5 seconds before proceeding.

---

#### **Text and Display Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `SET_TEXT` | Instantly displays static text on a segment or the full row. | `stringParam`, `targetSegment` (optional, default: -1) | `{"command":"SET_TEXT", "stringParam":"SYSTEM READY"}` |
| `MARQUEE` | Scrolls text across the target row. **Note:** After scrolling, it leaves the original text centered on the display, allowing you to chain effects like `PULSE`. | `stringParam` | `{"command":"MARQUEE", "stringParam":"A VERY LONG MESSAGE"}` |
| `SCRAMBLE_TEXT` | Reveals text one character at a time with a scrambling effect. | `stringParam`, `intParam` (flicker speed ms), `intParam2` (reveal delay ms) | `{"command":"SCRAMBLE_TEXT", "stringParam":"ACCESSING", "intParam":50, "intParam2":150}` |
| `TYPEWRITER` | Reveals text one character at a time, like a typewriter. | `stringParam`, `intParam` (delay ms) | `{"command":"TYPEWRITER", "stringParam":"LOADING...", "intParam":100}` |
| `WIPE` | Reveals text with a wipe effect from left to right. | `stringParam`, `intParam` (delay ms) | `{"command":"WIPE", "stringParam":"AUTHORIZED", "intParam":75}` |
| `SCROLL_IN` | Scrolls text in from the right and stops with the text justified to the right. | `stringParam`, `intParam` (delay ms) | `{"command":"SCROLL_IN", "stringParam":"WELCOME", "intParam":60}` |
| `CROSSFADE_TEXT` | Fades from the current text to new text. | `stringParam`, `intParam` (duration ms) | `{"command":"CROSSFADE_TEXT", "stringParam":"NEW TEXT", "intParam":1500}` |
| `RANDOM_FLICKER_TEXT` | Fills the display with random characters that flicker rapidly. If `stringParam` is empty, it intelligently flickers the existing display text. | `stringParam` (character set), `intParam` (duration ms), `intParam2` (flicker speed ms) | `{"command":"RANDOM_FLICKER_TEXT", "intParam":5000, "intParam2":50}` |
| `BAR_GRAPH` | Displays a "charging" bar that fills from left to right. | `stringParam` (label), `intParam` (duration ms), `intParam2` (speed ms) | `{"command":"BAR_GRAPH", "stringParam":"LOAD", "intParam":3000, "intParam2":50}` |
| `SCANNER` | Creates a "Knight Rider" style scanning effect. **This is a blocking command.** | `stringParam` (character), `intParam` (duration ms), `intParam2` (speed ms) | `{"command":"SCANNER", "stringParam":"-", "intParam":10000, "intParam2":50}` |
| `COUNTDOWN` | Displays a countdown. For numbers > 20, it shows digits. For 20-0, it spells out the word (e.g., "TWENTY"). **This is a blocking command.** | `intParam` (start number), `intParam2` (delay per number ms) | `{"command":"COUNTDOWN", "intParam":10, "intParam2":1000}` |
| `CLEAR_SEGMENT` | Clears the text from a specific segment or the entire row. | `targetSegment` (optional, default: -1) | `{"command":"CLEAR_SEGMENT", "targetSegment": 1}` |
| `RESTORE_ROW` | Restores the target row to its normal display (clock, weather, etc.). | (none) | `{"command":"RESTORE_ROW"}` |
| `RESTORE_ALL_ROWS` | Restores all three display rows to their normal function. | (none) | `{"command":"RESTORE_ALL_ROWS"}` |

---

#### **Effects and Utility Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `FADE_IN` | Fades the display brightness from 0 to the current setting. **This is a blocking command.** | `intParam` (duration ms) | `{"command":"FADE_IN", "intParam":2000}` |
| `FADE_OUT`| Fades the display brightness from the current setting to 0. **This is a blocking command.** | `intParam` (duration ms) | `{"command":"FADE_OUT", "intParam":2000}` |
| `PULSE` | Makes a segment (or row) blink slowly (750ms interval). **This is a blocking command.** | `targetSegment`, `intParam` (duration ms) | `{"command":"PULSE", "targetSegment":-1, "intParam":5000}` |
| `FLASH` | Makes a segment (or row) flash brightly and rapidly (75ms interval). **This is a blocking command.** | `targetSegment`, `intParam` (duration ms) | `{"command":"FLASH", "targetSegment":2, "intParam":1000}` |
| `SET_BRIGHTNESS` | Instantly sets the global display brightness. | `intParam` (0-7) | `{"command":"SET_BRIGHTNESS", "intParam":7}` |
| `SOUND` | Plays a sound effect from the device's filesystem. This command is non-blocking. | `stringParam` (path, e.g., `/REMOTE.mp3`) | `{"command":"SOUND", "stringParam":"/CONFIRM_ON.mp3"}` |
| `WAIT` | Pauses the current track for a set amount of time. | `intParam` (duration ms) | `{"command":"WAIT", "intParam":500}` |
| `LOOP_START` | Marks the beginning of a loop. | `intParam` (number of loops) | `{"command":"LOOP_START", "intParam":5}` |
| `LOOP_END` | Marks the end of a loop, jumping back to `LOOP_START`. | (none) | `{"command":"LOOP_END"}` |

---

#### **Advanced & Integration Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `TRIGGER_ANIMATION` | **Advanced Use.** Stops the current sequence and runs a built-in animation by its numeric `AnimationType` ID. See `AnimationSequences.h` for the full enum list. | `intParam` (AnimationType ID) | `{"command":"TRIGGER_ANIMATION", "intParam": 2}` |
| `MQTT_PUBLISH` | Publishes a payload to a specific MQTT topic. | `stringParam` (topic), `stringParam2` (payload) | `{"command":"MQTT_PUBLISH", "stringParam":"home/alarm", "stringParam2":"DISARMED"}` |
| `DISPLAY_HA_SENSOR` | Fetches and displays the state of a Home Assistant entity. The device must be subscribed to the entity's state topic. | `stringParam` (entity_id), `targetSegment` | `{"command":"DISPLAY_HA_SENSOR", "stringParam":"sensor.outside_temp", "targetSegment":0}` |

---

### **Built-in Animations**

The firmware includes a collection of pre-programmed animations that can be triggered by sending their `Animation Name` as a plain string payload to the `.../sequencer/command` MQTT topic.

*   **Example Payload**: `"Intruder Alert"`

There are three types of built-in animations, each with a different origin in the code.

> **Note on availability:** Not all animations defined in the firmware are available via a string name. Some complex animations (like `Scanner`, `Flux Capacitor Overload`, `System Error`, etc.) can only be started using the `TRIGGER_ANIMATION` command with the appropriate numeric `AnimationType` ID.

#### **1. Modern Generated Animations**
These are complex, multi-track animations generated by C++ functions in `AnimationSequences.cpp`. They represent the most advanced visual effects.

| Animation Name | Description |
| :--- | :--- |
| `All Displays Random` | The classic BTTF effect. All three rows simultaneously scramble and lock in the current time, character by character. Also known as `Time Circuits Lock-In`. |
| `Time Travel Tunnel` | Simulates traveling through a time vortex by repeatedly scrolling the current time in from the right on all three rows. |
| `Fire Trails` | "Burns" the current time onto the display with a fiery `WIPE` effect that reveals the text from left to right on all three rows. |
| `Sparkle Reveal` | A subtle reveal where the time appears out of a field of sparkling lights. The display flickers with random dots before wiping to reveal the current time. |

#### **2. JSON Alias Animations**
These names are shortcuts defined in `MqttManager.cpp` that trigger a hardcoded JSON payload. They are useful for common, multi-track scenarios.

| Animation Name | Description |
| :--- | :--- |
| `Intruder Alert` | A three-row alert with marquees, sound, and scrambling text. |
| `Time Travel` | A classic 88MPH sequence with a bar graph, marquee, and flashing lights. |
| `Party Mode` | A looping animation with marquees and pulsing lights for a party atmosphere. |
| `Countdown` | Displays "COUNTDOWN" on the middle row, then shows a 10-second countdown (spelling out the numbers), ending with a "LIFTOFF!" marquee. |
| `Knight Rider` | A scanner effect on the bottom row. |
| `Cylon` | A scanner effect on the middle row. |
| `Lightning` | A chaotic, multi-stage lightning storm effect with loud crackling sounds and intense, random flashes across all displays. |
| `Loading` | A sequential "loading" message on all three rows. |
| `Error` | A simpler error message with scrambling text and a marquee. |
| `Flux Capacitor Charge-Up` | A charging bar graph on the bottom row with flashing on the top two. |
| `Tachyons Detected` | A scrambling message on the middle row with a sound effect. |
| `Data Stream` | All three rows flicker with random characters for 10 seconds. |
| `Wormhole Collapse`| All three rows flicker and then fade out sequentially. |

#### **3. Legacy Animations**
These names trigger older, single-purpose animation functions from the original firmware. They are generally simpler and often affect all three displays at once. They are included in the `Randomize All` pool when triggering animations from the Web UI.

| Animation Name | Description |
| :--- | :--- |
| `Sequential Flicker` | Reveals the current time segment by segment. |
| `Random Flicker` | A continuous loop of random characters glitching on a random display row. |
| `Counting Up` | All three displays rapidly count up. |
| `Wave Flicker` | Displays a flickering wave pattern. |
| `Tornado Flicker` | Random characters flicker up and down the display columns. |
| `Capacitor Charge-Up` | All three rows fill with a bar graph effect. |
| `Digital Rain` | All displays fill with continuously flickering random characters (Matrix-style). |
| `Waveform Collapse` | A symmetrical waveform pattern collapses and expands. |
| `Timeline Skim` | Displays flicker randomly, then reveal the time with a typewriter effect. |
| `Temporal Desync` | All three displays count up at different, unsynchronized speeds. |
| `Glitchy Jump-Cut` | A chaotic loop of random flickering and flashing. |
| `Plasma Warm-Up` | A slow fade-in and fade-out effect. |
| `Time Warp Streaks` | The current time scrolls in from the right on all three rows. |
| `Character Scanline` | Reveals the current time with a typewriter effect on all rows. |
| `Focus In` | Reveals the current time on each row sequentially with a scramble effect. |
| `Code Breaker` | A slower, more deliberate version of the `Time Circuits Lock-In` effect. |
| `Temporal Paradox` | The top and middle rows swap their text while the bottom row flickers. |
| `Digit Cascade` | Reveals the time one character at a time, cascading down the displays. |
| `Electric Surge` | A rapid series of bright flashes that cascade down the displays. |
| `Flip-Disc Display` | Simulates an old-school flip-disc board with a wipe effect. |
| `Interference Pattern` | The middle row flickers with the current time while the outer rows show random symbols. |