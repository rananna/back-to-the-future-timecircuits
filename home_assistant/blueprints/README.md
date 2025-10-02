# BTTF Time Circuits - Home Assistant Blueprints

This collection of Home Assistant **script blueprints** makes it easy to create and run custom animation sequences on your BTTF Time Circuits device. They provide a user-friendly form-based interface, so you can build complex animations without writing any JSON.

## Table of Contents

- [Installation](#installation)
- [Using Templates to Display Entity Data](#using-templates-to-display-entity-data)
- [Available Blueprints](#available-blueprints)
  - [Simple & Common Effects](#simple--common-effects)
    - [Simple Text Sequence](#simple-text-sequence)
    - [Marquee Text Sequence](#marquee-text-sequence)
    - [Scramble Text Sequence](#scramble-text-sequence)
    - [Countdown Sequence](#countdown-sequence)
  - [Entity & Helper Focused](#entity--helper-focused)
    - [Display Home Assistant Sensor](#display-home-assistant-sensor)
    - [Display Text from a Helper](#display-text-from-a-helper)
  - [Advanced & Themed Effects](#advanced--themed-effects)
    - [Trigger Built-in Animation](#trigger-built-in-animation)
    - [Visual Effects Generator](#visual-effects-generator)
    - [Row Effects Generator](#row-effects-generator)
    - [Multi-Track Advanced Builder](#multi-track-advanced-builder)

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

## Available Blueprints

This section details each blueprint, its purpose, and its configuration options.

### Simple & Common Effects

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
* **Description:** Creates a scrolling text animation (a marquee) on a selected row.
* **When to use it:** Perfect for longer messages that don't fit on the 13-character display, such as song titles, news headlines, or detailed status updates.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row to scroll the text on.
*   `text_to_display`: The text to scroll. Supports templates and can be longer than 13 characters.
*   `scroll_speed`: The delay between each step of the scroll animation (in milliseconds).

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_marquee_text_sequence
  data:
    target_row: "TOP"
    text_to_display: "Now playing: {{ state_attr('media_player.living_room', 'media_title') }}"
    scroll_speed: 150
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

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_scramble_text_sequence
  data:
    target_row: "BOTTOM"
    text_to_display: "ACCESS GRANTED"
    lock_in_speed: 100
    scramble_speed: 50
```

---

#### Countdown Sequence
* **File:** [`bttf_countdown_generator.yaml`](bttf_countdown_generator.yaml)
* **Description:** Displays a numerical countdown on a selected row.
* **When to use it:** Great for automations with a time component, such as "DISARMING IN 10..." or "SYSTEM REBOOT IN 5...".

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row for the countdown.
*   `start_number`: The number to start counting down from.
*   `end_text`: Text to display when the countdown finishes (e.g., "LIFTOFF").
*   `countdown_delay`: The delay between each number change (in milliseconds).

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_countdown_sequence
  data:
    target_row: "MIDDLE"
    start_number: 10
    end_text: "HAPPY NEW YEAR"
    countdown_delay: 1000
```

---

### Entity & Helper Focused

---

#### Display Home Assistant Sensor
* **File:** [`bttf_display_sensor_generator.yaml`](bttf_display_sensor_generator.yaml)
* **Description:** The easiest way to display a sensor's value. It uses a dropdown entity selector, so you can just pick any entity from your HA instance and see its state on the display. You can also add text before and after the value.
* **When to use it:** For quickly showing sensor data like temperature, humidity, or power usage without writing any templates.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row to show the sensor value on.
*   `entity_to_display`: The Home Assistant entity to display the state of.
*   `prefix`: Optional text to display before the sensor value.
*   `postfix`: Optional text to display after the sensor value.
*   `restore_row_after_a_delay`: If enabled, the row will be restored to its previous state after the specified duration.
*   `duration`: How long the text should remain on screen (in seconds).

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_display_home_assistant_sensor
  data:
    target_row: "TOP"
    entity_to_display: sensor.outside_temperature
    prefix: "OUT:"
    postfix: "C"
    restore_row_after_a_delay: true
    duration: 15
```

---

#### Display Text from a Helper
* **File:** [`bttf_display_text_helper_generator.yaml`](bttf_display_text_helper_generator.yaml)
* **Description:** Displays the current value of an `input_text` helper entity. This is a powerful feature that allows you to change the message an automation displays directly from your dashboard.
* **When to use it:** For creating a "message of the day," a dynamic status panel, or any message that needs to be updated frequently without editing automations.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row to show the text on.
*   `text_helper`: The `input_text` helper entity to read the message from.
*   `animation_style`: How the text should be displayed (Instant, Marquee, or Scramble).
*   `speed`: The speed of the animation effect in milliseconds (for Marquee or Scramble).

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_display_text_from_a_helper
  data:
    target_row: "BOTTOM"
    text_helper: input_text.time_circuits_message
    animation_style: "MARQUEE"
    speed: 120
```

---

### Advanced & Themed Effects

---

#### Trigger Built-in Animation
* **File:** [`bttf_trigger_animation_generator.yaml`](bttf_trigger_animation_generator.yaml)
* **Description:** A simple blueprint with a single dropdown to run the device's cool, pre-programmed, multi-track animations.
* **When to use it:** For easily triggering the most complex and cinematic effects the device has to offer, like `TimeTravel` or `IntruderAlert`.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `animation`: The built-in animation to trigger.

**Example Usage:**
```yaml
- service: script.bttf_time_circuits_trigger_built_in_animation
  data:
    animation: "IntruderAlert"
```

---

#### Visual Effects Generator
* **File:** [`bttf_visual_effects_generator.yaml`](bttf_visual_effects_generator.yaml)
* **Description:** A themed blueprint that groups several creative text-based visual effects into a single place.
* **When to use it:** For adding more creative flair to your text displays beyond the standard animations.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row for the effect.
*   `effect`: The visual effect to use (`Typewriter`, `Crossfade`, `Scanner`, `Bar Graph`).
*   `text_to_display`: The text for the effect.
*   `...other options`: Additional options vary by effect (e.g., speed, duration).

**Example Usage (Typewriter):**
```yaml
- service: script.bttf_time_circuits_visual_effects_generator
  data:
    target_row: "MIDDLE"
    effect: "TYPEWRITER"
    text_to_display: "SYSTEM ONLINE"
    typewriter_delay: 150
```

---

#### Row Effects Generator
* **File:** [`bttf_row_effects_generator.yaml`](bttf_row_effects_generator.yaml)
* **Description:** Starts or stops continuous, attention-grabbing effects like `PULSE` or `FLASH` on an entire row.
* **When to use it:** For creating persistent alerts, like making a row flash red while an alarm is active. **Note:** These effects run until they are explicitly stopped by running the blueprint again with the `STOP` command.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `target_row`: The display row for the effect.
*   `effect`: The effect to apply (`PULSE`, `FLASH`).
*   `action`: Whether to `START` or `STOP` the effect.

**Example Usage (Start):**
```yaml
- service: script.bttf_time_circuits_row_effects_generator
  data:
    target_row: "BOTTOM"
    effect: "FLASH"
    action: "START"
```

---

#### Multi-Track Advanced Builder
* **File:** [`bttf_multi_track_advanced_builder.yaml`](bttf_multi_track_advanced_builder.yaml)
* **Description:** The ultimate power-user tool. This blueprint lets you define separate command sequences for each of the three display rows, allowing you to run complex animations in parallel.
* **When to use it:** For creating highly choreographed sequences where multiple things are happening at once, such as showing scrolling text on one row, a countdown on another, and a flashing light on the third—all at the same time. This blueprint is for advanced users comfortable with the device's sequencer commands.

**Inputs:**
*   `mqtt_topic`: The base MQTT topic for your device.
*   `track_1_commands`: A JSON-formatted string of commands for the Top row.
*   `track_2_commands`: A JSON-formatted string of commands for the Middle row.
*   `track_3_commands`: A JSON-formatted string of commands for the Bottom row.

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
        {"command": "FLASH"},
        {"command": "WAIT", "intParam": 10000},
        {"command": "RESTORE_ROW"}
      ]
```