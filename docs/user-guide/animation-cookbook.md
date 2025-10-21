# 🎬 The Animation Cookbook

Welcome to the Animation Cookbook! This guide is for advanced users who want to move beyond the built-in animations and create their own custom sequences from scratch. Here, you'll learn the secrets of the **Command Sequencer**, the powerful engine that drives all the complex animations in the Time Circuits firmware.

This guide assumes you have a working knowledge of MQTT and JSON. For a complete reference of all available MQTT topics and sequencer commands, please see the **[🤖 MQTT API Reference](../developer/mqtt-api.md)**.

## Core Concept: The Sequencer

The sequencer allows you to define a series of commands that are executed in order on a specific display row. You can even run multiple sequences on different rows **in parallel**, allowing for highly complex, multi-layered animations.

A sequence is defined as a JSON payload and sent to the `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command` MQTT topic.

---

## Example 1: A Simple, Multi-Step Message

Let's create a sequence that displays "HELLO WORLD" on the middle row for 3 seconds, makes it flash, and then clears the display.

#### The Goal:
1.  Show "HELLO WORLD" on the middle row.
2.  Wait for 3 seconds.
3.  Flash the text for 2 seconds.
4.  Clear the middle row.

#### The Tool:
We'll use an MQTT client like [MQTT Explorer](http://mqtt-explorer.com/) to publish our JSON payload.

#### The JSON Payload:
This is a **single-track** sequence. We send a JSON object.

```json
{
  "targetRow": "MIDDLE",
  "commands": [
    {
      "command": "SET_TEXT",
      "stringParam": "HELLO WORLD"
    },
    {
      "command": "WAIT",
      "intParam": 3000
    },
    {
      "command": "FLASH",
      "intParam2": 2000
    },
    {
      "command": "RESTORE_ROW"
    }
  ]
}
```

#### Breakdown:
*   `"targetRow": "MIDDLE"`: Specifies that all commands in this sequence will run on the middle display row.
*   `"command": "SET_TEXT"`: Instantly displays the text from `stringParam`.
*   `"command": "WAIT"`: Pauses the sequence for 3000ms.
*   `"command": "FLASH"`: A blocking command that flashes the current text on the row for 2000ms.
*   `"command": "RESTORE_ROW"`: Clears the custom text and restores the row to its normal state (e.g., the clock).

---

## Example 2: A Multi-Row, Parallel Animation

Now for something more advanced. Let's create a "System Alert" that runs different effects on all three rows at the same time.

#### The Goal:
*   **Top Row**: Display a flashing "WARNING" message.
*   **Middle Row**: Show a "charge-up" bar graph.
*   **Bottom Row**: Scroll the text "SYSTEM ALERT" across the display.
*   **Audio**: Play an alarm sound at the beginning.

#### The JSON Payload:
This is a **parallel-track** sequence. We send a JSON **array** of track objects.

```json
[
  {
    "targetRow": "TOP",
    "commands": [
      {
        "command": "SOUND",
        "stringParam": "/alarm.mp3"
      },
      {
        "command": "SET_TEXT",
        "stringParam": "WARNING"
      },
      {
        "command": "FLASH",
        "intParam2": 5000
      },
      {
        "command": "RESTORE_ROW"
      }
    ]
  },
  {
    "targetRow": "MIDDLE",
    "commands": [
      {
        "command": "BAR_GRAPH",
        "intParam": 0,
        "intParam2": 4000
      },
      {
        "command": "WAIT",
        "intParam": 1000
      },
      {
        "command": "RESTORE_ROW"
      }
    ]
  },
  {
    "targetRow": "BOTTOM",
    "commands": [
      {
        "command": "MARQUEE",
        "stringParam": "SYSTEM ALERT"
      },
      {
        "command": "RESTORE_ROW"
      }
    ]
  }
]
```

#### Breakdown:
*   **The Root is an Array `[]`**: This tells the sequencer to run the enclosed track objects in parallel.
*   **Track 1 (Top Row)**:
    *   It starts by playing a `SOUND` effect. This is a non-blocking command, so the animation continues immediately.
    *   It then flashes the word "WARNING" for 5 seconds.
*   **Track 2 (Middle Row)**:
    *   It runs a `BAR_GRAPH` that takes 4 seconds to fill.
    *   We add a 1-second `WAIT` at the end to ensure this track also lasts for 5 seconds, keeping it synchronized with the top row.
*   **Track 3 (Bottom Row)**:
    *   It runs a `MARQUEE` effect. This is a blocking command that will run to completion. For our text, this will take approximately 5 seconds.
*   **Synchronization**: The `FLASH` and `BAR_GRAPH` commands are timed to last for a similar duration. The `MARQUEE` command will also run for about the same time. This keeps the overall animation feeling cohesive. All three tracks finish at roughly the same time and then restore their respective rows.

---

## Pro-Tip: Building Reusable Sequences in Home Assistant

You can use this knowledge to create incredibly powerful, reusable scripts in Home Assistant by using the `mqtt.publish` service.

Here is an example of a Home Assistant script that takes a `message` and `target_row` as variables and runs a custom "scramble-reveal" effect.

```yaml
alias: Time Circuits - Scramble Reveal Text
fields:
  message:
    description: The text you want to display.
    example: "SYSTEMS ONLINE"
  target_row:
    description: The row to display the message on.
    example: "TOP"
sequence:
  - service: mqtt.publish
    data:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command"
      payload: >-
        {
          "targetRow": "{{ target_row }}",
          "commands": [
            {
              "command": "SCRAMBLE_TEXT",
              "stringParam": "{{ message }}",
              "intParam": 50,
              "intParam2": 2000
            },
            {
              "command": "WAIT",
              "intParam": 3000
            },
            {
              "command": "RESTORE_ROW"
            }
          ]
        }
mode: single
```

Now, from any automation, you can call this script and pass in your desired text and target row, creating a dynamic and reusable animation effect.

```yaml
# Example automation action
- service: script.time_circuits_scramble_reveal_text
  data:
    message: "SHIELDS UP"
    target_row: "BOTTOM"
```
