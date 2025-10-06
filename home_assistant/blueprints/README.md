# BTTF Time Circuits - Home Assistant Blueprint

This directory contains a single, powerful Home Assistant **script blueprint** that makes it easy to create and run any custom animation sequence on your BTTF Time Circuits device. It provides a user-friendly form-based interface, so you can build everything from simple notifications to complex, parallel animations without writing any JSON.

The blueprint has been refactored for a simpler, more powerful, and more intuitive user experience, consolidating all features into one place.

## Table of Contents

- [Installation](#installation)
- [Using Templates to Display Entity Data](#using-templates-to-display-entity-data)
- [Core Concepts](#core-concepts)
- [The Scene Builder Blueprint](#the-scene-builder-blueprint)
  - [Example 1: Single-Row Notification](#example-1-single-row-notification)
  - [Example 2: Multi-Row Scene](#example-2-multi-row-scene)
- [Sequencer Command Reference](#sequencer-command-reference)

---

## Installation

1.  **Copy Blueprint**: Copy the `bttf_scene_builder.yaml` file from this directory into the `/config/blueprints/script/` directory of your Home Assistant installation. The Samba share, FTP, or File Editor add-ons are useful for this.
2.  **Reload Blueprints**: In the Home Assistant UI, navigate to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the bottom-right corner and select **Reload Blueprints**.
3.  **Create a Script**: The "BTTF Time Circuits - Scene Builder" blueprint will appear in the list. Click **Create Script** to start configuring your animation. This configured script can then be run from your dashboards or called as a service in other automations.

---

## Using Templates to Display Entity Data

A key feature of this blueprint is the ability to use [Home Assistant templates](https://www.home-assistant.io/docs/configuration/templating/) in any text field. This allows you to create dynamic messages that include sensor values, device states, or attributes.

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

It's important to understand two key concepts about how this blueprint works:

**1. Scripts are "One-Shot"**

The blueprint creates a Home Assistant **Script**. When you run a script (either manually or from an automation), it executes its sequence of commands once and then stops. It does not continuously run in the background.

*   **Example:** If you create a script to show the temperature, it will fetch the temperature *at that moment*, display it, and finish. The display will not automatically update if the temperature changes later.

**2. Automations are for Dynamic Updates**

To make the display dynamic and responsive to your home, you must use **Automations** to trigger these scripts. An automation can watch for a specific event (like a sensor changing or a door opening) and then run the corresponding script.

*   **Example:** To create a dynamic temperature display, you would create an Automation that triggers whenever `sensor.outside_temperature` changes its state. The automation's action would be to call the Scene Builder script you created. This way, every time the temperature updates, the script re-runs and sends the new value to the display.

---

## The Scene Builder Blueprint

* **File:** [`bttf_scene_builder.yaml`](bttf_scene_builder.yaml)
* **Description:** The new, all-in-one blueprint for building custom animations. It provides a user-friendly form to configure each of the three display rows independently. Simply enable the rows you want to use and configure their effects. The blueprint handles the complexity of building the final animation sequence for you.
* **When to use it:** This should be your default choice for creating **any** custom animation, from a simple single-line notification to a complex scene with effects running in parallel across all three rows.

---

### Example 1: Single-Row Notification

This automation would trigger when a smart plug monitoring your washing machine drops in power. It then flashes "WASHER DONE" on the bottom row. This is configured entirely through the blueprint's UI form.

*   **Top Row Settings:**
    *   **Enable Track:** `Off`
*   **Middle Row Settings:**
    *   **Enable Track:** `Off`
*   **Bottom Row Settings:**
    *   **Enable Track:** `On`
    *   **Effect Type:** `Flash Row`
    *   **Text to Display:** `WASHER DONE`
    *   **Display Duration (s):** `10`
    *   **Restore Row:** `On`

### Example 2: Multi-Row Scene

This scene scrolls an alert on the top row while simultaneously flashing the bottom row—perfect for an intruder alert.

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

## Sequencer Command Reference

The Scene Builder blueprint exposes the full power of the device's command sequencer. For a complete list of all available commands (like `SET_TEXT`, `PULSE`, `COUNTDOWN`, etc.) and their parameters, please see the canonical documentation here:

**[Developer Docs: Sequencer API Command Reference](../../docs/developer/sequencer-api.md#command-reference)**