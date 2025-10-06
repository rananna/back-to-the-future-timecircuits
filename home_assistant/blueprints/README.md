# BTTF Time Circuits - Home Assistant Blueprints

This collection of Home Assistant **script blueprints** makes it easy to create and run custom animation sequences on your BTTF Time Circuits device. They provide a user-friendly form-based interface, so you can build everything from simple notifications to complex, parallel animations without writing any JSON.

The blueprints have been refactored for a simpler, more powerful, and more intuitive user experience.

## Table of Contents

- [Installation](#installation)
- [Using Templates to Display Entity Data](#using-templates-to-display-entity-data)
- [Core Concepts](#core-concepts)
- [Available Blueprints](#available-blueprints)
  - [Scene Builder](#scene-builder)
  - [Row Effects Generator](#row-effects-generator)
  - [Trigger Built-in Animation](#trigger-built-in-animation)
  - [Advanced Multi-Track Builder (JSON)](#advanced-multi-track-builder-json)
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

*   **Example:** If you use the "Row Effects Generator" to show the temperature, it will fetch the temperature *at that moment*, display it, and finish. The display will not automatically update if the temperature changes later.

**2. Automations are for Dynamic Updates**

To make the display dynamic and responsive to your home, you must use **Automations** to trigger these scripts. An automation can watch for a specific event (like a sensor changing or a door opening) and then run the corresponding script.

*   **Example:** To create a dynamic temperature display, you would create an Automation that triggers whenever `sensor.outside_temperature` changes its state. The automation's action would be to call the "Row Effects Generator" script you created. This way, every time the temperature updates, the script re-runs and sends the new value to the display.

---

## Available Blueprints

This section details each blueprint, its purpose, and its configuration options.

### Scene Builder
* **File:** [`bttf_scene_builder.yaml`](bttf_scene_builder.yaml)
* **Description:** The new, recommended way to build custom multi-track animations. This blueprint provides a user-friendly form to configure each of the three display rows independently. Simply enable the rows you want to use and configure their effects. The blueprint handles the complexity of building the final animation sequence for you.
* **When to use it:** This should be your default choice for creating any animation that involves more than one display row working in parallel.

**Example: Intruder Alert**
This scene scrolls an alert on the top row while simultaneously flashing the bottom row. This is configured entirely through the blueprint's UI form, no JSON required.

*   **Top Row Settings:**
    *   **Enable Track:** `On`
    *   **Effect Type:** `Marquee (Scrolling Text)`
    *   **Text to Display:** `INTRUDER ALERT`
    *   **Restore Row:** `On`
*   **Middle Row Settings:**
    *   **Enable Track:** `Off`
*   **Bottom Row Settings:**
    *   **Enable Track:** `On`
    *   **Effect Type:** `Flash Row`
    *   **Display Duration (s):** `10`
    *   **Restore Row:** `On`

---

### Row Effects Generator
* **File:** [`bttf_row_effects_generator.yaml`](bttf_row_effects_generator.yaml)
* **Description:** An all-in-one tool for most common effects on a **single row**. You simply select the effect you want from a dropdown menu, and the blueprint will dynamically show only the relevant options.
* **When to use it:** This should be your default choice for any single-row effect, such as a notification or displaying a sensor value.

**Example: "Washer Done" Notification**
This automation would trigger when a smart plug monitoring your washing machine drops in power. It then flashes "WASHER" on the bottom row.
```yaml
- service: script.your_script_name_here # e.g., script.washer_done_alert
  data:
    target_row: "BOTTOM"
    effect: "FLASH"
    text_to_display: "WASHER"
    duration: 10 # Flash for 10 seconds
    restore_row: true
```

---

### Trigger Built-in Animation
* **File:** [`bttf_trigger_animation_generator.yaml`](bttf_trigger_animation_generator.yaml)
* **Description:** A simple blueprint with a single dropdown to run the device's cool, pre-programmed, multi-track animations like "Lightning" or "Time Travel".
* **When to use it:** For easily triggering the most complex and cinematic effects the device has to offer.

**Example: Trigger the Lightning Animation**
```yaml
- service: script.your_script_name_here # e.g., script.trigger_lightning
  data:
    animation: "Lightning"
```

---

### Advanced Multi-Track Builder (JSON)
* **File:** [`bttf_multi_track_builder_advanced.yaml`](bttf_multi_track_builder_advanced.yaml)
* **Description:** [ADVANCED] The ultimate power-user tool. This blueprint lets you define separate command sequences for each of the three display rows by writing raw JSON arrays.
* **When to use it:** This blueprint should only be used if you need to leverage complex Jinja2 templating to dynamically generate the JSON for your tracks. For all other multi-track use cases, the **Scene Builder** is the recommended tool.

**Inputs:**
*   `track_1_commands`: A JSON-formatted array of commands for the Top row.
*   `track_2_commands`: A JSON-formatted array of commands for the Middle row.
*   `track_3_commands`: A JSON-formatted array of commands for the Bottom row.

---

## Sequencer Command Reference

The **Scene Builder** and **Advanced Multi-Track Builder** blueprints expose the full power of the device's command sequencer. For a complete list of all available commands (like `SET_TEXT`, `PULSE`, `COUNTDOWN`, etc.) and their parameters, please see the canonical documentation here:

**[Developer Docs: Sequencer API Command Reference](../../docs/developer/sequencer-api.md#command-reference)**