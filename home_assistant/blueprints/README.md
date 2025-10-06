# BTTF Time Circuits - Home Assistant Blueprints

This collection of Home Assistant **script blueprints** makes it easy to create and run custom animation sequences on your BTTF Time Circuits device. They provide a user-friendly form-based interface, so you can build complex animations without writing any JSON.

## Table of Contents

- [Installation](#installation)
- [Using Templates to Display Entity Data](#using-templates-to-display-entity-data)
- [Core Concepts](#core-concepts)
- [Available Blueprints](#available-blueprints)
  - [General Purpose](#general-purpose)
  - [Advanced](#advanced)
  - [Helpers & Integrations](#helpers--integrations)

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

**Example 3: Combining text and attributes**
```jinja
Music: {{ state_attr('media_player.living_room', 'media_title') }}
```

---

## Core Concepts

It's important to understand two key concepts about how these blueprints work:

**1. Scripts are "One-Shot"**

Every blueprint creates a Home Assistant **Script**. When you run a script (either manually or from an automation), it executes its sequence of commands once and then stops. It does not continuously run in the background.

*   **Example:** If you use the "Display Home Assistant Sensor" blueprint to show the temperature, it will fetch the temperature *at that moment*, display it, and finish. The display will not automatically update if the temperature changes later.

**2. Automations are for Dynamic Updates**

To make the display dynamic and responsive to your home, you must use **Automations** to trigger these scripts. An automation can watch for a specific event (like a sensor changing or a door opening) and then run the corresponding script.

*   **Example:** To create a dynamic temperature display, you would create an Automation that triggers whenever `sensor.outside_temperature` changes its state. The automation's action would be to call the "Display Home Assistant Sensor" script you created. This way, every time the temperature updates, the script re-runs and sends the new value to the display.

---

## Available Blueprints

This section details each blueprint, its purpose, and its configuration options.

### General Purpose
*These blueprints cover the most common and straightforward use cases.*

---

#### Simple Text Sequence
* **File:** [`bttf_simple_sequence_generator.yaml`](bttf_simple_sequence_generator.yaml)
* **Description:** The most basic blueprint. It displays a static or templated line of text on a selected row for a specific duration, then restores the row to its previous state.
* **When to use it:** Ideal for simple, direct notifications like "GARAGE DOOR OPEN" or "WELCOME HOME".

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row to show the text on (Top, Middle, or Bottom).
*   `text_to_display`: The text to show (max 13 characters, supports templates).
*   `duration`: How long the text should remain on screen (in seconds).

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_simple_text_sequence
  data:
    target_row: "MIDDLE"
    text_to_display: "MOTION DETECTED"
    duration: 10
```

---

#### Marquee Text Sequence
* **File:** [`bttf_marquee_sequence_generator.yaml`](bttf_marquee_sequence_generator.yaml)
* **Description:** Creates a scrolling text animation (a marquee) on a selected row. After the message scrolls completely, the row can be automatically restored to its previous state.
* **When to use it:** Perfect for longer messages that don't fit on the 13-character display, such as song titles, news headlines, or detailed status updates.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row to scroll the text on.
*   `text_to_display`: The text to scroll. Supports templates and can be longer than 13 characters.
*   `speed`: The delay between each step of the scroll animation (in milliseconds). Lower is faster.
*   `restore_row`: If enabled (the default), the row will be restored to its previous state after the marquee finishes.

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_marquee_text_sequence
  data:
    target_row: "TOP"
    text_to_display: "Now playing: {{ state_attr('media_player.living_room', 'media_title') }}"
    speed: 150
    restore_row: true
```

---

#### Scramble Text Sequence
* **File:** [`bttf_scramble_text_generator.yaml`](bttf_scramble_text_generator.yaml)
* **Description:** Creates the "scramble" effect where random characters flicker on the display before locking in one-by-one to reveal the final message.
* **When to use it:** A more dramatic or "high-tech" way to reveal important information.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row for the effect.
*   `text_to_display`: The final text to be revealed (max 13 characters, supports templates).
*   `lock_in_speed`: The delay between each character being revealed (in milliseconds).
*   `scramble_speed`: The speed of the random character flicker (in milliseconds).
*   `restore_row`: If enabled (the default), the row will be restored to its previous state after the animation finishes.

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_scramble_text_sequence
  data:
    target_row: "BOTTOM"
    text_to_display: "ACCESS GRANTED"
    lock_in_speed: 100
    scramble_speed: 50
    restore_row: true
```

---

#### Countdown Sequence
* **File:** [`bttf_countdown_generator.yaml`](bttf_countdown_generator.yaml)
* **Description:** Displays a numerical countdown on a selected row. Can optionally display text at the end and then restore the row.
* **When to use it:** Great for automations with a time component, such as "DISARMING IN 10..." or "SYSTEM REBOOT IN 5...".

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row for the countdown.
*   `start_number`: The number to start counting down from.
*   `end_text`: Optional text to display when the countdown finishes (e.g., "LIFTOFF").
*   `countdown_delay`: The delay between each number change (in milliseconds).
*   `restore_row`: If enabled (the default), the row will be restored to its previous state after the countdown finishes.

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_countdown_sequence
  data:
    target_row: "MIDDLE"
    start_number: 10
    end_text: "HAPPY NEW YEAR"
    countdown_delay: 1000
    restore_row: false # Keep "HAPPY NEW YEAR" on the display
```

---

#### Trigger Built-in Animation
* **File:** [`bttf_trigger_animation_generator.yaml`](bttf_trigger_animation_generator.yaml)
* **Description:** A simple blueprint with a single dropdown to run the device's cool, pre-programmed, multi-track animations.
* **When to use it:** For easily triggering the most complex and cinematic effects the device has to offer.
* **Reference:** For a full list of available animations and what they do, please see the **[Built-in Animations Reference Table](../../docs/developer/sequencer-api.md#built-in-animations)** in the developer documentation.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `animation`: The built-in animation to trigger.

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_trigger_built_in_animation
  data:
    animation: "Lightning"
```

---

### Advanced
*These blueprints provide more powerful and creative control over the device's animations.*

---

#### Visual Effects Generator
* **File:** [`bttf_visual_effects_generator.yaml`](bttf_visual_effects_generator.yaml)
* **Description:** A themed blueprint that groups several creative text-based visual effects into one place. For effects with a natural end (like Typewriter), the blueprint automatically calculates the duration. For continuous effects (like Scanner), you must provide a manual duration.
* **When to use it:** For adding more creative flair to your text displays beyond the standard animations.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row for the effect.
*   `effect`: The visual effect to use (`Typewriter`, `Crossfade Text`, `Scanner`, `Bar Graph`).
*   `text_to_display`: The text for the effect (not used by Scanner).
*   `typewriter_delay`: [Typewriter] The delay between each character appearing (ms).
*   `crossfade_duration`: [Crossfade] The duration of the fade effect (ms).
*   `bargraph_percentage`: [Bar Graph] The percentage (0-100) at which the bar graph should start.
*   `bargraph_duration`: [Bar Graph] The total time for the bar graph animation to complete (ms).
*   `restore_row`: If enabled, restores the row after the effect.
*   `duration`: **[Scanner only]** If `restore_row` is enabled, this is how long the continuous effect will run before the row is restored (in seconds). This setting is ignored by other effects.

**Example Usage (Scanner):**
```yaml
- service: script.bttf_time_circuits_visual_effects_generator
  data:
    target_row: "MIDDLE"
    effect: "SCANNER"
    restore_row: true
    duration: 10 # Let the scanner run for 10 seconds before restoring the row
```

---

#### Row Effects Generator
* **File:** [`bttf_row_effects_generator.yaml`](bttf_row_effects_generator.yaml)
* **Description:** Runs a one-shot, attention-grabbing effect on an entire row, like a slow pulse or a rapid flash, for a specific duration.
* **When to use it:** For creating temporary, noticeable alerts, like making a row flash for 5 seconds when an alarm is triggered.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row for the effect.
*   `effect`: The effect to apply (`PULSE`, `FLASH`).
*   `duration`: How long the effect should run before the row is automatically restored (in seconds).

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_row_effects_generator
  data:
    target_row: "BOTTOM"
    effect: "FLASH"
    duration: 5
```

---

#### Multi-Track Advanced Builder
* **File:** [`bttf_multi_track_advanced_builder.yaml`](bttf_multi_track_advanced_builder.yaml)
* **Description:** The ultimate power-user tool. This blueprint lets you define separate command sequences for each of the three display rows, allowing you to run complex animations in parallel. This blueprint is for advanced users comfortable with the device's sequencer commands.
* **When to use it:** For creating highly choreographed sequences where multiple things are happening at once, such as showing scrolling text on one row, a countdown on another, and a flashing light on the third—all at the same time.
* **Reference:** This blueprint uses the full power of the device's command sequencer. For a complete list of commands and their parameters, please see the **[Command Reference](../../docs/developer/sequencer-api.md#command-reference)** in the developer documentation.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `track_1_commands`: A JSON-formatted array of commands for the Top row.
*   `track_2_commands`: A JSON-formatted array of commands for the Middle row.
*   `track_3_commands`: A JSON-formatted array of commands for the Bottom row.

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_multi_track_advanced_builder
  data:
    track_1_commands: >
      [
        {"command": "MARQUEE", "stringParam": "INTRUDER ALERT", "intParam": 100},
        {"command": "WAIT", "intParam": 10000},
        {"command": "RESTORE_ROW"}
      ]
    track_3_commands: >
      [
        {"command": "FLASH", "targetRow": "BOTTOM"},
        {"command": "WAIT", "intParam": 10000},
        {"command": "RESTORE_ROW"}
      ]
```

---

### Helpers & Integrations
*These blueprints are designed to connect the Time Circuits display with other parts of Home Assistant, like entities and helpers.*

---

#### Display Home Assistant Sensor
* **File:** [`bttf_display_sensor_generator.yaml`](bttf_display_sensor_generator.yaml)
* **Description:** Provides an easy way to display a sensor's value. It uses a dropdown entity selector, so you can just pick any entity from your HA instance and see its state on the display. This script performs a **one-time write**; to keep the value updated, you must re-run the script (e.g., in response to a state-change automation).
* **When to use it:** For showing sensor data like temperature, humidity, or power usage. It's best used inside an automation that triggers when the sensor's state changes.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row to show the sensor value on.
*   `entity_to_display`: The Home Assistant entity to display the state of.
*   `prefix`: Optional text to display before the sensor value.
*   `postfix`: Optional text to display after the sensor value.
*   `restore_row`: If enabled, the row will be restored to its previous state after the specified duration. Defaults to `false`, leaving the sensor value on the display.
*   `duration`: If `restore_row` is enabled, this is how long the sensor value should remain on screen.

**Example Usage (Persistent):**
```yaml
# This service call will put "OUT: 21.5C" on the top row and leave it there.
# Best used in an automation that triggers when sensor.outside_temperature changes.
- service: script.bttf_time_circuits_display_home_assistant_sensor
  data:
    target_row: "TOP"
    entity_to_display: sensor.outside_temperature
    prefix: "OUT: "
    postfix: "C"
    restore_row: false
```

---

#### Display Text from a Helper
* **File:** [`bttf_display_text_helper_generator.yaml`](bttf_display_text_helper_generator.yaml)
* **Description:** Displays the current value of an `input_text` helper entity. This script performs a **one-time write** of the helper's current text. To keep the display updated, you must re-run the script whenever the helper changes.
* **When to use it:** For creating a "message of the day," a dynamic status panel, or any message that needs to be updated frequently without editing automations. Best used inside an automation that triggers on the state change of the `input_text` helper.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row to show the text on.
*   `text_helper`: The `input_text` helper entity to read the message from.
*   `animation_style`: How the text should be displayed (`Set Text (Instant)`, `Marquee`, or `Scramble`).
*   `speed`: For `Marquee` or `Scramble`, the speed of the animation effect in milliseconds.

**Example Usage:**
```yaml
# This service call will take the text from the input_text.time_circuits_message
# helper and scroll it on the bottom row.
# Best used in an automation that triggers when that helper's state changes.
- service: script.bttf_time_circuits_display_text_from_a_helper
  data:
    target_row: "BOTTOM"
    text_helper: input_text.time_circuits_message
    animation_style: "MARQUEE"
    speed: 120
```