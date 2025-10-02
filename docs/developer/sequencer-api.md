# 🤖 Command Sequencer Deep Dive

The command sequencer is one of the most powerful features of the Time Circuits clock, allowing you to script complex, multi-step, and even parallel animations. You can create custom alerts, intricate visual effects, and timed sequences to integrate the clock into your smart home in creative ways.

This guide provides a complete reference for the sequencer's capabilities, including the payload structure, a full list of commands, and the available named sequences.

---

### **MQTT API Endpoint**

*   **Topic**: `bttf-time-circuits/[DEVICE_ID]/sequence/command`
*   **Payload**: A JSON array of *track objects*, or a string with a [Named Sequence](#named-sequences).

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
| `MARQUEE` | Scrolls text across the entire target row. | `stringParam`, `targetSegment` (optional, default: -1) | `{"command":"MARQUEE", "stringParam":"A VERY LONG MESSAGE"}` |
| `SCRAMBLE_TEXT` | Reveals text one character at a time with a scrambling effect. | `stringParam`, `targetSegment` (optional, default: -1), `intParam` (flicker speed), `intParam2` (reveal delay) | `{"command":"SCRAMBLE_TEXT", "stringParam":"ACCESSING", "intParam":50, "intParam2":150}` |
| `TYPEWRITER` | Reveals text one character at a time, like a typewriter. | `stringParam`, `targetSegment` (optional, default: -1), `intParam` (delay) | `{"command":"TYPEWRITER", "stringParam":"LOADING...", "intParam":100}` |
| `WIPE` | Reveals text with a wipe effect from left to right. | `stringParam`, `intParam` (delay) | `{"command":"WIPE", "stringParam":"AUTHORIZED", "intParam":75}` |
| `SCROLL_IN` | Scrolls text in from the right and stops. | `stringParam`, `intParam` (delay) | `{"command":"SCROLL_IN", "stringParam":"WELCOME", "intParam":60}` |
| `CROSSFADE_TEXT` | Fades from the current text to new text. | `stringParam`, `targetSegment` (optional, default: -1), `intParam` (duration) | `{"command":"CROSSFADE_TEXT", "stringParam":"NEW TEXT", "intParam":1500}` |
| `RANDOM_FLICKER_TEXT` | Fills the display with random characters that flicker rapidly. | `stringParam` (ignored), `intParam` (duration), `intParam2` (flicker speed) | `{"command":"RANDOM_FLICKER_TEXT", "intParam":5000, "intParam2":50}` |
| `BAR_GRAPH` | Displays a "charging" bar that fills from left to right. | `stringParam` (label), `intParam` (start %), `intParam2` (duration) | `{"command":"BAR_GRAPH", "stringParam":"LOAD", "intParam":0, "intParam2":3000}` |
| `SCANNER` | Creates a back-and-forth scanning light effect ("Knight Rider"). | `stringParam` (character), `intParam` (duration), `intParam2` (speed) | `{"command":"SCANNER", "stringParam":"-", "intParam":10000, "intParam2":50}` |
| `COUNTDOWN` | Displays a countdown from a set number. | `stringParam` (prefix), `targetSegment` (optional, default: -1), `intParam` (start number), `intParam2` (delay per number) | `{"command":"COUNTDOWN", "stringParam":"T-", "intParam":10, "intParam2":1000}` |
| `CLEAR_SEGMENT` | Clears the text from a specific segment or the entire row. | `targetSegment` (optional, default: 0) | `{"command":"CLEAR_SEGMENT", "targetSegment": 1}` |
| `RESTORE_ROW` | Restores the target row to its normal display (clock, weather, etc.). | (none) | `{"command":"RESTORE_ROW"}` |
| `RESTORE_ALL_ROWS` | Restores all three display rows to their normal function. | (none) | `{"command":"RESTORE_ALL_ROWS"}` |

---

#### **Effects and Utility Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `FADE_IN` | Fades the global brightness from 0 to the current setting. | `intParam` (duration) | `{"command":"FADE_IN", "intParam":2000}` |
| `FADE_OUT`| Fades the global brightness from the current setting to 0. | `intParam` (duration) | `{"command":"FADE_OUT", "intParam":2000}` |
| `PULSE` | Makes a segment (or row) blink slowly. | `targetSegment`, `intParam` (duration) | `{"command":"PULSE", "targetSegment":-1, "intParam":5000}` |
| `FLASH` | Makes a segment (or row) flash brightly and rapidly. | `targetSegment`, `intParam` (duration) | `{"command":"FLASH", "targetSegment":2, "intParam":1000}` |
| `SET_BRIGHTNESS` | Sets the global display brightness. | `intParam` (0-7) | `{"command":"SET_BRIGHTNESS", "intParam":7}` |
| `SOUND` | Plays a sound effect from the device's filesystem. | `stringParam` (path, e.g., `/REMOTE.mp3`) | `{"command":"SOUND", "stringParam":"/CONFIRM_ON.mp3"}` |
| `WAIT` | Pauses the current track for a set amount of time. | `intParam` (duration) | `{"command":"WAIT", "intParam":500}` |
| `LOOP_START` | Marks the beginning of a loop. | `intParam` (number of loops) | `{"command":"LOOP_START", "intParam":5}` |
| `LOOP_END` | Marks the end of a loop, jumping back to `LOOP_START`. | (none) | `{"command":"LOOP_END"}` |

---

#### **Advanced & Integration Commands**

| Command | Description | Parameters | Example |
| :--- | :--- | :--- | :--- |
| `TRIGGER_ANIMATION` | Stops the sequencer and runs one of the built-in cinematic animations. | `intParam` (AnimationType enum value) | `{"command":"TRIGGER_ANIMATION", "intParam":1}` |
| `MQTT_PUBLISH` | Publishes a payload to a specific MQTT topic. | `stringParam` (topic), `stringParam2` (payload) | `{"command":"MQTT_PUBLISH", "stringParam":"home/alarm", "stringParam2":"DISARMED"}` |
| `DISPLAY_HA_SENSOR` | Fetches and displays the state of a Home Assistant entity. | `stringParam` (entity_id), `targetSegment` | `{"command":"DISPLAY_HA_SENSOR", "stringParam":"sensor.outside_temp", "targetSegment":0}` |

---

### **Payload Structure: Named Sequences**

For convenience, a number of pre-programmed sequences can be triggered simply by sending the name of the sequence as a plain string payload (not JSON).

*   **Payload**: `"Intruder Alert"`

#### **Available Named Sequences**

| Name | Description |
| :--- | :--- |
| `Intruder Alert` | A multi-row alert with marquees, scrambles, and sounds. |
| `Time Travel` | Simulates the 88MPH time travel sequence with sounds and effects. |
| `Party Mode` | A looping sequence with pulsing lights and scrolling text. |
| `Countdown` | A 10-second countdown on the middle row, ending with a sound. |
| `Knight Rider` | A classic red scanner effect on the middle or bottom row. |
| `Cylon` | A wider, slower, red scanner effect. |
| `Lightning` | Simulates a lightning storm with flashes and crackle sounds across all displays. |
| `Loading` | A sequential boot-up style text animation. |
| `Error` | A system malfunction theme with scrambled text and error sounds. |
| `Flux Capacitor Charge-Up` | A charging bar on the bottom row with sound and light effects. |
| `Tachyons Detected` | A sci-fi themed text scramble effect. |
| `Data Stream` | Fills all displays with rapidly flickering random characters. |
| `Wormhole Collapse` | A multi-row fade-out and flicker effect. |

#### **Debug Sequences**
These are primarily for development and testing but can be triggered by anyone.
*   `Debug`: Runs a basic test of the sequencer.
*   `DebugEffects`: Showcases a variety of visual effects sequentially.
*   `DebugParallelLogic`: Tests the parallel execution of logic commands.
*   `DebugStress`: Triggers a chaotic sequence of random animations to stress-test the system.
*   `CrossfadeTest`: Specifically demonstrates the `CROSSFADE_TEXT` command.

---

### **Using the Sequencer with Home Assistant**

The real power of the sequencer is unlocked when you integrate it with Home Assistant. By using the `mqtt.publish` service in your automations, you can make the clock react to any event, sensor, or trigger in your smart home.

Here are a few examples to get you started.

#### **1. The Easy Way: Triggering Named Sequences**

The simplest method is to trigger one of the built-in [Named Sequences](#available-named-sequences). This is perfect for common alerts and effects.

**Use Case:** Create an "Intruder Alert" when a door sensor is tripped while your alarm is armed.

**Automation Example:**
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
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequence/command"
      payload: "Intruder Alert"
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