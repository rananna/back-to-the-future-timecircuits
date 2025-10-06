# 🤖 Command Sequencer Deep Dive

The command sequencer is one of the most powerful features of the Time Circuits clock, allowing you to script complex, multi-step, and even parallel animations. You can create custom alerts, intricate visual effects, and timed sequences to integrate the clock into your smart home in creative ways.

This guide provides a complete reference for the sequencer's capabilities, including the payload structure, a full list of commands, and the available built-in animations.

---

### **MQTT API Endpoint**

*   **Topic**: `bttf-time-circuits/[DEVICE_ID]/sequence/command`
*   **Payload**: A JSON array of *track objects*, or a string with a [Built-in Animation](#built-in-animations) name.

---

### **Payload Structure: JSON Tracks**

To create a custom sequence, you send a JSON payload to the MQTT topic. The root of the payload is an array `[]` that can contain one or more *track objects*. Each track object defines a sequence of commands that will run on a specific display row. Because each track runs independently, you can use them to create parallel animations on different rows.

A track object has the following structure:

```json
{
  "targetRow": 0,
  "commands": [
    { "command": "SET_TEXT", "stringParam": "HELLO WORLD" },
    { "command": "WAIT", "intParam": 2000 },
    { "command": "CLEAR_SEGMENT", "targetSegment": -1 }
  ]
}
```

*   `targetRow`: **(Required)** The display row to run this sequence on (`0` for Top, `1` for Middle, `2` for Bottom).
*   `commands`: **(Required)** An array of command objects that will be executed in order on the `targetRow`.

#### **Example: Parallel Sequences**

Here is an example of a payload that runs two sequences at the same time:
1.  On the top row (`targetRow: 0`), it scrolls a message.
2.  On the bottom row (`targetRow: 2`), it shows a charging bar graph and then flashes.

```json
[
  {
    "targetRow": 0,
    "commands": [
      { "command": "MARQUEE", "stringParam": "PARALLEL SEQUENCING" }
    ]
  },
  {
    "targetRow": 2,
    "commands": [
      { "command": "BAR_GRAPH", "stringParam": "CHARGING", "intParam": 0, "intParam2": 5000 },
      { "command": "FLASH", "targetSegment": -1, "intParam": 500 }
    ]
  }
]
```

---

### **Command Reference**

This table details every command available in the sequencer.

#### **Parameters**

*   `targetRow`: Specified in the parent track object. Determines which of the 3 display rows the command runs on.
*   `targetSegment`: An integer specifying which segment of the row to target. `0`=Month, `1`=Day, `2`=Year, `3`=Time. A value of `-1` targets the **entire row**.
*   `stringParam`, `stringParam2`: A string value used for text, MQTT topics/payloads, or sound file paths.
*   `intParam`, `intParam2`: An integer value, typically used for durations (in milliseconds), speeds, counts, or brightness levels.

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
| `RANDOM_FLICKER_TEXT` | Fills the display with random characters that flicker rapidly. If `stringParam` is empty, it intelligently flickers the existing display text. | `stringParam` (character set), `intParam` (flicker speed ms), `intParam2` (duration ms) | `{"command":"RANDOM_FLICKER_TEXT", "intParam":50, "intParam2":5000}` |
| `BAR_GRAPH` | Displays a "charging" bar that fills from left to right. | `stringParam` (label), `intParam` (start %), `intParam2` (duration ms) | `{"command":"BAR_GRAPH", "stringParam":"LOAD", "intParam":0, "intParam2":3000}` |
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
| `PULSE` | Makes a segment (or row) blink slowly. **This is a blocking command.** | `targetSegment`, `intParam` (duration ms) | `{"command":"PULSE", "targetSegment":-1, "intParam":5000}` |
| `FLASH` | Makes a segment (or row) flash brightly and rapidly. **This is a blocking command.** | `targetSegment`, `intParam` (duration ms) | `{"command":"FLASH", "targetSegment":2, "intParam":1000}` |
| `SET_BRIGHTNESS` | Instantly sets the global display brightness. | `intParam` (0-7) | `{"command":"SET_BRIGHTNESS", "intParam":7}` |
| `SOUND` | Plays a sound effect from the device's filesystem. This command is non-blocking. | `stringParam` (path, e.g., `/REMOTE.mp3`) | `{"command":"SOUND", "stringParam":"/CONFIRM_ON.mp3"}` |
| `WAIT` | Pauses the current track for a set amount of time. | `intParam` (duration ms) | `{"command":"WAIT", "intParam":500}` |
| `LOOP_START` | Marks the beginning of a loop. | `intParam` (number of loops) | `{"command":"LOOP_START", "intParam":5}` |
| `LOOP_END` | Marks the end of a loop, jumping back to `LOOP_START`. | (none) | `{"command":"LOOP_END"}` |

---

#### **Advanced & Integration Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `TRIGGER_ANIMATION` | **Advanced Use.** Stops the sequencer and runs a built-in animation by its numeric ID. To trigger by name, send the name as a top-level string payload instead. | `intParam` (AnimationType ID) | `{"command":"TRIGGER_ANIMATION", "intParam": 2}` |
| `MQTT_PUBLISH` | Publishes a payload to a specific MQTT topic. | `stringParam` (topic), `stringParam2` (payload) | `{"command":"MQTT_PUBLISH", "stringParam":"home/alarm", "stringParam2":"DISARMED"}` |
| `DISPLAY_HA_SENSOR` | Fetches and displays the state of a Home Assistant entity. The device must be subscribed to the entity's state topic. | `stringParam` (entity_id), `targetSegment` | `{"command":"DISPLAY_HA_SENSOR", "stringParam":"sensor.outside_temp", "targetSegment":0}` |

---

### **Built-in Animations**

The firmware includes a collection of pre-programmed, multi-track animations that can be triggered by sending their `Animation Name` as a plain string payload to the `.../sequencer/command` MQTT topic.

*   **Example Payload**: `"Lightning"`

> **✨ Special Animation: `Randomize All`**
> Sending a payload of `"Randomize All"` will cause the device to pick one of the other animations from this list at random and run it. This is a great way to add variety to your automations.

#### **Available Animations**

| Animation Name | Description |
| :--- | :--- |
| `Time Circuits Lock-In` | The classic BTTF effect. All three rows simultaneously scramble and lock in the current time, character by character. |
| `Lightning` | A chaotic, multi-stage lightning storm effect with loud crackling sounds and intense, random flashes across all displays. |
| `Scanner` | A Cylon-style red scanner (`---`) that sweeps back and forth across all three display rows in unison. |
| `Time Travel Tunnel` | Simulates traveling through a time vortex by repeatedly scrolling the current time in from the right on all three rows. |
| `Flux Capacitor Overload` | All displays pulse with intense energy, simulating a Flux Capacitor overload with a synchronized, slow pulsing effect. |
| `Fire Trails` | "Burns" the current time onto the display with a fiery `WIPE` effect that reveals the text from left to right on all three rows. |
| `Sparkle Reveal` | A subtle reveal where the time appears out of a field of sparkling lights. The display flickers with random dots before wiping to reveal the current time. |
| `Countdown` | Displays "COUNTDOWN" on the middle row, then shows a 10-second countdown (spelling out the numbers), ending with a "LIFTOFF!" marquee. |
| `System Error` | A system malfunction theme. The top row scrambles to show "ERROR" while the middle row scrolls "SYSTEM MALFUNCTION". |
| `Sequential Flicker` | The segments of each row appear one after the other in a quick, sequential pattern, revealing the full time. |
| `Random Flicker` | A continuous, 10-second loop of random characters glitching on a single, randomly-chosen display row. |
| `Tornado Flicker` | A chaotic animation where random characters flicker up and down the display columns, creating a "tornado" effect. |
| `Capacitor Charge Up` | All three rows fill up with a `BAR_GRAPH` effect from left to right, as if a capacitor is charging. |
| `Waveform Collapse` | Displays a symmetrical waveform pattern that collapses and expands across all three rows. |
| `Code Breaker` | A slower, more deliberate version of the `Time Circuits Lock-In`, revealing the time with a much slower scramble effect. |
| `Flip Disc Display` | Simulates an old-school flip-disc (or Solari) board, revealing the time with a `WIPE` effect on all three rows. |
| `Electric Surge` | A rapid series of bright `FLASH` effects that cascade down the displays from top to bottom. |
| `Digital Rain` | Fills all displays with continuously flickering random characters for 10 seconds, similar to the Matrix effect. |

---

### **Using the Sequencer with Home Assistant**

The real power of the sequencer is unlocked when you integrate it with Home Assistant. By using the `mqtt.publish` service in your automations, you can make the clock react to any event, sensor, or trigger in your smart home.

Here are a few examples to get you started.

#### **1. The Easy Way: Triggering Built-in Animations**

The simplest method is to trigger one of the [Built-in Animations](#built-in-animations). This is perfect for common alerts and effects.

**Use Case:** Create a "Lightning" effect when a door sensor is tripped while your alarm is armed.

**Automation Example:**
```yaml
alias: Trigger Lightning on Break-in
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
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequence/command"
      payload: "Lightning"
```

#### **2. The Powerful Way: Crafting Custom JSON Sequences**

For ultimate flexibility, you can build your own sequences directly in your automation's YAML. This allows you to use Home Assistant templates to include dynamic data from your entities right in the display.

**Use Case:** Display the outside temperature on the top row when it drops below freezing.

**Automation Example:**
```yaml
alias: Display Freezing Temperature Alert
trigger:
  - platform: numeric_state
    entity_id: sensor.outside_temperature
    below: 0
action:
  - service: mqtt.publish
    data_template:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequence/command"
      payload: >
        [
          {
            "targetRow": 0,
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
```

#### **3. Advanced Integration: The `DISPLAY_HA_SENSOR` Command**

For a more direct integration, the `DISPLAY_HA_SENSOR` command tells the clock to fetch the state of an entity itself. This is useful for simple, non-templated displays.

**Use Case:** Briefly show the current power consumption on the middle row's "Year" segment.

**Automation Example:**
```yaml
alias: Flash Power Usage
trigger:
  - platform: state
    entity_id: sensor.smart_plug_power
    to: 'on'
action:
  - service: mqtt.publish
    data:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequence/command"
      payload: >
        [
          {
            "targetRow": 1,
            "commands": [
              {
                "command": "DISPLAY_HA_SENSOR",
                "stringParam": "sensor.total_power_consumption",
                "targetSegment": 2
              },
              {
                "command": "WAIT",
                "intParam": 10000
              },
              {
                "command": "RESTORE_ROW"
              }
            ]
          }
        ]
```

#### **More Automation Examples**

Here are some more copy-paste-ready examples to inspire your own creations.

##### **Calendar Event Reminder**
This automation triggers 10 minutes before an event on your calendar and scrolls the event's summary on the top row.
```yaml
alias: Display Calendar Event Reminder
trigger:
  - platform: calendar
    event: start
    offset: "-0:10:00"
    entity_id: calendar.your_calendar # <-- Change this to your calendar entity
action:
  - service: mqtt.publish
    data_template:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequence/command"
      payload: >
        [
          {
            "targetRow": 0,
            "commands": [
              {
                "command": "SOUND",
                "stringParam": "/REMINDER.mp3"
              },
              {
                "command": "MARQUEE",
                "stringParam": "EVENT: {{ trigger.calendar_event.summary }}"
              }
            ]
          }
        ]
```

##### **Laundry Cycle Finished**
This automation uses a power-monitoring smart plug to detect when a washing machine has finished its cycle (i.e., power usage drops to near-zero for a few minutes). It then scrolls a message and plays a sound.
```yaml
alias: Notify When Laundry is Done
trigger:
  - platform: numeric_state
    entity_id: sensor.washing_machine_power # <-- Change this to your power sensor
    below: 2.5
    for:
      minutes: 2
condition:
  - condition: numeric_state
    entity_id: sensor.washing_machine_power
    above: 100 # Only trigger if it was previously running
action:
  - service: mqtt.publish
    data:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequence/command"
      payload: >
        [
          {
            "targetRow": 2,
            "commands": [
              {
                "command": "SOUND",
                "stringParam": "/CHIME.mp3"
              },
              {
                "command": "MARQUEE",
                "stringParam": "LAUNDRY CYCLE COMPLETE"
              }
            ]
          }
        ]
```

##### **Garbage Day Reminder**
This automation triggers every Wednesday at 7:00 PM and scrolls a reminder to take out the trash on the bottom display.
```yaml
alias: Weekly Garbage Day Reminder
trigger:
  - platform: time
    at: "19:00:00"
condition:
  - condition: time
    weekday:
      - wed # Trigger on Wednesdays
action:
  - service: mqtt.publish
    data:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequence/command"
      payload: >
        [
          {
            "targetRow": 2,
            "commands": [
              {
                "command": "MARQUEE",
                "stringParam": "REMINDER: TAKE OUT THE TRASH"
              }
            ]
          }
        ]
```