# 🏠 Home Assistant Integration Guide

This project features a native Home Assistant integration that provides a seamless and powerful way to control your Time Circuits clock.

### **Table of Contents**
1. [Features](#-features)
2. [Setup & Installation](#-setup--installation)
3. [Getting Started with Blueprints](#-getting-started-with-blueprints)
    * [Importing Blueprints](#importing-blueprints)
    * [Key Concepts](#key-concepts)
    * [Blueprint Showcase](#blueprint-showcase)
4. [Core Entities & Controls](#-core-entities--controls)
5. [Using the Media Player](#-using-the-media-player)
6. [Sending Notifications](#-sending-notifications)
7. [Advanced Control (MQTT)](#-advanced-control-mqtt)
8. [Troubleshooting](#troubleshooting)

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
*   **Clock Configured for MQTT**: In the clock's web UI, ensure the MQTT broker details are correctly configured under the **Data Link** tab.

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

## 🤖 Getting Started with Blueprints

Welcome, time traveler! For most automations, the easiest and most powerful way to create custom animations and notifications is with our **Home Assistant Blueprints**. They provide a simple, form-based UI inside Home Assistant, allowing you to build complex effects without writing any code.

Whether you want to see the outside temperature, get a visual alert when the doorbell rings, or run a countdown to movie night, you've come to the right place.

### Importing Blueprints

The easiest way to add the blueprints is by importing them directly from the project's GitHub repository. This ensures you always have the most up-to-date version.

1.  **Navigate to Blueprints in Home Assistant**:
    *   Open your Home Assistant web interface.
    *   Go to **Settings** > **Automations & Scenes**.
    *   Select the **Blueprints** tab.

2.  **Import a Blueprint**:
    *   Click the **Import Blueprint** button in the bottom right corner.
    *   A dialog box will appear asking for a URL. You will need to provide the URL to the specific blueprint file you want to add.

3.  **Get the Blueprint URLs**:
    *   The blueprints are located in the [`home_assistant/blueprints/`](https://github.com/rananna/back-to-the-future-timecircuits/tree/main/home_assistant/blueprints/) directory of the repository.
    *   You will need to import each blueprint you want to use. Here are the direct links:
        *   **Display Text**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_text.yaml`
        *   **Display Entity**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_entity.yaml`
        *   **Countdown Timer**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/countdown.yaml`

4.  **Paste and Preview**:
    *   Paste a URL into the "URL to import" field.
    *   Click **Preview Blueprint**. Home Assistant will fetch the file and show you its details.
    *   If the preview looks correct, click **Import Blueprint** to save it to your instance.

Repeat this process for all blueprints you wish to use.

### Key Concepts

To unlock the full potential of your Time Circuits, it's crucial to understand a few core concepts.

#### **1. Scripts vs. Automations: The "What" and the "When"**

When you fill out a blueprint, you are creating a **Script**.
*   A **Script** defines *what* you want to happen. It's a single, specific mission like "Show the outside temperature with a pulse effect and a beep sound." When you run the script, it executes its commands and then it's done.

To make things happen automatically, you need an **Automation**.
*   An **Automation** defines *when* you want your script to run. It's the trigger—like a sensor changing, a specific time of day, or a button press—that executes your script.

> **The Golden Rule:** You create a **Script** from a blueprint to define the action. Then, you create an **Automation** to call that script based on a trigger.

#### **2. Blocking vs. Non-Blocking Effects: "Why Did My Text Disappear?"**

This is the most common point of confusion, and understanding it is key to building successful automations.

*   **Non-Blocking Effects (`SET_TEXT`)**: These effects execute instantly. The command is sent, and the script immediately moves to the next step. If there are no more steps, the script ends, and the display row is restored to its previous state.
    *   **Solution**: To keep static text visible, you **must** use the **`Display Duration`** input. This tells the blueprint to add a `WAIT` command to the sequence, holding the text on-screen for the time you specify.

*   **Blocking Effects (`MARQUEE`, `PULSE`, `FLASH`, `TYPEWRITER`, `SCRAMBLE_TEXT`, `COUNTDOWN`)**: These effects have a built-in duration. The script will **wait** for them to finish before moving on. They do **not** require a separate `Display Duration`.

#### **3. How Audio Works: The Three Choices**

The blueprints give you three options for accompanying your visual effects with sound.

1.  **None**: The simplest option. No sound will play.
2.  **Built-in Sound Effect**: Plays a sound file stored directly on the clock's memory. This is fast, reliable, and does not depend on your network.
    *   The sound will loop if the `Repeat Count` is greater than 1.
    *   The volume is controlled by the `Volume` slider in the blueprint.
3.  **Stream from Home Assistant**: Plays an audio file from your Home Assistant media library. This is great for custom sounds or longer clips.
    *   The audio will play **once** at the beginning of the sequence, even if the `Repeat Count` is greater than 1.

#### **4. The Power of Templates: Making Your Display Dynamic**

Almost all text fields in the blueprints support Home Assistant templates. This is how you unlock the real power of the device by combining static text with live data from your smart home.

Instead of just displaying "TEMPERATURE", you can display the actual temperature:
```jinja
OUTSIDE: {{ states('sensor.outside_temperature') }}°F
```

You can combine multiple entities and text:
```jinja
BACK DOOR {{ states('binary_sensor.back_door_contact') | upper }}
```
This allows you to create rich, informative displays that are tailored to your specific needs.

### Blueprint Showcase

Here you'll find a detailed guide to each blueprint, including its purpose, all available inputs, and practical examples to get you started.

---

#### **1. Display Text**

*   **URL for Import**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_text.yaml`

**What it's for:** This is your workhorse for showing any custom message. It's perfect for alerts, notifications, and simple status updates. With full control over visual effects, sound, and looping, you can create rich, multi-sensory alerts.

**Inputs:**
*   `Time Circuits Device`: The clock you want to control.
*   `Target Row`: Which of the three rows (**Top, Middle, Bottom, or All Rows**) to use.
*   `Text to Display`: The message to show. **Supports templates!**
*   `Visual Effect`: The animation style for the text (`Set Text`, `Marquee`, `Pulse`, `Flash`, `Typewriter`, `Scramble Text`).
*   `Display Duration (s)`: How long to keep non-blocking effects on screen.
*   `Marquee Speed (ms)`: Scroll speed for the marquee effect (lower is faster).
*   `Audio Source`: Choose `None`, `Built-in Sound Effect`, or `Stream from Home Assistant`.
*   `Sound Effect`: If using a built-in sound, select it from the list.
*   `Media File`: If streaming, pick an audio file from your HA media library.
*   `Volume`: A slider (0-100) to control the volume of the alert.
*   `Repeat Count`: How many times to loop the alert.
*   `Restore Row After Effect`: If checked, the row returns to its normal state after the effect finishes.

**Use Case Example: Doorbell Alert**

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
    *   **Trigger:** Set the trigger to your doorbell's sensor (e.g., `binary_sensor.doorbell_ding`).
    *   **Action:**
        *   `Action type`: `Call service`
        *   `Service`: Find the script you just created (e.g., `script.time_circuits_doorbell_alert`).
    *   Save the automation.

---

#### **2. Display Entity**

*   **URL for Import**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display_entity.yaml`

**What it's for:** The most powerful blueprint for displaying live, dynamic data from your smart home. Show the state or a specific attribute of any Home Assistant entity, complete with prefixes, postfixes, and all the audio/visual effects from the Display Text blueprint.

**Inputs:**
*   *Includes all the advanced controls from the Display Text blueprint.*
*   `Entity to Display`: The sensor or other entity you want to show.
*   `Attribute to Display (Optional)`: The real power-up. Leave blank to show the entity's main state, or enter an attribute name (e.g., `temperature` for a weather entity) to display its specific value.
*   `Prefix`: Optional text to add *before* the value (e.g., a "TEMP: ").
*   `Postfix`: Optional text to add *after* the value (e.g., a "°F").

**Use Case Example: Dynamic Weather Display**

This example displays the current weather forecast and temperature from a weather entity on the top row, updating automatically whenever the forecast changes.

1.  **Create the Script:**
    *   Use the "BTTF Time Circuits: Display Entity" blueprint to create a new script.
    *   Name it "Time Circuits Weather Display".
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

#### **3. Countdown Timer**

*   **URL for Import**: `https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/countdown.yaml`

**What it's for:** Perfect for building anticipation for movie night, a gaming session, or just counting down to dinner time. When the timer hits zero, it can display a final message with its own unique sound and visual effect.

**Inputs:**
*   `Time Circuits Device`, `Target Row`: Same as other blueprints.
*   `Start Number`: The number to start counting down from.
*   `Countdown Delay (ms)`: The delay between each number change (1000ms = 1s).
*   `End Text`: A message to display when the countdown hits zero.
*   `End Text Visual Effect`: A separate visual effect for the end text.
*   `End Text Display Duration (s)`: Duration for the end text effect.
*   `End Text Marquee Speed (ms)`: Marquee speed for the end text.
*   `End Audio Source`, `End Sound Effect`, `End Media File`, `End Audio Volume`: A full audio suite for the moment the countdown finishes.
*   `Repeat End Effect Count`: Loop the end text and sound for extra emphasis.
*   `Restore Row`: If checked, the row returns to normal after all effects complete.

**Use Case Example: Movie Night Countdown**

This example runs a 10-second countdown on the middle row. When it finishes, it displays "MOVIE TIME" with a scramble effect and plays a sound.

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

2.  **Use in a Dashboard:**
    *   Since this is a manual action, you don't need an automation.
    *   Go to your Home Assistant dashboard, enter edit mode, and add a **Button Card**.
    *   Set the `Tap Action` to `Call Service`.
    *   Set the `Service` to `script.movie_night_countdown`.
    *   Now you have a one-touch button to start your movie night countdown!

---

## 🎮 Core Entities & Controls

The integration creates a device with a rich set of entities to control every aspect of the clock. You can use these entities in your own automations if you prefer not to use the blueprints.

| Entity Type | Name | Description |
| :--- | :--- | :--- |
| **Select** | `Display Mode` | Sets the main operating mode of the clock. Choose between `Normal Clock`, `Stock Ticker`, `Weather`, or `Data Link`. |
| **Text** | `Data Point 1-5 Marquee` | Sets the scrolling text for one of the five "Data Link" pages. This is the recommended way to display custom marquee text. |
| **Switch** | `Data Point 1-5 Enabled`| Enables or disables one of the five "Data Link" pages. |
| **Text** | `(all 12 segments)` | Provides direct, granular control over every individual display segment (e.g., `Destination Month`, `Present Day`). Useful for hyper-specific automations. |
| **Number** | `Brightness` | Adjusts the brightness of the displays (0-7). |
| **Number** | `Volume` | Adjusts the volume of the speaker (0-21). |
| **Button** | `Engage Time Circuits` | Manually starts the full time travel animation sequence. |
| **Button** | `Reboot Device` | Restarts the clock. |
| **Sensor** | `Status` | Monitors the clock's current state (e.g., `Idle`, `Animating`, `Asleep`) and has useful attributes like `free_heap` and `wifi_rssi`. |
| **Update** | `Firmware` | Notifies you when a new firmware version is available and allows for one-click OTA updates. |
| **Switch** | `Time Circuits On/Off`| A master switch to turn the displays on or off. Note: This is an internal toggle and may be overridden by other automations. |

---

## 🔊 Using the Media Player

The `media_player` entity (e.g., `media_player.time_circuits`) is your central hub for all audio.

### **Playing Sound Effects**
You can play any of the built-in sound effects by calling the `media_player.play_media` service with a `media_content_type` of `music`.

```yaml
# Example: Play an alarm sound in an automation
- service: media_player.play_media
  target:
    entity_id: media_player.time_circuits # Change if you renamed it
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
    entity_id: media_player.time_circuits
  data:
    media_content_id: "http://d.liveatc.net/kcrw_eclectic" # Example Stream URL
    media_content_type: "url"
```

### **Playing Your Favorite Radio Station**
For easy access, the clock allows you to save one "Favorite" radio station in its web interface. This lets you quickly play your go-to station from Home Assistant without needing the full URL each time.

**Step 1: Set Your Favorite Station**
1.  Navigate to the clock's web UI.
2.  Go to the **Sound** tab.
3.  Enter the URL for your desired radio stream in the "Favorite Radio Station URL" field and give it a name.
4.  Click **Save**.

**Step 2: Play from Home Assistant**
To play the saved station, call the `media_player.play_media` service with a special `media_content_type` of `channel` and a `media_content_id` of `Favorite Radio Station`.

```yaml
# Example: Play the saved favorite radio station from an automation or script
- service: media_player.play_media
  target:
    entity_id: media_player.time_circuits # Change if you renamed it
  data:
    media_content_id: "Favorite Radio Station"
    media_content_type: "channel"
```

> ✨ **Pro Tip:** You can create a Script in Home Assistant with this service call and then add that script to your dashboard as a button for one-touch playback of your favorite station!

### **Text-to-Speech (TTS)**
Use your favorite TTS service in Home Assistant to make the clock speak.

```yaml
# Example: Announce when the washer is done
- service: tts.google_en_com
  data:
    entity_id: media_player.time_circuits
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

## ⚙️ Advanced Control (MQTT)

For ultimate control, you can bypass the blueprints and standard entities to publish directly to the clock's raw MQTT topics. This gives you access to the powerful **Command Sequencer**, which allows you to create custom, multi-step animations and trigger pre-programmed cinematic effects.

*   **Topic**: `bttf_time_circuits/DEVICE_ID/cmnd/sequencer` (replace `DEVICE_ID` with your clock's ID)
*   **Payload**: A string with the name of a built-in animation, or a JSON object for a custom sequence.

> **This is an advanced feature.** For a complete guide on the sequencer, including all commands, parameters, a full list of built-in animation names, and examples, please see the **[🤖 Developer Guide](../developer/developer-guide.md#command-sequencer-deep-dive)**.

---

## Troubleshooting

*   **Device Not Appearing in Home Assistant?**
    *   During setup, ensure you entered the correct **Device ID**.
    *   Double-check the MQTT broker IP, port, and credentials in the clock's web UI.
    *   Use a tool like [MQTT Explorer](http://mqtt-explorer.com/) to see if the clock is publishing topics under `bttf_time_circuits/YOUR_DEVICE_ID/`.

*   **Entities are 'Unavailable'?**
    *   Check the clock's Wi-Fi connection.
    *   In MQTT Explorer, check the `bttf_time_circuits/YOUR_DEVICE_ID/status` topic. It should have a retained message of `online`.

*   **Why does my text disappear immediately?**
    This usually happens when using the `SET_TEXT` effect in a blueprint. This command is non-blocking, meaning it executes and immediately moves on. The blueprint's `Duration` input solves this by automatically adding a `WAIT` command. If your text isn't staying visible, ensure you have set a `Duration` greater than zero. Effects like `PULSE`, `MARQUEE`, and `COUNTDOWN` are blocking; they run for their own specific length and do not require an additional `WAIT`.

*   **Can I combine sensor data with my own text?**
    Absolutely! All text fields in the blueprints support Home Assistant templates. This lets you build rich, dynamic strings.

    *Example for the "Display Text" blueprint:*
    ```jinja
    Garage is {{ states('cover.garage_door') }}. Temp: {{ states('sensor.garage_temp') }}°F
    ```