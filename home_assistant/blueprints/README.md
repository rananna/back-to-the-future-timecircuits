# BTTF Time Circuits - Home Assistant Blueprints

Welcome! This directory contains a collection of Home Assistant **script blueprints** that make it easy to create and run custom animations on your BTTF Time Circuits device.

To provide the best experience, we offer a set of focused blueprints for common tasks. This approach keeps things simple for everyday notifications and automations.

## Which Blueprint Should I Use?

Here's a quick guide to help you choose the right blueprint for your needs:

| Your Goal                                       | Recommended Blueprint                                   |
| ------------------------------------------------- | ------------------------------------------------------- |
| "I want to display a simple text message."        | [**Display Text**](./display_text.yaml)                 |
| "I want to show a sensor's value on the display." | [**Display Entity**](./display_entity.yaml)             |
| "I want to run a countdown timer."                | [**Countdown Timer**](./countdown.yaml)                 |

---

## Installation

1.  **Copy Blueprints**: Copy the `.yaml` files from this directory into the `/config/blueprints/script/` directory of your Home Assistant installation. You can choose to copy all of them, or just the ones you need. The Samba share, FTP, or File Editor add-ons are useful for this.
2.  **Reload Blueprints**: In the Home Assistant UI, navigate to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the bottom-right corner and select **Reload Blueprints**.
3.  **Create a Script**: The blueprints you copied will appear in the list. Click **Create Script** on the one you wish to use. This configured script can then be run from your dashboards or called as a service in other automations.

---

## Core Concepts

It's important to understand two key concepts about how these blueprints work:

**1. Scripts are "One-Shot"**

The blueprints create a Home Assistant **Script**. When you run a script (either manually or from an automation), it executes its sequence of commands once and then stops. It does not continuously run in the background.

*   **Example:** If you create a script to show the temperature, it will fetch the temperature *at that moment*, display it, and finish. The display will not automatically update if the temperature changes later.

**2. Automations are for Dynamic Updates**

To make the display dynamic and responsive to your home, you must use **Automations** to trigger these scripts. An automation can watch for a specific event (like a sensor changing or a door opening) and then run the corresponding script.

*   **Example:** To create a dynamic temperature display, you would use the **Display Entity** blueprint to create a script. Then, you would create an **Automation** that triggers whenever `sensor.outside_temperature` changes its state. The automation's action would be to call the script you created. This way, every time the temperature updates, the script re-runs and sends the new value to the display.

---

## Using Templates to Display Entity Data

A key feature of these blueprints is the ability to use [Home Assistant templates](https://www.home-assistant.io/docs/configuration/templating/) in any text field. This allows you to create dynamic messages that include sensor values, device states, or attributes.

While the **Display Entity** blueprint is the easiest way to show sensor data, templates are useful in other blueprints as well.

**Example: Dynamic alert message in the "Display Text" blueprint**
```jinja
Alert: The {{ trigger.to_state.name }} was opened!
```

---

## Advanced Usage: Triggering Built-in Animations

Beyond the blueprints, you can also trigger a wide variety of pre-programmed, named animations for more cinematic effects (e.g., `Intruder Alert`, `Time Travel`, `Lightning`). This is useful when you want a cool visual effect without building a complex sequence yourself.

You can do this by calling the `mqtt.publish` service directly in your automations.

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
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command"
      payload: "Intruder Alert"
```

For a complete, up-to-date list of all the available animation names you can use in the `payload`, please see the canonical list in the main API documentation:

**[Developer Docs: Built-in Animations List](../../docs/developer/sequencer-api.md#built-in-animations)**

---

## Full Command Reference

The blueprints expose the full power of the device's command sequencer. For a complete list of all available commands (like `SET_TEXT`, `PULSE`, `COUNTDOWN`, etc.) and their parameters, please see the canonical documentation here:

**[Developer Docs: Sequencer API Command Reference](../../docs/developer/sequencer-api.md#command-reference)**