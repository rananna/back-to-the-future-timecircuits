# Home Assistant Blueprints for BTTF Time Circuits

This directory contains a collection of Home Assistant **script blueprints** designed to make it easy to create and run custom animation sequences on your BTTF Time Circuits device.

## What are Blueprints?

Home Assistant Blueprints are templates for scripts or automations. You can import them into your Home Assistant instance, and they will provide you with a user-friendly form to configure a new script. This saves you from having to write complex JSON code by hand.

## Installation

1.  **Copy to your HA instance:** Copy the `.yaml` file for the blueprint you want to use into the `/config/blueprints/script/` directory of your Home Assistant installation. You can use the Samba share add-on, the File editor add-on, or `scp` to do this.
2.  **Reload Blueprints:** In the Home Assistant UI, navigate to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the bottom-right corner and select **Reload Blueprints**.
3.  **Create a Script:** Your new blueprint will now appear in the list. Click **Create Script** next to its name to start configuring it. The script can then be run from your dashboards or called as a service in your automations.

---

## Using Templates to Display Entity Data

A key feature of these blueprints is the ability to use [Home Assistant templates](https://www.home-assistant.io/docs/configuration/templating/) in any text field. This allows you to create dynamic messages that include sensor values or other entity states.

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

### Simple & Common Effects

#### Simple Text Sequence (`bttf_simple_sequence_generator.yaml`)
The most basic blueprint. It displays a static or templated line of text on a selected row for a specific duration, then restores the row.
*   **Use this for:** Simple notifications, like "GARAGE DOOR OPEN" or "WELCOME HOME".

#### Marquee Text Sequence (`bttf_marquee_sequence_generator.yaml`)
Creates a scrolling text animation (a marquee) on a selected row. You can control the text and the scroll speed.
*   **Use this for:** Longer messages that don't fit on the screen, like displaying a song title or a longer status update.

#### Scramble Text Sequence (`bttf_scramble_text_generator.yaml`)
Creates the "scramble" effect where random characters flicker on the display before locking in one-by-one to reveal the final message.
*   **Use this for:** A more dramatic or "high-tech" way to reveal a message.

#### Countdown Sequence (`bttf_countdown_generator.yaml`)
Displays a numerical countdown on a selected row. You can set the starting number and the speed of the countdown.
*   **Use this for:** Automations that have a time component, such as "DISARMING IN 10..." or "SYSTEM REBOOT IN 5...".

---

### Entity & Helper Focused

#### Display Home Assistant Sensor (`bttf_display_sensor_generator.yaml`)
The easiest way to display a sensor's value. It uses a dropdown entity selector, so you can just pick any entity from your HA instance and see its state on the display.
*   **Use this for:** Quickly showing sensor data like temperature, humidity, or power usage without writing any templates.

#### Display Text from a Helper (`bttf_dynamic_text_generator.yaml`)
Displays the text from an `input_text` helper entity. This is a powerful feature that allows you to change the message an automation displays directly from your dashboard.
*   **Use this for:** Creating a "message of the day" or a dynamic status panel that can be updated without editing any automations.

---

### Advanced & Themed Effects

#### Trigger Built-in Animation (`bttf_trigger_animation_generator.yaml`)
A simple blueprint with a single dropdown to run the device's cool, pre-programmed, multi-track animations like `TimeTravel` or `IntruderAlert`.
*   **Use this for:** Easily triggering the most complex and cinematic effects the device has to offer.

#### Visual Effects Generator (`bttf_visual_effects_generator.yaml`)
A themed blueprint that groups several text-based visual effects (`Typewriter`, `Crossfade`, `Scanner`, `Bar Graph`) into a single place.
*   **Use this for:** Adding more creative flair to your text displays beyond the standard animations.

#### Row Effects Generator (`bttf_row_effects_generator.yaml`)
Starts or stops continuous, attention-grabbing effects like `PULSE` or `FLASH` on an entire row. Note that these effects run until they are explicitly stopped.
*   **Use this for:** Creating persistent alerts, like making a row flash red while an alarm is active.

#### Multi-Track Advanced Builder (`bttf_multi_track_advanced_builder.yaml`)
The ultimate power-user tool. This blueprint lets you define separate command sequences for each of the three display rows, allowing you to run complex animations in parallel.
*   **Use this for:** Creating highly choreographed sequences where multiple things are happening at once, such as showing scrolling text, a countdown, and a flashing light all at the same time.