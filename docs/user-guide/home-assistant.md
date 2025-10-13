# 🏠 Home Assistant Integration Guide

Welcome, time traveler! This guide provides everything you need to know to integrate your Time Circuits clock with Home Assistant. From initial setup to creating advanced automations, you'll find it all here.

### **Table of Contents**
1. [Features](#-features)
2. [Setup & Installation](#-setup--installation)
3. [Understanding Blueprints, Scripts, and Automations](#-understanding-blueprints-scripts-and-automations)
4. [Importing the Blueprints](#-importing-the-blueprints)
5. [Blueprint Showcase](#-blueprint-showcase)
    * [Display: The All-in-One Blueprint](#1-display-the-all-in-one-blueprint)
    * [Multi-Row Status Board](#2-multi-row-status-board)
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

1.  **Navigate to Blueprints in Home Assistant**:
    *   Open your Home Assistant web interface.
    *   Go to **Settings** > **Automations & Scenes**.
    *   Select the **Blueprints** tab.

2.  **Import a Blueprint**:
    *   Click the **Import Blueprint** button in the bottom right corner.
    *   A dialog box will appear asking for a URL. Paste one of the URLs below.
    *   Click **Preview Blueprint**, then **Import Blueprint**.

3.  **Blueprint URLs (Copy and Paste)**:
    *   **Display**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display.yaml`
    *   **Multi-Row Status Board**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/multi_row_status_board.yaml`
    *   **Countdown Timer**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/countdown.yaml`

Repeat the import process for each blueprint you wish to use.

---

## 🎬 Blueprint Showcase

This section is your comprehensive guide to each blueprint. You'll find detailed explanations of all available inputs and practical examples to help you build powerful automations.

### **1. Display: The All-in-One Blueprint**

This is the most versatile blueprint for showing information on the Time Circuits. It can display either a fixed piece of text or the live state of any Home Assistant entity.

#### **How It Works**

The `Data Source` selector is the key to this blueprint.
*   If you select **"Static Text"**, you can type any message directly into the `Text to Display` field. This field also supports templates.
*   If you select **"Home Assistant Entity"**, you can choose any entity. The blueprint will automatically fetch its state and display it. You can optionally add a `Prefix` and `Postfix` to the value.

#### **Inputs**
*   **Time Circuits Device**: The clock you want to control.
*   **Target Row**: Which row(s) to show the message on (`Top`, `Middle`, `Bottom`, or `All`).
*   **Data Source**: `Static Text` or `Home Assistant Entity`.
*   **Text to Display**: The static message to show. *Only appears if Data Source is "Static Text".*
*   **Entity to Display**: The entity to monitor. *Only appears if Data Source is "Home Assistant Entity".*
*   **Attribute to Display**: (Optional) A specific attribute of the entity to show instead of its main state.
*   **Prefix / Postfix**: (Optional) Text to add before or after the entity's value.
*   **Visual Effect**: The animation to use. See the reference table below.
*   **Display Duration (s)**: The total time the effect should last. This is now used by `SET_TEXT`, `PULSE`, `FLASH`, `TYPEWRITER`, and `SCRAMBLE_TEXT`.
*   **Marquee Speed (ms)**: The scroll speed for the marquee effect (lower is faster).
*   **Flicker Speed (ms)**: The update speed for the Random Flicker effect (lower is faster).
*   **Audio Source**: `None`, `Built-in Sound Effect`, or `Stream from Home Assistant`.
*   **Sound Effect / Media File**: The specific sound or media file to play.
*   **Volume**: The playback volume (0-100%).
*   **Repeat Count**: How many times to repeat the visual effect.
*   **Restore Row After Effect**: If enabled, the row returns to its normal state after the effect.

#### **Example 1: Static Text Alert**
Display a "DOORBELL" message on the middle row for 15 seconds with a chime.

1.  **Create the Script** using the `Display` blueprint.
2.  **Configure Inputs:**
    *   `Data Source`: `Static Text`
    *   `Target Row`: `Middle`
    *   `Text to Display`: `DOORBELL`
    *   `Visual Effect`: `Set Text`
    *   `Display Duration (s)`: `15`
    *   `Audio Source`: `Built-in Sound Effect`
    *   `Sound Effect`: `arrival_chime.mp3`
3.  **Create an Automation** that calls this script when your doorbell sensor is triggered.

#### **Example 2: Live Weather Display**
Show the current outdoor temperature on the top row, updated automatically.

1.  **Create the Script** using the `Display` blueprint.
2.  **Configure Inputs:**
    *   `Data Source`: `Home Assistant Entity`
    *   `Target Row`: `Top`
    *   `Entity to Display`: `weather.home` (or your weather entity)
    *   `Attribute to Display`: `temperature`
    *   `Postfix`: ` F` (note the leading space)
    *   `Visual Effect`: `Set Text`
    *   `Display Duration (s)`: `60` (the message will show for one minute)
    *   `Restore Row After Effect`: `True`
3.  **Create an Automation:**
    *   **Trigger**: `State` trigger, watching `weather.home`.
    *   **Action**: `Call service`, and select the script you just created.

### **2. Multi-Row Status Board**
This blueprint lets you display different information on all three rows *simultaneously*.

#### **How It Works**
You are given a text and effect input for each row (`Top`, `Middle`, `Bottom`). The blueprint generates a single command with three parallel animation tracks. Leaving a row's text field blank will exclude it from the animation.

#### **Inputs**
*   **Time Circuits Device**: The clock to control.
*   **Top/Middle/Bottom Row Text**: The text to display on each respective row.
*   **Top/Middle/Bottom Row Effect**: The visual effect for each respective row.
*   **Display Duration (s)**: A *single* duration that applies to all active rows.
*   **Marquee Speed (ms)**: A *single* marquee speed that applies to any row using the marquee effect.
*   **Restore Rows After Effect**: Restores all affected rows to their normal state.

#### **Example: System Status Display**
Show CPU temperature, memory usage, and processor load all at once.

1.  **Create the Script** using the `Multi-Row Status Board` blueprint.
2.  **Configure Inputs:**
    *   `Top Row Text`: `CPU: {{ states('sensor.processor_temperature') }} C`
    *   `Top Row Effect`: `Set Text`
    *   `Middle Row Text`: `MEM: {{ states('sensor.memory_use_percent') }} %`
    *   `Middle Row Effect`: `Set Text`
    *   `Bottom Row Text`: `LOAD: {{ states('sensor.processor_use') }} %`
    *   `Bottom Row Effect`: `Pulse`
    *   `Display Duration (s)`: `30`
3.  **Create an Automation** that calls this script on a `Time pattern` trigger (e.g., every 5 minutes).

### **3. Countdown Timer**
This blueprint runs a numerical countdown on the display, with an optional message and sound at the end.

#### **Inputs**
*   **Time Circuits Device**: The clock to control.
*   **Target Row**: Which row(s) to show the countdown on.
*   **Start Number**: The number to count down from.
*   **Countdown Delay (ms)**: The time between each number change.
*   **End Text**: (Optional) A message to display when the countdown finishes.
*   **End Text Visual Effect**: The effect for the end text.
*   **End Text Display Duration (s)**: The total duration for the end text effect.
*   **Audio Source / Sound Effect / Media File**: Sound to play *after* the countdown completes.
*   **Volume**: Playback volume for the end sound.

#### **Example: Microwave Timer**
Run a 30-second countdown on the middle row, ending with a flashing "DONE" message and a chime.

1.  **Create the Script** using the `Countdown Timer` blueprint.
2.  **Configure Inputs:**
    *   `Target Row`: `Middle`
    *   `Start Number`: `30`
    *   `Countdown Delay (ms)`: `1000`
    *   `End Text`: `DONE`
    *   `End Text Visual Effect`: `Flash`
    *   `End Text Display Duration (s)`: `5`
    *   `Audio Source`: `Built-in Sound Effect`
    *   `Sound Effect`: `arrival_chime.mp3`
3.  **Expose to Voice Assistant**: Expose the created script to your voice assistant (e.g., Google Assistant, Alexa) to run it with a simple voice command like "run the microwave timer".

---
### **Visual Effects Reference**

| Visual Effect | Description | Duration Control |
| :--- | :--- | :--- |
| `SET_TEXT` | Instantly displays the static text. | `Display Duration` adds a `WAIT` command. |
| `MARQUEE` | Scrolls the text from right to left. | Duration depends on text length and `Marquee Speed`. |
| `PULSE` | Gently fades the text in and out in a fixed 2-second cycle. | `Display Duration` sets the total run time. |
| `FLASH` | Flashes the text on and off rapidly. | `Display Duration` sets the total run time. |
| `TYPEWRITER` | Reveals text one character at a time. | Speed is auto-calculated to fill the `Display Duration`. |
| `SCRAMBLE_TEXT`| Displays random characters that resolve into the final text. | Speed is auto-calculated to fill the `Display Duration`. |
| `RANDOM_FLICKER`| Makes the text appear unstable by rapidly replacing characters. | `Display Duration` sets total run time. `Flicker Speed` controls update rate. |

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

## ⚙️ Advanced Control (MQTT)

For ultimate control, you can publish directly to the clock's raw MQTT command topic. This gives you access to the powerful **Command Sequencer**, which allows you to create custom, multi-step animations.

*   **Topic**: `bttf_time_circuits/DEVICE_ID/sequencer/command` (replace `DEVICE_ID` with your clock's ID)
*   **Payload**: A JSON array of tracks.

> **This is an advanced feature.** For a complete guide on the sequencer, including all commands, parameters, and examples, please see the **[🤖 Developer Guide](../developer/developer-guide.md#command-sequencer-deep-dive)**.

---

## Troubleshooting

*   **Entities are 'Unavailable'?**
    *   Check the clock's Wi-Fi connection.
    *   In an MQTT client like [MQTT Explorer](http://mqtt-explorer.com/), check the `bttf_time_circuits/YOUR_DEVICE_ID/status` topic. It should have a retained message of `online`. If not, check the MQTT settings in the clock's web UI.

*   **Blueprint fails with "Message malformed" or "TemplateSyntaxError"**
    *   This can happen if you are using an older version of Home Assistant Core. Please ensure you are on the latest version. Some of the templating features used in the blueprints require a recent version of Home Assistant.
    *   If you are on the latest version and still see this, please open an issue on the project's GitHub page.

*   **Can I combine sensor data with my own text?**
    Absolutely! All text fields in the blueprints support Home Assistant templates. This lets you build rich, dynamic strings.
    *Example for the "Display" blueprint's `Prefix` and `Postfix` fields:*
    ```jinja
    # In the Prefix field:
    TEMP: {{ state_attr('weather.home', 'temperature') }}

    # In the Postfix field:
    C
    ```