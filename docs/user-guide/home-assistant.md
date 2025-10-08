# 🏠 Home Assistant Integration Guide

This project features a native Home Assistant integration that provides a seamless and powerful way to control your Time Circuits clock. This guide covers everything from initial setup to advanced automations.

### **Table of Contents**
1. [Features](#-features)
2. [Prerequisites](#-prerequisites)
3. [Setup & Installation](#-setup--installation)
4. [Finding Your Device ID](#-finding-your-device-id)
5. [**Using Blueprints (Recommended)**](#-the-easy-way-using-blueprints)
    - [Core Concepts: Scripts vs. Automations](#core-concepts-scripts-vs-automations)
    - [Blueprint Guide: Display Text](#blueprint-guide-display-text)
    - [Blueprint Guide: Display Entity](#blueprint-guide-display-entity)
    - [Blueprint Guide: Countdown Timer](#blueprint-guide-countdown-timer)
6. [Core Entities & Controls](#-core-entities--controls)
7. [Using the Media Player](#-using-the-media-player)
8. [Sending Notifications](#-sending-notifications)
9. [**Advanced Control: Custom Sequences & MQTT**](#-advanced-control-custom-sequences--mqtt)
    - [Triggering a Built-in Animation](#triggering-a-built-in-animation)
    - [Crafting a Custom JSON Sequence](#crafting-a-custom-json-sequence)
10. [Troubleshooting & FAQ](#-troubleshooting--faq)

---
## ✨ Features

The custom component provides a rich, native Home Assistant experience:

*   **Simple Setup**: Add the integration directly from the Home Assistant UI.
*   **Powerful Blueprints**: A set of pre-built blueprints to easily create custom animations and notifications without writing any code.
*   **Unified Audio Control**: A single `media_player` entity for playing sound effects, streaming radio, and using Text-to-Speech (TTS).
*   **Native Notifications**: A built-in `notify` service to easily send alerts and messages to the clock's display.
*   **Seamless OTA Updates**: An `update` entity that tells you when new firmware is available and lets you install it with one click.
*   **Intuitive Controls**: All core functions are exposed as standard HA entities like switches, text inputs, and number sliders.

---

## 🛑 Prerequisites

*   A running Home Assistant instance.
*   [HACS](https://hacs.xyz/) (Home Assistant Community Store) installed.
*   **MQTT Broker Configured in Home Assistant**: You must have the MQTT integration set up and connected to your broker.
*   **Clock Connected to WiFi & MQTT**: The Time Circuits Clock must be powered on, connected to your Wi-Fi, and configured with your MQTT broker details in its web UI.

---

## 🚀 Setup & Installation

### **Step 1: Install the Custom Component via HACS**
1.  In Home Assistant, navigate to **HACS > Integrations**.
2.  Click the three dots in the top-right corner and select **Custom repositories**.
3.  In the "Repository" field, paste the URL to this GitHub repository: `https://github.com/rananna/back-to-the-future-timecircuits`
4.  Select **Integration** for the category and click **ADD**.
5.  Close the "Custom repositories" window. The "Back to the Future Time Circuits" integration will now appear. Click on it and then click **INSTALL**.
6.  Restart Home Assistant as prompted.

### **Step 2: Add the Integration in Home Assistant**
1.  Navigate to **Settings > Devices & Services**.
2.  Click **Add Integration** and search for "**Back to the Future Time Circuits**".
3.  Follow the on-screen instructions. The integration will automatically discover your clock on the network via its MQTT messages.

Your Time Circuits clock will now appear as a new device in Home Assistant, with all its entities automatically created.

### **Step 3: Install the Blueprints**
1.  **Copy Blueprints**: Copy the `.yaml` files from the [`home_assistant/blueprints`](../../home_assistant/blueprints/) directory of this repository into your Home Assistant's `/config/blueprints/script/` directory. The easiest way to do this is with the Samba Share or File Editor add-ons.
2.  **Reload Blueprints**: In the Home Assistant UI, go to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the corner and select **Reload Blueprints**. The Time Circuits blueprints will now appear in your list, ready to be used.

---

## 🆔 Finding Your Device ID

Your clock's unique **Device ID** is essential for using blueprints and sending advanced MQTT commands. You can find it in the clock's Web UI.

1.  Connect to the clock's Wi-Fi network or access it via its local network IP address.
2.  Navigate to the **Settings** page.
3.  The Device ID is displayed prominently at the top of the page. It's the same as the "MQTT Base Topic".

![Finding the Device ID](https://raw.githubusercontent.com/rananna/back-to-the-future-timecircuits/main/images/documentation/web-ui-device-id.png)

---

## 🤖 The Easy Way: Using Blueprints

For most automations, the easiest and most powerful way to create custom animations and notifications is with our **Home Assistant Blueprints**. They provide a simple, form-based UI inside Home Assistant, allowing you to build complex effects without writing any code.

### Core Concepts: Scripts vs. Automations

To unlock the full potential of your Time Circuits, it's crucial to understand how Home Assistant uses scripts and automations together.

**1. Scripts are "One-Shot" Actions**

Think of a blueprint script as a single, specific mission: "Show this text now." When you run the script, it executes its commands (e.g., display a temperature reading) and then it's done. It doesn't keep running or update automatically.

**2. Automations Provide the Brains**

Automations are the engine of your smart home. They watch for triggers (like a sensor changing, a time of day, or a button press) and then perform actions, such as running one of your blueprint scripts.

> **The Pattern:** You create a **Script** from a blueprint to define *what* you want to happen. Then, you create an **Automation** to decide *when* it should happen.

### Blueprint Guide: Display Text

This is your workhorse for displaying any custom message. It's perfect for alerts, notifications, and simple status messages.

*   **Inputs:**
    *   `Device ID`: The unique identifier for your Time Circuits clock.
    *   `Display Row`: Which of the three rows (Top, Middle, Bottom) to use.
    *   `Text`: The message to display. You can use Home Assistant templates here!
    *   `Effect`: An optional visual effect (e.g., Pulse, Flash, Marquee).
    *   `Duration (seconds)`: How long the text should remain visible.
    *   `Sound Effect`: An optional sound to play from the device's library.
    *   `Restore Row`: If checked, the row will return to its previous state (e.g., the clock) after the duration.

*   **Automation Example: Front Door Alert**
    This automation displays "FRONT DOOR OPEN" on the middle row when the front door is opened. It stays visible for 10 seconds, then the row reverts to the clock.

    ```yaml
    alias: Alert - Front Door Opened
    trigger:
      - platform: state
        entity_id: binary_sensor.front_door_contact
        to: 'on'
    action:
      - service: script.your_display_text_script_name # <-- Change this!
        data:
          row: Middle
          text: FRONT DOOR OPEN
          duration: 10
          effect: Flash
    ```

### Blueprint Guide: Display Entity

This blueprint is the easiest way to show the state or value of any Home Assistant entity on your display.

*   **Inputs:**
    *   `Device ID`, `Display Row`, `Effect`, `Duration`, `Sound`, `Restore Row`: Same as the Display Text blueprint.
    *   `Entity`: The sensor or entity you want to display.
    *   `Prefix / Suffix`: Optional text to add before or after the entity's value (e.g., a "°F" suffix for temperature).

*   **Automation Example: Dynamic Temperature Display**
    This automation runs the script whenever the outside temperature sensor changes, keeping the display on the top row always up-to-date.

    ```yaml
    alias: Update Outside Temperature Display
    trigger:
      - platform: state
        entity_id: sensor.outside_temperature
    action:
      - service: script.your_display_entity_script_name # <-- Change this!
        data:
          row: Top
          entity: sensor.outside_temperature
          suffix: " F"
          restore_row: false # Keep the value on screen
    ```

### Blueprint Guide: Countdown Timer

Perfect for building anticipation for movie night, a gaming session, or just counting down to dinner time.

*   **Inputs:**
    *   `Device ID`, `Display Row`, `Sound`: Same as the other blueprints.
    *   `Duration (seconds)`: The total length of the countdown.
    *   `Prefix`: Optional text to show before the countdown number (e.g., "T-MINUS").
    *   `Completion Text`: A message to display when the countdown hits zero.

*   **Automation Example: Movie Night Countdown**
    When you turn on the "Movie Night" switch, this automation starts a 10-second countdown on the bottom row, ending with the message "SHOWTIME!".

    ```yaml
    alias: Movie Night Countdown
    trigger:
      - platform: state
        entity_id: input_boolean.movie_night
        to: 'on'
    action:
      - service: script.your_countdown_script_name # <-- Change this!
        data:
          row: Bottom
          duration_seconds: 10
          prefix: "STARTING IN"
          completion_text: "SHOWTIME!"
          sound: Time Travel
    ```

---

## 🎮 Core Entities & Controls

The integration creates a device with a rich set of entities to control every aspect of the clock. You can use these entities in your own automations if you prefer not to use the blueprints.

| Entity Type | Name | Description |
| :--- | :--- | :--- |
| **Select** | `Display Mode` | Sets the main operating mode of the clock. Choose between `Normal Clock`, `Stock Ticker`, `Weather`, or `Data Link`. |
| **Text** | `Data Point 1-5 Marquee` | Sets the scrolling text for one of the five "Data Link" pages. |
| **Switch** | `Data Point 1-5 Enabled`| Enables or disables one of the five "Data Link" pages. |
| **Text** | `(all 12 segments)` | Provides direct, granular control over every individual display segment (e.g., `Destination Month`, `Present Day`). |
| **Number** | `Brightness` | Adjusts the brightness of the displays (0-7). |
| **Number** | `Volume` | Adjusts the volume of the speaker (0-21). |
| **Button** | `Engage Time Circuits` | Manually starts the full time travel animation sequence. |
| **Button** | `Reboot Device` | Restarts the clock. |
| **Sensor** | `Status` | Monitors the clock's current state (e.g., `Idle`, `Animating`) and has useful attributes like `free_heap` and `wifi_rssi`. |
| **Update** | `Firmware` | Notifies you when a new firmware version is available and allows for one-click OTA updates. |
| **Switch** | `Time Circuits On/Off`| A master switch to turn the displays on or off. |

---

## 🔊 Using the Media Player

The `media_player.bttf_time_circuits` entity is your central hub for all audio.

### **Playing Sound Effects**
You can play any of the built-in sound effects by calling the `media_player.play_media` service with a `media_content_type` of `music`.

```yaml
# Example: Play an alarm sound in an automation
- service: media_player.play_media
  target:
    entity_id: media_player.bttf_time_circuits
  data:
    media_content_id: "electric_sparks.mp3"
    media_content_type: "music"
```

### **Playing Radio Streams**
Use the same `media_player.play_media` service with a `media_content_type` of `url`.

```yaml
# Example: Play an 80s radio station
- service: media_player.play_media
  target:
    entity_id: media_player.bttf_time_circuits
  data:
    media_content_id: "http://d.liveatc.net/kcrw_eclectic" # Example Stream URL
    media_content_type: "url"
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

#### **`notify.bttf_time_circuits`**
| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`message`** | `string` | Yes | The text to display. Use `\n` to separate lines (e.g., `LINE 1\nLINE 2`). |
| **`data.duration`** | `integer`| No | How long the message should be displayed, in seconds. (Default: 10) |
| **`data.sound_effect`**| `string` | No | The filename of a sound effect to play from the device's library (e.g., `REMINDER.mp3`). |

**Example:** Show a "MAILBOX" notification for 60 seconds with a sound.
```yaml
- service: notify.bttf_time_circuits
  data:
    message: "\nMAILBOX"
    data:
      duration: 60
      sound_effect: "REMINDER.mp3"
```

---

## ⚙️ Advanced Control: Custom Sequences & MQTT

For the most advanced automations, you can bypass the blueprints and standard entities to publish directly to the clock's raw MQTT topics. This gives you access to the powerful **Command Sequencer**, which allows you to create custom animations and trigger pre-programmed cinematic effects.

*   **Topic**: `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload**: A string with the name of a built-in animation, or a JSON object for a custom sequence.

> **For a complete guide on the sequencer, including all commands, parameters, and a full list of built-in animation names, please see the [🔬 Developer & Sequencer API Guide](../developer/developer-guide.md).**

### Triggering a Built-in Animation

This is perfect for common alerts and effects. You simply send the animation's name as the payload.

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

### Crafting a Custom JSON Sequence
You can build your own sequences directly in your automation's YAML using Home Assistant templates.

```yaml
alias: Display Freezing Temperature Alert
trigger:
  - platform: numeric_state
    entity_id: sensor.outside_temperature
    below: 0
action:
  - service: mqtt.publish
    data: # Use data:, not the deprecated data_template:
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command"
      payload: >
        {
          "tracks": [
            {
              "targetRow": "TOP",
              "commands": [
                {
                  "command": "MARQUEE",
                  "stringParam": "FREEZING TEMP: {{ states('sensor.outside_temperature') }}°C"
                },
                {
                  "command": "PULSE",
                  "targetSegment": -1,
                  "intParam": 5000
                }
              ]
            }
          ]
        }
```
---

## ❔ Troubleshooting & FAQ

> ⚠️ **Device Not Appearing in Home Assistant?**
> * Double-check the MQTT broker IP, port, and credentials in the clock's web UI.
> * Verify that "Enable discovery" is turned on for your MQTT integration in Home Assistant.
> * Use a tool like [MQTT Explorer](http://mqtt-explorer.com/) to see if the clock is publishing topics under `homeassistant/`.

> ⚠️ **Entities are 'Unavailable'?**
> * Check the clock's Wi-Fi connection.
> * In MQTT Explorer, check the `bttf-time-circuits/<UNIQUE_ID>/status` topic. It should have a retained message of `online`.

> 🤔 **Why does my text disappear immediately?**
> This usually happens when using the `SET_TEXT` effect (or no effect at all). This command is non-blocking, meaning it executes and immediately moves on. The blueprint's `Duration (seconds)` input solves this by automatically adding a `WAIT` command. If your text isn't staying visible, ensure you have set a `Duration` greater than zero. Effects like `PULSE`, `MARQUEE`, and `COUNTDOWN` are blocking; they run for their own specific length and do not require an additional `WAIT`.

> 🤔 **Can I combine sensor data with my own text?**
> Absolutely! All text fields in the blueprints support Home Assistant templates. This lets you build rich, dynamic strings.
>
> *Example for the "Display Text" blueprint:*
> ```jinja
> Garage is {{ states('cover.garage_door') }}. Temp: {{ states('sensor.garage_temp') }}°F
> ```