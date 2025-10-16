# 🏠 Home Assistant Integration Guide

Welcome, time traveler! This guide provides everything you need to know to integrate your Time Circuits clock with Home Assistant. From initial setup to creating advanced automations, you'll find it all here.

### **Table of Contents**
1. [Features](#-features)
2. [Setup & Installation](#-setup--installation)
3. [Understanding Blueprints, Scripts, and Automations](#-understanding-blueprints-scripts-and-automations)
4. [Importing the Blueprints](#-importing-the-blueprints)
5. [Blueprint Showcase](#-blueprint-showcase)
    * [Display Text](#1-display-text)
    * [Display Entity](#2-display-entity)
    * [Countdown Timer](#3-countdown-timer)
6. [Core Entities & Controls](#-core-entities--controls)
7. [Using the Media Player](#-using-the-media-player)
8. [Sending Notifications](#-sending-notifications)
9. [Advanced Control (MQTT)](#-advanced-control-mqtt)
10. [Troubleshooting](#troubleshooting)

---
## ✨ Features

The custom component provides a rich, native Home Assistant experience:

*   **Simple Setup**: Add the integration directly from the Home Assistant UI. No more YAML for setup!
*   **Powerful Blueprints**: A set of pre-built blueprints to easily create custom animations and notifications without writing any code.
*   **Unified Audio Control**: A single `media_player` entity for playing sound effects, streaming radio, and using Text-to-Speech (TTS).
*   **Native Notifications**: A built-in `notify` service to easily send alerts and messages to the clock's display.
*   **Seamless OTA Updates**: An `update` entity that tells you when new firmware is available and lets you install it with one click.
*   **Intuitive Controls**: All core functions are exposed as standard HA entities like switches, text inputs, and number sliders.

---

## 🛑 Prerequisites

*   A running Home Assistant instance.
*   [HACS](https://hacs.xyz/) (Home Assistant Community Store) installed.
*   **MQTT Broker Configured in Home Assistant**: You must have the MQTT integration set up and connected to your broker. The Time Circuits clock does not connect directly to HA, but to your MQTT broker.
*   **Clock Connected to WiFi**: The Time Circuits Clock must be powered on and connected to your Wi-Fi network.
*   **Clock Configured for MQTT**: In the clock's web UI, ensure the MQTT broker details are correctly configured under the **Settings -> Data Link** tab.

---

## 🚀 Setup & Installation

### **Step 1: Install the Custom Component via HACS**
1.  In Home Assistant, navigate to **HACS > Integrations**.
2.  Click **Explore & Download Repositories**.
3.  Search for "Back to the Future Time Circuits" and install it.
4.  Restart Home Assistant as prompted.

### **Step 2: Add the Integration in Home Assistant**
1.  Navigate to **Settings > Devices & Services**.
2.  Click **Add Integration** and search for "**Back to the Future Time Circuits**".
3.  You will be prompted for your clock's **Device ID**. You can find this in the clock's web interface under **Settings -> Device**.
4.  Click **Submit**.

Your Time Circuits clock will now appear as a new device in Home Assistant, with all its entities automatically created and ready to use.

---

## 🤖 Understanding Blueprints, Scripts, and Automations

To unlock the full potential of your Time Circuits, it's crucial to understand how these three concepts work together.

#### **1. Blueprints: The Template**
A **Blueprint** is a pre-made template. It contains all the logic for a specific action (like displaying text or running a countdown) and exposes a simple form for you to fill out. You don't have to worry about the complex code underneath.

#### **2. Scripts: The "What"**
When you fill out a blueprint's form and save it, you create a **Script**. A Script is a specific, runnable action. It defines *what* you want to happen.

*   **Example Script:** "Show the text 'INTRUDER ALERT' on all three rows, with a flashing effect and an alarm sound."

You can run a script manually from the Home Assistant UI or call it from an automation.

#### **3. Automations: The "When"**
An **Automation** is the trigger that runs your script. It defines *when* you want your action to occur, automatically.

*   **Example Automation:** "*When* my front door sensor is opened, *then* run my 'INTRUDER ALERT' script."

> **The Golden Rule:**
> 1. You **import** a Blueprint once.
> 2. You use that Blueprint to **create** one or more Scripts (the "what").
> 3. You use an Automation to **call** a Script based on a trigger (the "when").

---

## 📥 Importing the Blueprints

The easiest way to add the blueprints is by importing them directly from the project's GitHub repository. This ensures you always have the most up-to-date version.

1.  **Navigate to Blueprints in Home Assistant**: Go to **Settings > Automations & Scenes** and select the **Blueprints** tab.
2.  **Import a Blueprint**: Click the **Import Blueprint** button, paste one of the URLs below, and click **Preview Blueprint**, then **Import Blueprint**.

3.  **Blueprint URLs (Copy and Paste)**:
    *   **Display**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display.yaml`
    *   **Countdown Timer**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/countdown.yaml`
    *   **Multi-Row Status Board**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/multi_row_status_board.yaml`

Repeat the import process for each blueprint you wish to use.

## 📖 Blueprint Reference

This section is your comprehensive guide to the blueprints. You'll find detailed explanations of all available inputs, a complete list of visual and sound effects, and practical examples to help you build powerful automations.

### **The "Display" Blueprint**
This is the most versatile blueprint. It replaces the old "Display Text" and "Display Entity" blueprints with a single, more powerful solution.

#### **Key Inputs**
*   `Target Row`: Choose which row (or all rows) to display the message on.
*   `Data Source`: The most important input!
    *   `Static Text`: Lets you type a message directly. Supports templates.
    *   `Home Assistant Entity`: Lets you display the state or an attribute of any entity.
*   `Visual Effect`: Choose from a list of animations like `Set Text`, `Marquee`, `Pulse`, `Flash`, `Typewriter`, `Scramble Text`, or `Random Flicker`.
*   `Display Duration`: **Crucial for `Set Text`**, `Pulse`, and `Flash` effects. It sets how long the effect stays on screen. For other effects like `Marquee` or `Typewriter`, the blueprint waits for them to finish automatically.
*   `Audio Source`: Choose whether to play a sound.
    *   `None`: No sound.
    *   `Built-in Sound Effect`: Play a sound from the clock's internal memory.
    *   `Stream from Home Assistant`: Stream an audio file from your HA media library.
*   `Repeat Count`: Loop the visual effect and built-in sound. (Streamed audio only plays once at the beginning).
*   `Restore Row After Effect`: When enabled, the display will return to its normal state after the effect is finished.

#### **Reference: Visual Effects**
| Visual Effect | Description | Type |
| :--- | :--- | :--- |
| `SET_TEXT` | Instantly displays static text. | Non-Blocking |
| `MARQUEE` | Scrolls text from right to left. | Blocking |
| `PULSE` | Gently fades text in and out. | Blocking |
| `FLASH` | Flashes text on and off. | Blocking |
| `TYPEWRITER` | Reveals text one character at a time. | Blocking |
| `SCRAMBLE_TEXT`| Displays random characters that resolve into the final text. | Blocking |
| `RANDOM_FLICKER_TEXT` | Rapidly flickers the text for a glitchy effect. | Blocking |

#### **Reference: Sound Effects**
| Sound Effect File | Description |
| :--- | :--- |
| `sys_beep.mp3` | A simple, single system beep. |
| `arrival_chime.mp3` | A pleasant chime, perfect for notifications. |
| `electric_sparks.mp3`| Crackling electrical sounds. |
| `engine_rev.mp3` | A powerful engine revving up. |
| `flux_capacitor_power_on.mp3`| The hum and crackle of the Flux Capacitor activating. |
| `hum.mp3` | A steady, low electronic hum. |
| `keypad_beeps.mp3` | A sequence of beeps from the time circuit keypad. |
| `lock_on.mp3` | A confirmation sound, as if a target is locked. |
| `relay_activation.mp3`| The click-clack of multiple mechanical relays. |
| `time_travel.mp3` | The full, iconic time travel sequence sound effect. |

#### **Use Case: Weather Alert**
This example displays the current temperature from a weather entity on the top row, with a "Temp: " prefix and a "°F" postfix. It uses the `Pulse` effect and plays a chime.

1.  **Create the Script:**
    *   Go to **Settings > Automations & Scenes > Blueprints**.
    *   Find the "BTTF Time Circuits: Display" blueprint and click **Create Script**.
    *   Name it "Time Circuits Weather Update".
    *   Configure the inputs:
        *   `Target Row`: `Top`
        *   `Data Source`: `Home Assistant Entity`
        *   `Entity to Display`: Your weather sensor (e.g., `weather.home`)
        *   `Attribute to Display`: `temperature`
        *   `Prefix`: `Temp: `
        *   `Postfix`: `°F`
        *   `Visual Effect`: `Pulse`
        *   `Display Duration (s)`: `10`
        *   `Audio Source`: `Built-in Sound Effect`
        *   `Sound Effect`: `arrival_chime.mp3`
    *   Save the script.

2.  **Create the Automation:**
    *   Create a new automation.
    *   **Trigger:** Use a `Time Pattern` trigger to run every 30 minutes.
    *   **Action:** Call the `script.time_circuits_weather_update` service.
    *   Save the automation.

---

## 🎮 Core Entities & Controls

The integration creates a device with a rich set of entities. You can use these in your own automations if you prefer not to use the blueprints.

| Entity Type | Name | Description |
| :--- | :--- | :--- |
| **Select** | `Display Mode` | Sets the main operating mode of the clock: `Normal Clock`, `Stock Ticker`, `Weather`, or `Data Link`. |
| **Text** | `Data Point 1-5 Marquee` | Sets the scrolling text for one of the five "Data Link" pages. |
| **Switch** | `Data Point 1-5 Enabled`| Enables or disables one of the five "Data Link" pages. |
| **Text** | `(all 12 segments)` | Provides direct, granular control over every individual display segment (e.g., `Destination Month`). |
| **Number** | `Brightness` | Adjusts the brightness of the displays (0-7). |
| **Number** | `Volume` | Adjusts the volume of the speaker (0-21). |
| **Button** | `Engage Time Circuits` | Manually starts the full time travel animation sequence. |
| **Button** | `Reboot Device` | Restarts the clock. |
| **Sensor** | `Status` | Monitors the clock's current state (e.g., `Idle`, `Animating`) and has useful attributes like `free_heap` and `wifi_rssi`. |
| **Update** | `Firmware` | Notifies you when a new firmware version is available and allows for one-click OTA updates. |

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
Save a "Favorite" radio station in the clock's web UI (**Sound** tab). Then, play it easily from Home Assistant.

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
| `data.sound_effect`| `string` | No | The filename of a sound effect to play (e.g., `REMINDER.mp3`). |

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

## ⚙️ Advanced Control (MQTT)

For ultimate control, you can publish directly to the clock's raw MQTT command topic. This gives you access to the powerful **Command Sequencer**, which allows you to create custom, multi-step animations.

*   **Topic**: `bttf_time_circuits/DEVICE_ID/cmnd/sequencer` (replace `DEVICE_ID` with your clock's ID)
*   **Payload**: A string with the name of a built-in animation, or a JSON object for a custom sequence.

> **This is an advanced feature.** For a complete guide on the sequencer, including all commands, parameters, and examples, please see the **[🤖 Developer Guide](../developer/developer-guide.md#command-sequencer-deep-dive)**.

---

## Troubleshooting

*   **Entities are 'Unavailable'?**
    *   Check the clock's Wi-Fi connection.
    *   In an MQTT client like [MQTT Explorer](http://mqtt-explorer.com/), check the `bttf_time_circuits/YOUR_DEVICE_ID/status` topic. It should have a retained message of `online`. If not, check the MQTT settings in the clock's web UI.

*   **Why does my text disappear immediately?**
    This usually happens when using the `Set Text` effect in a blueprint. This command is **non-blocking**. The script sends the command and immediately moves on. If there are no more steps, the script ends and the display restores.
    *   **Solution**: For non-blocking effects like `Set Text`, you **must** use the **`Display Duration`** input. This tells the blueprint to add a `WAIT` command, holding the text on-screen.
    *   Effects like `Marquee`, `Pulse`, and `Countdown` are **blocking**; they have their own duration and do not require a separate `Display Duration`.

*   **Can I combine sensor data with my own text?**
    Absolutely! All text fields in the blueprints support Home Assistant templates. This lets you build rich, dynamic strings.
    *Example for the "Display Entity" blueprint's Postfix field:*
    ```jinja
    {{ state_attr('weather.home', 'temperature') }}°F
    ```