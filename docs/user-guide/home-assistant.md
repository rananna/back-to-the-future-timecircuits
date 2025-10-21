# 🏠 Home Assistant Integration Guide

Welcome, time traveler! This guide provides everything you need to know to integrate your Time Circuits clock with Home Assistant. From initial setup to creating advanced automations, you'll find it all here.

### **Table of Contents**
1. [Features](#-features)
2. [Installation Guide](#-installation-guide)
3. [Importing the Blueprints](#-importing-the-blueprints)
4. [Core Concepts: Blueprints, Scripts & Automations](#-core-concepts-blueprints-scripts--automations)
5. [The Display Blueprint: A Deep Dive](#-the-display-blueprint-a-deep-dive)
6. [Core Entities & Controls Reference](#-core-entities--controls-reference)
7. [Using the Media Player](#-using-the-media-player)
8. [Sending Notifications](#-sending-notifications)
9. [Advanced Control: The Animation Sequencer](#-advanced-control-the-animation-sequencer)
10. [Troubleshooting](#troubleshooting)

---
## ✨ Features

The custom component provides a rich, native Home Assistant experience:

*   **Simple Setup**: Add the integration directly from the Home Assistant UI. No more YAML for setup!
*   **Powerful Blueprints**: A set of pre-built, well-documented blueprints to easily create custom animations and notifications without writing any code.
*   **Unified Audio Control**: A single `media_player` entity for playing sound effects, streaming radio, and using Text-to-Speech (TTS).
*   **Native Notifications**: A built-in `notify` service to easily send alerts and messages to the clock's display.
*   **Seamless OTA Updates**: An `update` entity that tells you when new firmware is available and lets you install it with one click.
*   **Intuitive Controls**: All core functions are exposed as standard HA entities like switches, selects, and number sliders.

---

## 🚀 Installation Guide

This section provides a detailed, step-by-step walkthrough for installing and configuring the Home Assistant integration.

### 🛑 Prerequisites

*   A running Home Assistant instance.
*   [HACS](https://hacs.xyz/) (Home Assistant Community Store) installed.
*   **MQTT Broker Configured in Home Assistant**: You must have the MQTT integration set up and connected to your broker. The Time Circuits clock does not connect directly to HA, but to your MQTT broker.
*   **Clock Connected to WiFi**: The Time Circuits Clock must be powered on and connected to your Wi-Fi network.
*   **Clock Configured for MQTT**: In the clock's web UI, ensure the MQTT broker details are correctly configured under the **Connectivity** tab.

### Step 1: Install the Custom Component via HACS

1.  In Home Assistant, navigate to **HACS > Integrations**.
2.  Click **Explore & Download Repositories**.
3.  Search for "Back to the Future Time Circuits" and install it.
4.  Restart Home Assistant as prompted.

### Step 2: Add the Integration in Home Assistant

1.  Navigate to **Settings > Devices & Services**.
2.  Click **Add Integration** and search for "**Back to the Future Time Circuits**".
3.  You will be prompted for your clock's **Device ID**. You can find this in the clock's web interface under **System -> System Status**.
4.  Click **Submit**.

Your Time Circuits clock will now appear as a new device in Home Assistant, with all its entities automatically created and ready to use.

---

## 📥 Importing the Blueprints

The easiest way to add the blueprint is by importing it directly from the project's GitHub repository. This ensures you always have the most up-to-date version.

1.  **Navigate to Blueprints in Home Assistant**: Go to **Settings > Automations & Scenes** and select the **Blueprints** tab.
2.  **Import a Blueprint**: Click the **Import Blueprint** button in the bottom right corner.
3.  **Paste the URL**: In the dialog box, paste the URL below into the "URL of the Blueprint to import" field.
4.  **Preview and Import**: Click **Preview Blueprint**. Home Assistant will show you the details. If it looks correct, click **Import Blueprint**.

#### **Blueprint URL (Click to Copy)**

*   **Display Blueprint:**
    ```
    https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display.yaml
    ```

---

## 🤖 Core Concepts: Blueprints, Scripts & Automations

To unlock the full potential of your Time Circuits, it's crucial to understand how these three concepts work together.

#### **1. Blueprints: The Template**
A **Blueprint** is a pre-made template. It contains all the logic for a specific action (like displaying text or running a countdown) and exposes a simple form for you to fill out. You don't have to worry about the complex code underneath.

#### **2. Scripts: The "What"**
When you fill out a blueprint's form and save it, you create a **Script**. A Script is a specific, runnable action. It defines *what* you want to happen.

*   **Example Script:** "Show the text 'INTRU DER ALERT' on all three rows, with a flashing effect and an alarm sound."

You can run a script manually from the Home Assistant UI or call it from an automation.

#### **3. Automations: The "When"**
An **Automation** is the trigger that runs your script. It defines *when* you want your action to occur, automatically.

*   **Example Automation:** "*When* my front door sensor is opened, *then* run my 'INTRUDER ALERT' script."

> **The Golden Rule:**
> 1. You **import** a Blueprint once.
> 2. You use that Blueprint to **create** one or more Scripts (the "what").
> 3. You use an Automation to **call** a Script based on a trigger (the "when").

---

## 📖 The Display Blueprint: A Deep Dive

To simplify the integration, all functionality has been consolidated into a single, powerful **Display Blueprint**. This is your central tool for creating any kind of visual alert or message on the Time Circuits clock.

It's a versatile, all-in-one blueprint for showing information on the display. You can write a custom message, show the state of any Home Assistant entity, apply various visual effects, and add sound.

#### **Core Capabilities:**

*   **Display Any Data**: Show a fixed, static message (e.g., "WELCOME HOME") or dynamically display the state of any Home Assistant entity (e.g., the current temperature from a sensor). The blueprint is robust enough to correctly handle numeric-only values (like `27.13`) without requiring a prefix or postfix. You can also use templates to combine data (e.g., "Temp: {{ states('sensor.outside_temperature') | round(1) }}°F").
*   **Target a Specific Row**: Choose whether your message appears on the TOP, MIDDLE, BOTTOM, or all three rows simultaneously.
*   **Rich Visual Effects**: Select from a wide range of animations (like `Marquee`, `Typewriter`, `Pulse`, `Flash`, `Scramble Text`, and `Random Flicker Text`) to make your message stand out. You can also fine-tune some effects with parameters like **Flicker Speed**.
*   **Sound Effects**: Add an audible alert by playing one of the device's built-in sound effects or by streaming any audio file from your Home Assistant media library. The **Volume** can be adjusted.
*   **Looping & Duration**: Repeat an effect multiple times or hold a static message on screen for a specific duration.
*   **Automatic Cleanup**: After your message is finished, the blueprint can automatically restore the display to its previous state (e.g., the clock).

#### **How to...**

*   **...create a simple alert?**
    *   **Data Source**: `Static Text`
    *   **Text to Display**: `GARAGE DOOR OPEN`
    *   **Effect**: `FLASH`
    *   **Repeat**: `5`
    *   **Sound Effect**: `sys_beep.mp3`

*   **...display a sensor value?**
    *   **Data Source**: `Home Assistant Entity`
    *   **Entity to Display**: `sensor.living_room_humidity`
    *   **Prefix**: `Humidity: `
    *   **Postfix**: `%`
    *   **Effect**: `SET_TEXT`

*   **...create a multi-row status board?**
    To display different information on each row at the same time, simply create three separate **Scripts** from the same Display blueprint.
    1.  **Script 1**: Configure it for the **Top Row** (e.g., to show the weather).
    2.  **Script 2**: Configure it for the **Middle Row** (e.g., to show the date).
    3.  **Script 3**: Configure it for the **Bottom Row** (e.g., to show a stock price).

    Then, in a single **Automation**, use a `parallel` action to run all three scripts at the same time.

    ```yaml
    # Example Automation Action
    action:
      - parallel:
          - service: script.time_circuits_show_weather
          - service: script.time_circuits_show_date
          - service: script.time_circuits_show_stock
    ```

*   **...create a countdown?**
    While there is no dedicated "countdown" function, you can achieve this with a simple automation that calls the Display blueprint in a loop.

    1.  **Trigger**: Use any trigger you like (e.g., a button press).
    2.  **Action**:
        *   Use a `repeat` loop.
        *   Inside the loop, use a `delay` of 1 second.
        *   Call the `script` created from the Display blueprint to show the current countdown number.

    Here is an example that counts down from 5 on the middle row:
    ```yaml
    # Example Automation for a 5-second countdown
    trigger:
      - platform: state
        entity_id: input_boolean.start_countdown # A helper toggle to start it
        to: "on"
    action:
      - repeat:
          count: 5
          sequence:
            # This calls a script created from the Display blueprint
            - service: script.time_circuits_display_countdown_number
              data:
                # The 'text_to_display' input of the blueprint is overridden here
                text_to_display: "{{ 5 - repeat.index }}"
                target_row: "MIDDLE"
                # Optional: Make each number flash briefly
                effect: "FLASH"
                duration: 0.5 # Show each number for half a second
            - delay: "00:00:01"
      # After the loop, show a final message
      - service: script.time_circuits_display_countdown_number
        data:
          text_to_display: "LIFTOFF!"
          target_row: "ALL"
          effect: "SCRAMBLE_TEXT"
          duration: 3
          sound_effect: "time_travel.mp3"
    ```

---
## 🎮 Core Entities & Controls Reference

The integration creates a device with a rich set of entities for granular control. You can use these in your own automations if you prefer not to use the blueprints. This section provides a detailed look at each entity.

### **Selects (Dropdowns)**

#### **`select.display_mode`**
Sets the main operating mode of the clock. The text for these modes is **static** and will remain on the display.
*   **`Normal Clock`**: The default mode. Displays the current time and date.
*   **`Stock Ticker`**: Displays data for a configured stock symbol.
*   **`Weather`**: Displays current weather information from a configured source.
*   **`Data Link`**: Displays custom, user-defined data points configured in the web UI.

**Example Automation:** Change the display to show the weather every morning at 8 AM.
```yaml
trigger:
  - platform: time
    at: "08:00:00"
action:
  - service: select.select_option
    target:
      entity_id: select.bttf_time_circuits_display_mode
    data:
      option: "Weather"
```

#### **`select.default_animation_sequence`**
Sets the animation that plays when you click the "Save and Engage Time Circuits" button in the device's web UI. This does not affect automations or scripts from Home Assistant.

#### **`select.run_animation`**
This is a special "fire-and-forget" dropdown. Selecting any animation from this list will immediately trigger it on the device. It's designed for quick manual tests or for use in automations where you want to run a specific, self-contained animation. The display will automatically revert to its previous state after the animation completes.

**Example Script:** Create a script that plays the "Intruder Alert" animation, which you can then call from an automation.
```yaml
alias: Time Circuits - Intruder Alert
sequence:
  - service: select.select_option
    target:
      entity_id: select.bttf_time_circuits_run_animation
    data:
      option: "Intruder Alert"
mode: single
```

---
### **Switches**

#### **`switch.override_switch`**
This is a master switch that allows you to take manual control of the display. When this switch is **ON**, the clock's normal display mode is ignored, and the device will instead show whatever text you have set in the `text` entities (see below). This is powerful for creating persistent, custom status displays. The text is **static** and will remain until the switch is turned **OFF**.

#### **`switch.24h_format`**
Toggles the main clock display between 12-hour and 24-hour time formats.

---
### **Numbers (Sliders)**

#### **`number.brightness`**
Adjusts the brightness of the LED displays.
*   **Range:** `0` (dimmest) to `7` (brightest).

#### **`number.stock_refresh`**
Sets the refresh interval for the stock ticker mode.
*   **Range:** `1` to `60` minutes.

---
### **Buttons**

These entities perform a single action when pressed.

*   **`button.time_travel`**: Starts the full, iconic "Time Travel" animation sequence with sound.
*   **`button.favorite_radio_station`**: Plays the favorite radio station you have configured in the device's web UI. Pressing it again while playing will stop the stream.
*   **`button.reboot_device`**: Restarts the clock.
*   **`button.force_ntp_sync`**: Manually forces the clock to synchronize its time with an internet time server.
*   **`button.factory_reset`**: Resets all device settings to their defaults.
*   **`button.refresh_weather_data`**: Manually fetches the latest data for the weather display mode.

---
### **Text Inputs**

These entities allow you to write custom, **static** text directly to the display. This text will **only be shown when the `switch.override_switch` is turned ON**.

There are two types of text entities:

1.  **Individual Segments (`text.destination_year`, `text.present_month`, etc.)**
    These 12 entities correspond to each of the individual display blocks (e.g., the 4-digit year, the 3-character month). They are useful for setting specific parts of the display.

2.  **Full Row Overrides (`text.override_line_1`, `text.override_line_2`, `text.override_line_3`)**
    These 3 entities allow you to write a string of up to 13 characters to an entire row, giving you more flexibility than the individual segments. Line 1 corresponds to the top row (Destination Time), Line 2 to the middle (Present Time), and Line 3 to the bottom (Last Time Departed). The text will be displayed exactly as you type it. For text shorter than 13 characters, it will be left-justified.

**Example Script:** Create a custom "Welcome Home" message that displays static text on all three rows.
```yaml
alias: Time Circuits - Welcome Home Message
sequence:
  # Set the text for each line
  - service: text.set_value
    target:
      entity_id: text.bttf_time_circuits_override_line_1
    data:
      value: "WELCOME HOME"
  - service: text.set_value
    target:
      entity_id: text.bttf_time_circuits_override_line_2
    data:
      value: "SYSTEMS ARE"
  - service: text.set_value
    target:
      entity_id: text.bttf_time_circuits_override_line_3
    data:
      value: "ALL GREEN"
  # Turn on the override switch to display the text
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_time_circuits_override_switch
mode: single
```

To turn the message off and return to the normal clock, you would simply call `switch.turn_off` on the `switch.override_switch`.

---
### **Other Entities**

*   **`sensor.status`**: Monitors the clock's current state (e.g., `Idle`, `Animating`) and has useful attributes like `free_heap` and `wifi_rssi`.
*   **`update.firmware`**: Notifies you when a new firmware version is available and allows for one-click OTA updates from the Home Assistant UI.

---

## 🔊 Using the Media Player

The `media_player.bttf_time_circuits` entity is your central hub for all audio.

### **Playing Sound Effects**
Play any built-in sound effect by calling the `media_player.play_media` service.

```yaml
# Example: Play an alarm sound in an automation
- service: media_player.play_media
  target:
    entity_id: media_player.bttf_time_circuits # Change if you renamed it
  data:
    media_content_id: "electric_sparks.mp3"
    media_content_type: "music"
```

### **Playing Your Favorite Radio Station**
Save a "Favorite" radio station in the clock's web UI (**Temporal Controls** tab). Then, play it easily from Home Assistant.

```yaml
# Example: Play the saved favorite radio station
- service: media_player.play_media
  target:
    entity_id: media_player.bttf_time_circuits
  data:
    media_content_id: "Favorite Radio Station"
    media_content_type: "channel"
```

### **Text-to-Speech (TTS)**
Use your favorite TTS service in Home Assistant to make the clock speak.

```yaml
# Example: Announce when the washer is done
- service: tts.google_en_com
  data:
    entity_id: media_player.bttf_time_circuits
    message: "Great Scott! The washing machine is finished."
```

---

## 🔔 Sending Notifications
This is the easiest way to display a temporary message. Use the built-in `notify` service.

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `message` | `string` | Yes | The text to display. Use `\n` to separate lines for the three rows. |
| `data.duration` | `integer`| No | How long the message should be displayed, in seconds. (Default: 10) |
| `data.sound_effect`| `string` | No | The filename of a sound effect to play (e.g., `arrival_chime.mp3`). |

**Example:** Show a "MAILBOX" notification for 60 seconds with a sound.
```yaml
- service: notify.bttf_time_circuits # This service name is based on your device name
  data:
    message: "\nMAILBOX" # The \n pushes the text to the middle row
    data:
      duration: 60
      sound_effect: "arrival_chime.mp3"
```

---

## ⚙️ Advanced Control: The Animation Sequencer

For ultimate control, you can bypass the blueprints and publish directly to the clock's raw MQTT command topic. This gives you access to the powerful **Command Sequencer**, which allows you to create custom, multi-step animations from scratch.

*   **Topic**: `bttf-time_circuits/DEVICE_ID/sequencer/command` (replace `DEVICE_ID` with your clock's ID)
*   **Payload**: A string with the name of a built-in animation, or a JSON object for a custom sequence.

> **This is an advanced feature.** For a complete guide on the sequencer, including all commands, parameters, and delicious examples, please see our new **[🎬 Animation Cookbook](./animation-cookbook.md)**.

---

## Troubleshooting

*   **Entities are 'Unavailable'?**
    *   Check the clock's Wi-Fi connection.
    *   In an MQTT client like [MQTT Explorer](http://mqtt-explorer.com/), check the `bttf_time_circuits/YOUR_DEVICE_ID/status` topic. It should have a retained message of `online`. If not, check the MQTT settings in the clock's web UI.

*   **Why does my text disappear immediately?**
    This usually happens when using a non-blocking effect like `Set Text` without specifying a duration. The script sends the command and immediately finishes, so the display restores to its previous state.
    *   **Solution**: In the blueprints, you **must** use the **`Display Duration`** input for effects that aren't self-timing. This tells the blueprint to add a `WAIT` command, holding the effect on-screen for the specified time.

*   **Can I combine sensor data with my own text?**
    Absolutely! All text fields in the blueprints support Home Assistant templates. This lets you build rich, dynamic strings.
    *Example for the "Display" blueprint's text field:*
    ```jinja
    The temperature is {{ states('sensor.outside_temperature') }}°F
    ```
