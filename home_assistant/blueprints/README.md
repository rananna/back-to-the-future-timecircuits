# BTTF Time Circuits - Home Assistant Blueprints

This collection of Home Assistant **script blueprints** makes it easy to create and run custom animation sequences on your BTTF Time Circuits device. They provide a user-friendly form-based interface, so you can build complex animations without writing any JSON.

The blueprints have been consolidated for a simpler, more powerful user experience.

## Table of Contents

- [Installation](#installation)
- [Using Templates to Display Entity Data](#using-templates-to-display-entity-data)
- [Core Concepts](#core-concepts)
- [Available Blueprints](#available-blueprints)
  - [Universal Effect Generator](#universal-effect-generator)
  - [Multi-Track Sequence Builder](#multi-track-sequence-builder)
  - [Trigger Built-in Animation](#trigger-built-in-animation)
- [Sequencer Command Reference](#sequencer-command-reference)

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

*   **Example:** To create a dynamic temperature display, you would create an Automation that triggers whenever `sensor.outside_temperature` changes its state. The automation's action would be to call the "Universal Effect Generator" script you created. This way, every time the temperature updates, the script re-runs and sends the new value to the display.

---

## Available Blueprints

This section details each blueprint, its purpose, and its configuration options.

### Universal Effect Generator
* **File:** [`bttf_effect_generator.yaml`](bttf_effect_generator.yaml)
* **Description:** This is the new, all-in-one tool for most common effects. It replaces numerous older blueprints. You simply select the effect you want from a dropdown menu, and the blueprint will dynamically show only the relevant options. It can display static text, sensor values, run marquees, countdowns, and much more.
* **When to use it:** This should be your default choice for almost any single-row effect or notification.

**Inputs:**
*   `effect`: A dropdown list to select the primary effect (e.g., `Set Text`, `Marquee`, `Countdown`, `Pulse`, `Scramble`, etc.).
*   `...`: Based on the selected `effect`, other options like `text_to_display`, `entity_to_display`, `speed`, `duration`, and `restore_row` will appear.

**Example: Display a Sensor Value**
This service call will put "Temp: 21.5°C" on the top row and leave it there. It's best used inside an automation that triggers when `sensor.outside_temperature` changes.
```yaml
- service: script.your_script_name_here # e.g., script.display_temperature
  data:
    target_row: "TOP"
    effect: "SET_TEXT"
    entity_to_display: sensor.outside_temperature
    prefix: "Temp: "
    postfix: "°C"
    restore_row: false
```

**Example: Calendar Alert Marquee**
This service call, used in an automation that triggers on a calendar event, will scroll the event's summary on the middle row.
```yaml
- service: script.your_script_name_here # e.g., script.calendar_alert_marquee
  data:
    target_row: "MIDDLE"
    effect: "MARQUEE"
    text_to_display: "EVENT: {{ trigger.calendar_event.summary }}"
    sound: "REMINDER.mp3"
```

**Example: "Washer Done" Notification**
This automation would trigger when a smart plug monitoring your washing machine drops in power. It then flashes "WASHER" on the bottom row.
```yaml
- service: script.your_script_name_here # e.g., script.washer_done_alert
  data:
    target_row: "BOTTOM"
    effect: "FLASH"
    text_to_display: "WASHER"
    duration: 10000 # Flash for 10 seconds
    sound: "CHIME.mp3"
    restore_row: true
```

---

### Multi-Track Sequence Builder
* **File:** [`bttf_multi_track_builder.yaml`](bttf_multi_track_builder.yaml)
* **Description:** The ultimate power-user tool. This blueprint lets you define separate command sequences for each of the three display rows, allowing you to run complex animations in parallel. This blueprint is for advanced users comfortable with the device's sequencer commands.
* **When to use it:** For creating highly choreographed sequences where multiple things are happening at once.

**Inputs:**
*   `track_1_commands`: A JSON-formatted array of commands for the Top row.
*   `track_2_commands`: A JSON-formatted array of commands for the Middle row.
*   `track_3_commands`: A JSON-formatted array of commands for the Bottom row.

**Example: Intruder Alert**
This sequence scrolls an alert on the top row while simultaneously flashing the bottom row.
```yaml
- service: script.your_script_name_here # e.g., script.intruder_alert_sequence
  data:
    track_1_commands: >
      [
        {"command": "MARQUEE", "stringParam": "INTRUDER ALERT"},
        {"command": "WAIT", "intParam": 10000},
        {"command": "RESTORE_ROW"}
      ]
    track_3_commands: >
      [
        {"command": "FLASH", "intParam": 10000},
        {"command": "RESTORE_ROW"}
      ]
```

---

### Trigger Built-in Animation
* **File:** [`bttf_trigger_animation_generator.yaml`](bttf_trigger_animation_generator.yaml)
* **Description:** A simple blueprint with a single dropdown to run the device's cool, pre-programmed, multi-track animations like "Lightning" or "Time Travel".
* **When to use it:** For easily triggering the most complex and cinematic effects the device has to offer.

**Inputs:**
*   `animation`: The built-in animation to trigger.

**Example: Trigger the Lightning Animation**
```yaml
- service: script.your_script_name_here # e.g., script.trigger_lightning
  data:
    animation: "Lightning"
```

---

## Sequencer Command Reference

The **Multi-Track Sequence Builder** blueprint exposes the full power of the device's command sequencer. For a complete list of all available commands (like `SET_TEXT`, `PULSE`, `COUNTDOWN`, etc.) and their parameters, please see the canonical documentation here:

**[Developer Docs: Sequencer API Command Reference](../../docs/developer/sequencer-api.md#command-reference)**