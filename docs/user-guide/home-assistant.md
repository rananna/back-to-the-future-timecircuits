# 🏠 Home Assistant Integration Guide

Welcome, time traveler! This guide provides everything you need to know to integrate your Time Circuits clock with Home Assistant. From initial setup to creating advanced automations, you'll find it all here.

### **Table of Contents**
1. [Features](#-features)
2. [Setup & Installation](#-setup--installation)
3. [Understanding Blueprints, Scripts, and Automations](#-understanding-blueprints-scripts-and-automations)
4. [Importing the Blueprints](#-importing-the-blueprints)
5. [Available Blueprints: A Deep Dive](#-available-blueprints-a-deep-dive)
6. [Core Entities & Controls](#-core-entities--controls)
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

## 🛑 Prerequisites

*   A running Home Assistant instance.
*   [HACS](https://hacs.xyz/) (Home Assistant Community Store) installed.
*   **MQTT Broker Configured in Home Assistant**: You must have the MQTT integration set up and connected to your broker. The Time Circuits clock does not connect directly to HA, but to your MQTT broker.
*   **Clock Connected to WiFi**: The Time Circuits Clock must be powered on and connected to your Wi-Fi network.
*   **Clock Configured for MQTT**: In the clock's web UI, ensure the MQTT broker details are correctly configured under the **Connectivity** tab.

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
3.  You will be prompted for your clock's **Device ID**. You can find this in the clock's web interface under **System -> System Status**.
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

## 📥 Importing the Blueprint

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

## 📖 The Display Blueprint: Your All-in-One Tool

To simplify the integration, all functionality has been consolidated into a single, powerful **Display Blueprint**. This is your central tool for creating any kind of visual alert or message on the Time Circuits clock.

It's a versatile, all-in-one blueprint for showing information on the display. You can write a custom message, show the state of any Home Assistant entity, apply various visual effects, and add sound.

#### **Core Capabilities:**

*   **Display Any Data**: Show a fixed, static message (e.g., "WELCOME HOME") or dynamically display the state of any Home Assistant entity (e.g., the current temperature from a sensor). You can even use templates to combine them (e.g., "Temp: {{ states('sensor.outside_temperature') }}°F").
*   **Target a Specific Row**: Choose whether your message appears on the TOP, MIDDLE, BOTTOM, or all three rows simultaneously.
*   **Rich Visual Effects**: Select from a wide range of animations (like `Marquee`, `Typewriter`, `Pulse`, `Flash`, and `Scramble Text`) to make your message stand out.
*   **Sound Effects**: Add an audible alert by playing one of the device's built-in sound effects or by streaming any audio file from your Home Assistant media library.
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
    While there is no dedicated "countdown" function, you can achieve a similar result using an automation with a `repeat` loop and the `display` blueprint. For most use cases, a simple timed alert is sufficient (e.g., show "TIMER DONE" after a delay).

---

## 🎮 Core Entities & Controls

The integration creates a device with a rich set of entities. You can use these in your own automations if you prefer not to use the blueprints.

| Entity Type | Name | Description |
| :--- | :--- | :--- |
| **Select** | `Display Mode` | Sets the main operating mode: `Normal Clock`, `Stock Ticker`, `Weather`, or `Data Link`. |
| **Select** | `Default Animation Sequence` | Sets the animation that plays when saving settings in the web UI. |
| **Select** | `Run Animation` | A special dropdown to manually trigger any built-in animation. |
| **Switch** | `Override Switch` | A master switch to force the display to show manually set text. |
| **Switch** | `24h Format` | Toggles the main clock between 12-hour and 24-hour time formats. |
| **Switch** | `Time Travel Sounds` | Enables or disables the sound effects that play during animations. |
| **Number** | `Brightness` | Adjusts the brightness of the displays (0-7). |
| **Number** | `Stock Refresh` | Sets the refresh interval for the stock ticker mode (1-60 minutes). |
| **Button** | `Time Travel` | Manually starts the full time travel animation sequence. |
| **Button** | `Favorite Radio Station` | Plays the favorite radio station configured in the web UI. |
| **Button** | `Reboot Device` | Restarts the clock. |
| **Button** | `Force NTP Sync` | Manually syncs the clock's time with an internet time server. |
| **Button** | `Factory Reset` | Resets all device settings to their defaults. |
| **Button** | `Refresh Weather Data` | Manually fetches the latest data for the weather display mode. |
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

*   **Topic**: `bttf_time_circuits/DEVICE_ID/sequencer/command` (replace `DEVICE_ID` with your clock's ID)
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