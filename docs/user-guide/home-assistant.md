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

## 📥 Importing the Blueprints

The easiest way to add the blueprints is by importing them directly from the project's GitHub repository. This ensures you always have the most up-to-date version.

1.  **Navigate to Blueprints in Home Assistant**: Go to **Settings > Automations & Scenes** and select the **Blueprints** tab.
2.  **Import a Blueprint**: Click the **Import Blueprint** button in the bottom right corner.
3.  **Paste the URL**: In the dialog box, paste one of the URLs below into the "URL of the Blueprint to import" field.
4.  **Preview and Import**: Click **Preview Blueprint**. Home Assistant will show you the details. If it looks correct, click **Import Blueprint**.

You'll need to repeat this copy-paste process for each of the blueprints you want to use.

#### **Blueprint URLs (Click to Copy)**
*   **Display Blueprint:**
    ```
    https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display.yaml
    ```
*   **Countdown Timer Blueprint:**
    ```
    https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/countdown.yaml
    ```
*   **Multi-Row Status Board Blueprint:**
    ```
    https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/multi_row_status_board.yaml
    ```

---

## 📖 Available Blueprints: A Deep Dive

This section provides a detailed look at each blueprint to help you choose the right one for your automation. The descriptions in the Home Assistant UI are powered by the comments within each blueprint file and serve as the primary source of truth for all options.

---

### **1. Display Blueprint: The All-Rounder**
This is the most versatile and commonly used blueprint. It's a powerful all-in-one tool for showing a single piece of information on a specific display row. It can display static text, the state of any Home Assistant entity, or a combination of both using templates.

#### **When to Use It:**
*   You want to display a single, important piece of data (e.g., the outdoor temperature).
*   You need to show a simple, custom alert message (e.g., "GARAGE DOOR OPEN").
*   You want to announce the title of the currently playing song on a media player.

#### **Key Features:**
*   **Target a Specific Row**: Choose whether your message appears on the TOP, MIDDLE, or BOTTOM row.
*   **Rich Visual Effects**: Select from a wide range of entry and exit animations (like `Marquee`, `Typewriter`, `Scramble Text`) to make your message stand out.
*   **Timed Display**: Set a duration to keep the message on screen for a specific amount of time before it disappears.
*   **Sound Effects**: Add an audible alert by playing a built-in or custom sound effect.

#### **Example Use Case:**
_"When my 3D printer finishes, show the message 'PRINT COMPLETE' on the top row with a chime sound for 5 minutes."_
*   **Blueprint**: Display
*   **Trigger**: `3D Printer State changes to 'finished'`
*   **Text to Display**: `PRINT COMPLETE`
*   **Display Row**: `TOP`
*   **Display Duration**: `300` seconds
*   **Sound Effect**: `arrival_chime.mp3`

---

### **2. Countdown Timer Blueprint: The Specialist**
This blueprint is designed for one specific task: running a numeric countdown on one or more display rows. When the timer hits zero, it can trigger a follow-up visual effect and play a sound for a grand finale.

#### **When to Use It:**
*   You need a visual kitchen timer.
*   You want to create a dramatic "launch sequence" for an automation.
*   You need a timer for a child's timeout or a game.

#### **Key Features:**
*   **Multi-Row Countdown**: Display the countdown on one, two, or all three rows simultaneously.
*   **Customizable Finale**: When the countdown finishes, you can show a final message (e.g., "LIFTOFF!"), play a sound, or stream audio.
*   **Flexible Audio**: Stream the finale sound to any speaker in your home, not just the clock's speaker.

#### **Example Use Case:**
_"Create a 10-second countdown for my 'Movie Time' scene. When it finishes, show 'ENJOY THE SHOW' and play a cinematic sound on my living room speakers."_
*   **Blueprint**: Countdown Timer
*   **Trigger**: `Button press for 'Movie Time'`
*   **Countdown Duration**: `10` seconds
*   **Completion Message**: `ENJOY THE SHOW`
*   **Streamed Media**: `local/sounds/cinematic_hit.mp3`
*   **Audio Output**: `media_player.living_room_speaker`

---

### **3. Multi-Row Status Board Blueprint: The Power User's Tool**
This is the most advanced blueprint, allowing you to control all three display rows independently and simultaneously from a single script. You can set a different message and visual effect for the top, middle, and bottom rows, creating a dense, information-rich display.

#### **When to Use It:**
*   You want to build a custom "dashboard" screen showing multiple pieces of information at once (e.g., weather, time, and a stock price).
*   You need to display a complex, multi-line alert message.
*   You want to create advanced, parallel animation effects across all three rows.

#### **Key Features:**
*   **Independent Row Control**: Configure the text, effect, and duration for the TOP, MIDDLE, and BOTTOM rows separately.
*   **Synchronized Animations**: All three rows animate in and out at the same time, creating a clean, professional look.
*   **Single Script Simplicity**: Manage a complex, three-line display from a single, easy-to-use script.

#### **Example Use Case:**
_"Every morning at 8 AM, show a status screen: the weather forecast on top, the current date in the middle, and my portfolio value on the bottom. Hold for 1 minute."_
*   **Blueprint**: Multi-Row Status Board
*   **Trigger**: `Time is 8:00 AM`
*   **Top Row Text**: `{{ states('weather.home') }}`
*   **Middle Row Text**: `{{ now().strftime('%B %d') }}`
*   **Bottom Row Text**: `TSLA: ${{ states('sensor.tsla_stock') }}`
*   **Display Duration**: `60` seconds

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