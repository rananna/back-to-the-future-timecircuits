# 🏠 Home Assistant Integration Guide

This project features a native Home Assistant integration that provides a seamless and powerful way to control your Time Circuits clock.

### **Table of Contents**
1. [Features](#-features)
2. [Prerequisites](#-prerequisites)
3. [Setup & Installation](#-setup--installation)
4. [Using Blueprints](#-using-blueprints)
5. [Core Entities & Controls](#-core-entities--controls)
6. [Using the Media Player](#-using-the-media-player)
7. [Sending Notifications](#-sending-notifications)
8. [Advanced Control via MQTT](#-advanced-control-via-mqtt)
9. [Automation Examples](#-automation-examples)
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

## 🤖 Using Blueprints

Welcome, time traveler! For most automations, the easiest and most powerful way to create custom animations and notifications is with our **Home Assistant Blueprints**. They provide a simple, form-based UI inside Home Assistant, allowing you to build complex effects without writing any code.

Whether you want to see the outside temperature, get a visual alert when the doorbell rings, or run a countdown to movie night, you've come to the right place.

### **Blueprint Installation**

1.  **Copy Blueprints**: Copy the `.yaml` files from the `home_assistant/blueprints/` directory of this project into your Home Assistant's `/config/blueprints/script/` directory. The easiest way is with the Samba Share, FTP, or File Editor add-ons.
2.  **Reload Blueprints**: In the Home Assistant UI, go to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the corner and select **Reload Blueprints**.

### **Creating a Script from a Blueprint**

The blueprints will now appear in your list. Click **Create Script** on the one you want to use. Instead of entering MQTT details, you'll see a single dropdown:

*   **Time Circuits Device**: Simply select your clock from the list.

Fill in the other options (like the text to display) and save it. This script is now ready to be called from your dashboards or, more powerfully, from automations.

### **Core Concepts: Scripts vs. Automations**

To unlock the full potential of your Time Circuits, it's crucial to understand how Home Assistant uses scripts and automations together.

**1. Scripts are "One-Shot" Actions**

Think of a blueprint script as a single, specific mission: "Show this text now." When you run the script, it executes its commands (e.g., display a temperature reading) and then it's done. It doesn't keep running or update automatically.

**2. Automations Provide the Brains**

Automations are the engine of your smart home. They watch for triggers (like a sensor changing, a time of day, or a button press) and then perform actions, such as running one of your blueprint scripts.

> **The Pattern:** You create a **Script** from a blueprint to define *what* you want to happen. Then, you create an **Automation** to decide *when* it should happen.

### **Blueprint Guides**

#### **Display Text**

Your workhorse for displaying any custom message. Perfect for alerts, notifications, and simple status messages.

*   **Inputs:**
    *   `Time Circuits Device`: The clock you want to control.
    *   `Target Row`: Which of the three rows (Top, Middle, Bottom) to use.
    *   `Text`: The message to display. Supports templates!
    *   `Effect`: An optional visual effect (e.g., Pulse, Flash, Marquee).
    *   `Duration`: How long the text should remain visible.
    *   `Restore Row`: If checked, the row returns to its normal state afterward.

#### **Display Entity**

The easiest way to show the state or value of any Home Assistant entity.

*   **Inputs:**
    *   `Time Circuits Device`, `Target Row`, `Effect`, `Duration`, `Restore Row`: Same as the Display Text blueprint.
    *   `Entity`: The sensor or entity you want to display.
    *   `Prefix / Postfix`: Optional text to add before or after the entity's value (e.g., a "°F" postfix for temperature).

#### **Countdown Timer**

Perfect for building anticipation for movie night, a gaming session, or just counting down to dinner time.

*   **Inputs:**
    *   `Time Circuits Device`, `Target Row`: Same as the other blueprints.
    *   `Start Number`: The number to start counting down from.
    *   `End Text`: A message to display when the countdown hits zero.
    *   `Restore Row`: If checked, the row returns to its normal state afterward.

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

## ⚙️ Advanced Control via MQTT

For the most advanced automations, you can bypass the blueprints and standard entities to publish directly to the clock's raw MQTT topics. This gives you access to the powerful **Command Sequencer**, which allows you to create custom animations and trigger pre-programmed cinematic effects.

*   **Topic**: `bttf_time_circuits/DEVICE_ID/sequencer/command` (replace `DEVICE_ID` with your clock's ID)
*   **Payload**: A string with the name of a built-in animation, or a JSON array for a custom sequence.

> **For a complete guide on the sequencer, including all commands, parameters, a full list of built-in animation names, and examples, please see the [🤖 Developer Guide](../developer/developer-guide.md#command-sequencer-deep-dive).**


#### **Example: Triggering a Named Sequence via MQTT**
This automation triggers the **Intruder Alert** sequence. It uses a template to dynamically find the correct MQTT topic for your device.

```yaml
# In this example, 'switch.time_circuits' is an entity belonging to the target device.
# Replace it with any entity from your Time Circuits device.
- service: mqtt.publish
  data:
    topic: "bttf_time_circuits/{{ device_id('switch.time_circuits') }}/sequencer/command"
    payload: "Intruder Alert"
```

---

## 💡 Automation Examples

Here are a few ideas to get you started.

<details>
<summary><strong>1. "It's 10:04 PM!" - The Lightning Strike</strong></summary>
This automation triggers the "Lightning" sequence every night at 10:04 PM, just like in the movie.

```yaml
alias: "BTTF - Lightning Strike at 10:04 PM"
trigger:
  - platform: time
    at: "22:04:00"
action:
  - service: mqtt.publish
    data:
      topic: "bttf_time_circuits/{{ device_id('switch.time_circuits') }}/sequencer/command"
      payload: "Lightning"
```
</details>

<details>
<summary><strong>2. "Now Playing" Marquee</strong></summary>
This automation displays the currently playing song from a media player on the bottom display row using the standard text entities.

```yaml
alias: "BTTF - Now Playing Marquee"
trigger:
  - platform: template
    value_template: "{{ state_attr('media_player.spotify', 'media_title') }}"
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_data_point_1_marquee
    data:
      value: "♪ {{ state_attr('media_player.spotify', 'media_title') }}"
  - service: switch.turn_on
    target:
      entity_id: switch.time_circuits_data_point_1_enabled
```
</details>

<details>
<summary><strong>3. Low Memory Reboot Warning</strong></summary>
This automation monitors the clock's free memory and, if it gets too low, displays a warning using the `notify` service and then safely reboots the device.

```yaml
alias: "BTTF - Low Memory Reboot"
trigger:
  - platform: numeric_state
    entity_id: sensor.time_circuits_status
    attribute: free_heap
    below: 20000  # 20 KB
action:
  - service: notify.bttf_time_circuits
    data:
      message: "REBOOTING\nLOW MEMORY\nSTAND BY"
      data:
        duration: 10
        sound_effect: "REBOOT_SOUND.mp3"
  - delay: "00:00:10"
  - service: button.press
    target:
      entity_id: button.time_circuits_reboot_device
```
</details>

<details>
<summary><strong>4. Dynamic Weather Alert</strong></summary>
This automation uses a template to show a dynamic weather alert on the top row if it starts raining. It combines a static message with the current temperature. This example uses the **Display Text Blueprint**.

First, create a script using the `display_text.yaml` blueprint. Select your device and set your desired options (e.g., Target Row, Effect, Duration). Save the script (e.g., as `Display Weather Alert`). Then, create the following automation:

```yaml
alias: "BTTF - Weather Alert on Rain"
trigger:
  - platform: state
    entity_id: weather.home
    to: "rainy"
action:
  - service: script.display_weather_alert # Your script's name
    data:
      text_to_display: "RAIN {{ states('sensor.outside_temperature') }}°F"
```
</details>

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