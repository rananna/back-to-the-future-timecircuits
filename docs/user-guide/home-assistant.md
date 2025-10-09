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

1.  **Navigate to Blueprints in Home Assistant**:
    *   Open your Home Assistant web interface.
    *   Go to **Settings** > **Automations & Scenes**.
    *   Select the **Blueprints** tab.

2.  **Import a Blueprint**:
    *   Click the **Import Blueprint** button in the bottom right corner.
    *   A dialog box will appear asking for a URL. Paste one of the URLs below.
    *   Click **Preview Blueprint**, then **Import Blueprint**.

3.  **Blueprint URLs (Copy and Paste)**:
    *   **Display Text**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_text.yaml`
    *   **Display Entity**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_entity.yaml`
    *   **Countdown Timer**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/countdown.yaml`

Repeat the import process for each blueprint you wish to use.

---

## 🏆 Blueprint Showcase

Here you'll find a detailed guide to each blueprint, including its purpose, all available inputs, and practical examples to get you started.

### **1. Display Text**
*   **What it's for:** Your go-to for showing any custom message. It's perfect for alerts, notifications, and simple status updates. With full control over visual effects, sound, and looping, you can create rich, multi-sensory alerts.
*   **URL for Import**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_text.yaml`

#### **Inputs**
| Input | Description | Default Value |
| :--- | :--- | :--- |
| `Time Circuits Device` | The clock you want to control. | - |
| `Target Row` | Which of the three rows (**Top, Middle, Bottom, or All**) to use. | `Top` |
| `Text to Display` | The message to show. **Supports templates!** | `(empty)` |
| `Visual Effect` | The animation style (`Set Text`, `Marquee`, `Pulse`, `Flash`, etc.). | `Set Text` |
| `Display Duration (s)` | How long to keep **non-blocking** effects (like `Set Text`) on screen. Blocking effects like `Marquee` or `Pulse` have their own duration. | `10` |
| `Marquee Speed (ms)` | Scroll speed for the marquee effect (lower is faster). | `100` |
| `Audio Source` | Choose `None`, `Built-in Sound Effect`, or `Stream from Home Assistant`. | `None` |
| `Sound Effect` | If using a built-in sound, select it from the list. | `sys_beep.mp3` |
| `Media File` | If streaming, pick an audio file from your HA media library. | - |
| `Volume` | A slider (0-100) to control the volume of the alert. | `80` |
| `Repeat Count` | How many times to loop the alert. Streamed audio plays only once. | `1` |
| `Restore Row` | If checked, the row returns to its normal state after the effect. | `true` |

#### **Use Case: Doorbell Alert**
This example shows a "DOORBELL" message that flashes on all three rows, plays a chime sound, and repeats 3 times.

1.  **Create the Script:**
    *   Go to **Settings > Automations & Scenes > Blueprints**.
    *   Find the "BTTF Time Circuits: Display Text" blueprint and click **Create Script**.
    *   Name the script something descriptive, like "Time Circuits Doorbell Alert".
    *   Configure the inputs:
        *   `Target Row`: `All Rows`
        *   `Text to Display`: `DOORBELL`
        *   `Visual Effect`: `Flash`
        *   `Audio Source`: `Built-in Sound Effect`
        *   `Sound Effect`: `arrival_chime.mp3`
        *   `Repeat Count`: `3`
    *   Save the script.

2.  **Create the Automation:**
    *   Go to **Settings > Automations & Scenes > Automations**.
    *   Click **Create Automation** and select **Start with an empty automation**.
    *   **Trigger:** Set the trigger to your doorbell's sensor (e.g., `binary_sensor.doorbell_ding` changes to `on`).
    *   **Action:**
        *   `Action type`: `Call service`
        *   `Service`: Find the script you just created (e.g., `script.time_circuits_doorbell_alert`).
    *   Save the automation.

---

### **2. Display Entity**
*   **What it's for:** The most powerful blueprint for displaying live data. Show the state or a specific attribute of any Home Assistant entity, complete with prefixes, postfixes, and all the audio/visual effects.
*   **URL for Import**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_entity.yaml`

#### **Inputs**
| Input | Description | Default Value |
| :--- | :--- | :--- |
| *All inputs from Display Text* | (see above) | - |
| `Entity to Display` | The sensor or other entity you want to show. | - |
| `Attribute to Display` | Optional. Leave blank to show the entity's main state, or enter an attribute name (e.g., `temperature`) to display its value. | `(empty)` |
| `Prefix` | Optional text to add *before* the value (e.g., a "TEMP: "). | `(empty)` |
| `Postfix` | Optional text to add *after* the value (e.g., a "°F"). | `(empty)` |

#### **Use Case: Dynamic Weather Display**
This example displays the current weather forecast and temperature from a weather entity, updating automatically whenever the forecast changes.

1.  **Create the Script:**
    *   Use the "BTTF Time Circuits: Display Entity" blueprint to create a script named "Time Circuits Weather Display".
    *   Configure the inputs:
        *   `Target Row`: `Top Row`
        *   `Entity to Display`: Your weather entity (e.g., `weather.home`).
        *   `Prefix`: `FCST `
        *   `Postfix`: ` {{ state_attr('weather.home', 'temperature') }} F`  *(Note the use of a template in the postfix!)*
        *   `Visual Effect`: `Typewriter`
        *   `Audio Source`: `None`
    *   Save the script.

2.  **Create the Automation:**
    *   Create a new, empty automation.
    *   **Trigger:**
        *   `Trigger type`: `State`
        *   `Entity`: Your weather entity (e.g., `weather.home`).
    *   **Action:**
        *   `Action type`: `Call service`
        *   `Service`: `script.time_circuits_weather_display`.
    *   Save the automation. Now, your Time Circuits will always show the latest forecast!

---

### **3. Countdown Timer**
*   **What it's for:** Perfect for building anticipation for movie night, a gaming session, or just counting down to dinner. When the timer hits zero, it can display a final message with its own unique sound and visual effect.
*   **URL for Import**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/countdown.yaml`

#### **Inputs**
| Input | Description | Default Value |
| :--- | :--- | :--- |
| `Time Circuits Device` | The clock you want to control. | - |
| `Target Row` | Which of the three rows (**Top, Middle, Bottom, or All**) to use. | `Top` |
| `Start Number` | The number to start counting down from (1-9999). | `10` |
| `Countdown Delay (ms)` | The delay between each number change (1000ms = 1s). | `1000` |
| `End Text` | Optional text to display when the countdown finishes. | `LIFTOFF` |
| `End Text Visual Effect` | A separate visual effect for the end text. | `Set Text` |
| `End Text Display Duration (s)` | Duration for the non-blocking end text effect. | `5` |
| `End Text Marquee Speed (ms)` | Marquee speed for the end text. | `100` |
| `End Audio Source` | Choose `None`, `Built-in Sound Effect`, or `Stream from Home Assistant`. | `None` |
| `End Sound Effect` | A built-in sound effect to play from the device. | `arrival_chime.mp3` |
| `End Media File`| An audio file from the HA media library for the end effect. | - |
| `End Audio Volume`| Volume for the sound when the countdown finishes (0-100). | `80` |
| `Repeat End Effect Count` | Number of times to repeat the end effect. Streamed audio will not repeat. | `1` |
| `Restore Row` | If checked, the row returns to normal after all effects complete. | `true` |

#### **Use Case: Movie Night Countdown Button**
This example runs a 10-second countdown. When it finishes, it displays "MOVIE TIME" with a scramble effect and plays a sound.

1.  **Create the Script:**
    *   Use the "BTTF Time Circuits: Countdown Timer" blueprint to create a script named "Movie Night Countdown".
    *   Configure the inputs:
        *   `Target Row`: `Middle Row`
        *   `Start Number`: `10`
        *   `End Text`: `MOVIE TIME`
        *   `End Text Visual Effect`: `Scramble Text`
        *   `End Audio Source`: `Built-in Sound Effect`
        *   `End Sound Effect`: `time_travel.mp3`
    *   Save the script.

2.  **Add a Button to your Dashboard:**
    *   Since this is a manual action, you don't need an automation.
    *   Go to your Home Assistant dashboard, enter edit mode, and add a **Button Card**.
    *   Set the `Tap Action` to `Call Service`.
    *   Set the `Service` to `script.movie_night_countdown`.
    *   Now you have a one-touch button to start your movie night!

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