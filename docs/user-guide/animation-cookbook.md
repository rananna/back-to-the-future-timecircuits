# Animation Cookbook: A Guide to Creating Custom Sequences

Welcome to the Animation Cookbook! This guide will teach you how to use the powerful built-in sequencer to create your own custom animations and visual effects for your Time Circuits clock.

## Table of Contents
- [Introduction: What is the Sequencer?](#introduction-what-is-the-sequencer)
- [The Basics: Tracks and Commands](#the-basics-tracks-and-commands)
- [How to Trigger a Custom Animation](#how-to-trigger-a-custom-animation)
- [Command Reference](#command-reference)
- [Cookbook Recipes: Practical Examples](#cookbook-recipes-practical-examples)
  - [Recipe 1: Simple "Hello World"](#recipe-1-simple-hello-world)
  - [Recipe 2: Multi-Line Welcome Message](#recipe-2-multi-line-welcome-message)
  - [Recipe 3: "Warning" with Sound and Flashing Lights](#recipe-3-warning-with-sound-and-flashing-lights)
  - [Recipe 4: Looping Knight Rider Scanner](#recipe-4-looping-knight-rider-scanner)

---

## Introduction: What is the Sequencer?

The sequencer is a powerful engine inside your Time Circuits clock that allows you to create complex, multi-step animations. Think of it like a script for a movie scene: you define a series of commands that happen in order, telling the displays exactly what to do and when. You can control text, create visual effects, play sounds, and even run multiple effects on different display rows at the same time!

This guide will show you how to write those scripts.

---
### A Simpler Way: Home Assistant Blueprints

**Do you use Home Assistant?** If so, you have a much simpler, no-code way to create powerful animations!

This cookbook is for users who want to create highly custom animations by writing JSON "scripts" and sending them via MQTT. This is a powerful and flexible method, but it requires some technical comfort.

For a much easier, user-friendly experience, we **highly recommend** using our pre-built **Home Assistant Blueprints**. These provide a simple form-based interface right inside Home Assistant to create and trigger animations, with no coding required.

> **[➡️ Click here to learn all about the Home Assistant Blueprints](./home-assistant.md#-available-blueprints-a-deep-dive)**

If you're not a Home Assistant user, or you're a power user who wants maximum control, then this cookbook is for you!

---

## The Basics: Tracks and Commands

There are two core concepts to understand:

1.  **Track**: A track is a sequence of commands that runs on a single display row (`TOP`, `MIDDLE`, or `BOTTOM`). You can run a single track or run up to three tracks in parallel for amazing multi-line effects.
2.  **Command**: A command is a single action that the sequencer performs. Examples include `SET_TEXT` to display a message, `WAIT` to pause, or `FLASH` to make the text blink.

You create an animation by building a list of these commands for a specific track.

## How to Trigger a Custom Animation

The most common way to run your own animation is by sending a specially formatted **JSON payload** to an MQTT topic.

*   **Topic**: `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`
    *   (You must replace `YOUR_DEVICE_ID` with the actual ID of your device).
*   **Payload**: The JSON "script" for your animation.

### Payload Structure

The JSON payload defines the track (or tracks) and the list of commands.

**For a single track:**
```json
{
  "targetRow": "TOP",
  "commands": [
    { "command": "SET_TEXT", "stringParam": "HELLO" },
    { "command": "WAIT", "intParam": 5000 },
    { "command": "CLEAR_SEGMENT", "targetSegment": -1 }
  ]
}
```

**For parallel tracks (running at the same time):**
Just wrap the track objects in a JSON array `[]`.

```json
[
  {
    "targetRow": "TOP",
    "commands": [
      { "command": "SET_TEXT", "stringParam": "LINE 1" }
    ]
  },
  {
    "targetRow": "MIDDLE",
    "commands": [
      { "command": "SET_TEXT", "stringParam": "LINE 2" }
    ]
  }
]
```
> **⚠️ Important Rule for Parallel Tracks**: When you run tracks in parallel, each track object in the array **must target a unique display row**.

### Tools for Sending MQTT Commands

To send these JSON payloads to your clock, you need an MQTT client. Here are a couple of popular, user-friendly options for Windows:

*   **[MQTT Explorer](http://mqtt-explorer.com/)**: A fantastic, all-around MQTT client with a great user interface that shows you a structured overview of all topics. It's excellent for both sending commands and debugging.
*   **[MQTTX](https://mqttx.app/)**: A modern, cross-platform MQTT client with a clean interface. It makes publishing and subscribing easy and supports all the features you'll need.

**General Steps to send a command:**
1.  Download and install one of the clients above.
2.  Configure a new connection to your MQTT broker (the same one your clock is connected to).
3.  In the "Publish" section of the client, set the topic to `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`.
4.  Paste your JSON payload into the message/payload field.
5.  Click "Publish". Your animation should start immediately!

## Command Reference

This is a complete list of all commands available in the sequencer.

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

## Cookbook Recipes: Practical Examples

Here are a few "recipes" to get you started. You can send these JSON payloads directly to the MQTT topic `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`.

### Recipe 1: Simple "Hello World"
This recipe displays "HELLO WORLD" on the top row for 5 seconds, then clears the display.

```json
{
  "targetRow": "TOP",
  "commands": [
    {
      "command": "SET_TEXT",
      "stringParam": "HELLO WORLD"
    },
    {
      "command": "WAIT",
      "intParam": 5000
    },
    {
      "command": "CLEAR_SEGMENT",
      "targetSegment": -1
    }
  ]
}
```

### Recipe 2: Multi-Line Welcome Message
This recipe uses parallel tracks to display a three-line message simultaneously, holds it for 10 seconds, and then restores all the rows to the normal clock display.

```json
[
  {
    "targetRow": "TOP",
    "commands": [ { "command": "SET_TEXT", "stringParam": "WELCOME TO" } ]
  },
  {
    "targetRow": "MIDDLE",
    "commands": [ { "command": "SET_TEXT", "stringParam": "THE FUTURE" } ]
  },
  {
    "targetRow": "BOTTOM",
    "commands": [
      { "command": "SET_TEXT", "stringParam": "DOC BROWN" },
      { "command": "WAIT", "intParam": 10000 },
      { "command": "RESTORE_ROW" }
    ]
  }
]
```
> **Note**: Only one of the parallel tracks needs the `WAIT` and `RESTORE_ROW` command, since `RESTORE_ROW` in this context will restore all rows after the wait is complete.

### Recipe 3: "Warning" with Sound and Flashing Lights
This recipe creates a 10-second alert. It plays a siren sound, shows a "WARNING" message on the top row, and flashes "SHIELDS UP" on the bottom row.

```json
[
  {
    "targetRow": "TOP",
    "commands": [
      { "command": "SOUND", "stringParam": "/alarm.mp3" },
      { "command": "SET_TEXT", "stringParam": "WARNING" },
      { "command": "WAIT", "intParam": 10000 },
      { "command": "RESTORE_ROW" }
    ]
  },
  {
    "targetRow": "BOTTOM",
    "commands": [
      { "command": "SET_TEXT", "stringParam": "SHIELDS UP" },
      { "command": "FLASH", "intParam2": 10000 }
    ]
  }
]
```

### Recipe 4: Looping Knight Rider Scanner
This recipe creates a classic "Knight Rider" scanner effect on the middle row that loops 5 times.

```json
{
  "targetRow": "MIDDLE",
  "commands": [
    { "command": "LOOP_START", "intParam": 5 },
    {
      "command": "SCANNER",
      "stringParam": "=",
      "intParam": 4000,
      "intParam2": 50
    },
    { "command": "LOOP_END" }
  ]
}
```
