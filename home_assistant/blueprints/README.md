# BTTF Time Circuits - Home Assistant Blueprints

This collection of Home Assistant **script blueprints** makes it easy to create and run custom animation sequences on your BTTF Time Circuits device. They provide a user-friendly form-based interface, so you can build complex animations without writing any JSON or code.

## Table of Contents

- [Installation](#installation)
- [Using Templates to Display Entity Data](#using-templates-to-display-entity-data)
- [Core Concepts](#core-concepts)
- [Available Blueprints](#available-blueprints)
  - [Universal Effect Generator](#universal-effect-generator)
  - [Multi-Track Sequence Builder (Advanced)](#multi-track-sequence-builder-advanced)
  - [Trigger Built-in Animation](#trigger-built-in-animation)
- [Full Sequencer Command Reference](#full-sequencer-command-reference)

---

## Installation

1.  **Copy Blueprints**: Copy the `.yaml` files from this directory into the `/config/blueprints/script/` directory of your Home Assistant installation. The Samba share, FTP, or File Editor add-ons are useful for this.
2.  **Reload Blueprints**: In the Home Assistant UI, navigate to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the bottom-right corner and select **Reload Blueprints**.
3.  **Create a Script**: Your new blueprints will appear in the list. Click **Create Script** next to the one you wish to use. This configured script can then be run from your dashboards or called as a service in other automations.

---

## Using Templates to Display Entity Data

A key feature of these blueprints is the ability to use [Home Assistant templates](https://www.home-assistant.io/docs/configuration/templating/) in any text field. This allows you to create dynamic messages that include sensor values, device states, or attributes.

**Example 1: Displaying a sensor value**
```jinja
The temperature is {{ states('sensor.outside_temperature') }}°C
```

**Example 2: Creating a dynamic alert message**
```jinja
Alert: The {{ trigger.to_state.name }} was opened!
```

---

## Core Concepts

It's important to understand two key concepts about how these blueprints work:

**1. Scripts are "One-Shot"**

Every blueprint creates a Home Assistant **Script**. When you run a script (either manually or from an automation), it executes its sequence of commands once and then stops. It does not continuously run in the background.

*   **Example:** If you use the "Universal Effect Generator" to show the temperature, it will fetch the temperature *at that moment*, display it, and finish. The display will not automatically update if the temperature changes later.

**2. Automations are for Dynamic Updates**

To make the display dynamic and responsive to your home, you must use **Automations** to trigger these scripts. An automation can watch for a specific event (like a sensor changing or a door opening) and then run the corresponding script.

*   **Example:** To create a dynamic temperature display, you would create an Automation that triggers whenever `sensor.outside_temperature` changes its state. The automation's action would be to call the script you created. This way, every time the temperature updates, the script re-runs and sends the new value to the display.

---

## Available Blueprints

### Universal Effect Generator
* **File:** [`bttf_effect_generator.yaml`](bttf_effect_generator.yaml)
* **Description:** This is your new one-stop-shop for creating most single-track animations. It consolidates over seven of the old blueprints into one. Simply select the effect you want from a dropdown list, and the blueprint will intelligently show you only the options relevant to that effect. It can display static text, sensor values, run visual effects, and more. For effects with a defined duration (like `Marquee` or `Countdown`), the blueprint automatically calculates how long to wait before restoring the row, simplifying your automations.
* **When to use it:** This should be your default choice for 90% of use cases, including displaying text, showing sensor data, and running effects like `Marquee`, `Scramble`, `Pulse`, or `Countdown`.

**Example Usage (Displaying a Sensor with a Marquee):**
```yaml
# Creates a script that scrolls a sensor's value on the top row.
# Use this script in an automation that triggers when the sensor changes.
- service: script.bttf_time_circuits_universal_effect_generator
  data:
    target_row: "TOP"
    effect: "MARQUEE"
    text_to_display: "Now playing: {{ state_attr('media_player.living_room', 'media_title') }}"
    speed: 150
    restore_row: true
```

**Example Usage (Countdown):**
```yaml
# Creates a script that runs a 10-second countdown on the middle row
# and displays "LIFTOFF" at the end.
- service: script.bttf_time_circuits_universal_effect_generator
  data:
    target_row: "MIDDLE"
    effect: "COUNTDOWN"
    countdown_start: 10
    countdown_delay: 1000
    end_text: "LIFTOFF"
    restore_row: false # Leave "LIFTOFF" on the display
```

---

### Multi-Track Sequence Builder (Advanced)
* **File:** [`bttf_multi_track_builder.yaml`](bttf_multi_track_builder.yaml)
* **Description:** The ultimate power-user tool. This blueprint lets you define separate command sequences for each of the three display rows, allowing you to run complex animations in parallel. You provide a JSON-formatted array of commands for each track.
* **When to use it:** For creating highly choreographed sequences where multiple things are happening at once, such as showing scrolling text on one row, a countdown on another, and a flashing light on the third—all at the same time. This blueprint is for advanced users comfortable with the device's sequencer commands.

**Example Usage (Intruder Alert):**
```yaml
# This creates a complex alert with a scrolling message on top,
# a countdown in the middle, and a flashing effect on the bottom.
- service: script.bttf_time_circuits_multi_track_sequence_builder
  data:
    track_1_commands: >
      [
        {"command": "MARQUEE", "targetRow": "TOP", "stringParam": "INTRUDER ALERT", "intParam": 100},
        {"command": "WAIT", "intParam": 10000},
        {"command": "RESTORE_ROW", "targetRow": "TOP"}
      ]
    track_2_commands: >
      [
        {"command": "COUNTDOWN", "targetRow": "MIDDLE", "intParam": 10, "intParam2": 1000},
        {"command": "RESTORE_ROW", "targetRow": "MIDDLE"}
      ]
    track_3_commands: >
      [
        {"command": "FLASH", "targetRow": "BOTTOM", "intParam": 10000},
        {"command": "RESTORE_ROW", "targetRow": "BOTTOM"}
      ]
```

---

### Trigger Built-in Animation
* **File:** [`bttf_trigger_animation_generator.yaml`](bttf_trigger_animation_generator.yaml)
* **Description:** A simple blueprint with a single dropdown to run the device's cool, pre-programmed, multi-track animations like `TimeTravel` or `IntruderAlert`.
* **When to use it:** For easily triggering the most complex and cinematic effects the device has to offer without any custom configuration.

---

## Full Sequencer Command Reference

For a complete and up-to-date list of all available sequencer commands and their parameters for use with the **Multi-Track Sequence Builder**, please refer to the canonical documentation in the main project repository:

[**>> Sequencer API Developer Reference**](../../docs/developer/sequencer-api.md)

Linking to the developer documentation ensures you always have the most accurate information without it becoming outdated here.