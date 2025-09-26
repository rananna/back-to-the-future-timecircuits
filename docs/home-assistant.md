# 🏠 Home Assistant Integration Guide

This project features a native Home Assistant integration that provides a seamless and powerful way to control your Time Circuits clock. This new integration replaces the previous blueprint-based setup, offering a more streamlined experience with richer features.

### **Table of Contents**
1. [✨ Features](#-features)
2. [🚀 Setup & Installation](#-setup--installation)
3. [🎮 Core Entities & Controls](#-core-entities--controls)
4. [🔊 Using the Media Player](#-using-the-media-player)
5. [🔔 Sending Notifications](#-sending-notifications)
6. [⚙️ Advanced Services](#️-advanced-services)
   - [Custom Status Display](#custom-status-display)
   - [Custom Sequences](#custom-sequences)
7. [🔄 Firmware Updates](#-firmware-updates)
8. [💡 Automation Examples](#-automation-examples)
9. [troubleshooting](#troubleshooting)

---

## ✨ Features

The new custom component provides a rich, native Home Assistant experience:

*   **Simple Setup**: Add the integration directly from the Home Assistant UI. No more YAML!
*   **Unified Audio Control**: A single `media_player` entity for playing sound effects, streaming radio, and using Text-to-Speech (TTS).
*   **Native Notifications**: A built-in `notify` service to easily send alerts and messages to the clock's display.
*   **Seamless OTA Updates**: An `update` entity that tells you when new firmware is available and lets you install it with one click.
*   **Intuitive Controls**: Refactored display modes into simple `switch` entities.
*   **Powerful Services**: New services to replicate the functionality of the most advanced blueprints, like creating custom status displays and running complex animation sequences.

---

## 🚀 Setup & Installation

### **Step 1: Prerequisites**
> * A running Home Assistant instance.
> * A configured and running MQTT broker that is connected to Home Assistant.
> * The Time Circuits Clock is powered on and connected to your Wi-Fi network.
> * In the clock's web UI, ensure the MQTT broker details are correctly configured under the **Data Link** tab.
> * [HACS](https://hacs.xyz/) (Home Assistant Community Store) installed.

### **Step 2: Install the Custom Component**

There are two ways to install the custom component: via HACS (recommended) or manually.

#### **Method 1: Install via HACS (Recommended)**
1.  In Home Assistant, navigate to **HACS > Integrations**.
2.  Click the three dots in the top-right corner and select **Custom repositories**.
3.  In the "Repository" field, paste the URL to this GitHub repository: `https://github.com/rananna/back-to-the-future-timecircuits`
4.  Select **Integration** for the category and click **ADD**.
5.  Close the "Custom repositories" window.
6.  The "Back to the Future Time Circuits" integration will now appear in your HACS list. Click on it and then click **INSTALL**.
7.  Follow the on-screen instructions to complete the installation.
8.  Restart Home Assistant as prompted.

#### **Method 2: Manual Installation**
1.  Copy the `custom_components/bttf_time_circuits` directory from this project into your Home Assistant `config/` directory.
2.  Restart Home Assistant.

### **Step 3: Add the Integration**
1.  Navigate to **Settings > Devices & Services**.
2.  Click **Add Integration** and search for "**Back to the Future Time Circuits**".
3.  Follow the on-screen instructions. The integration will be added without any further configuration needed.

Your Time Circuits clock will now appear as a new device in Home Assistant, with all its entities automatically created.

---

## 🎮 Core Entities & Controls

The integration creates a device with a rich set of entities to control every aspect of the clock.

*   **Display Mode Switches**: `switch.stock_ticker_mode`, `switch.weather_mode`, and `switch.data_link_mode` allow you to easily change the clock's primary function. Turning one on will automatically turn the others off.
*   **Direct Text Input**: 22 `text` entities give you granular control over every display segment and marquee.
*   **Core Controls**: `number` entities for `brightness` and `volume`, and `button` entities for `trigger_animation` and `reboot_device`.
*   **Sensors**: A `sensor.status` entity to monitor the clock's state (`Idle`, `Animating`) and an `sensor.audio_stream_status` for the speaker.

---

## 🔊 Using the Media Player

The `media_player.bttf_time_circuits_speaker` entity is your central hub for all audio.

### **Playing Sound Effects**
You can play any of the built-in sound effects using the `media_player.select_sound` service.

```yaml
# Example: Play an alarm sound
- service: media_player.select_sound
  target:
    entity_id: media_player.bttf_time_circuits_speaker
  data:
    sound: "ALARM_SOUND"
```

### **Playing Radio Streams**
Use the `media_player.play_media` service with a `media_content_type` of `url`.

```yaml
# Example: Play an 80s radio station
- service: media_player.play_media
  target:
    entity_id: media_player.bttf_time_circuits_speaker
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
    entity_id: media_player.bttf_time_circuits_speaker
    message: "Great Scott! The washing machine is finished."
```

---

## 🔔 Sending Notifications

The native `notify.bttf_time_circuits` service makes sending alerts simple.

### **Service Data**
*   **`message`**: The text to display. Use `\n` for new lines (e.g., `LINE 1\nLINE 2`).
*   **`data.duration`**: (Optional) How long the message should be displayed in seconds (default: 10).
*   **`data.sound_effect`**: (Optional) The name of a sound effect to play (e.g., `REMINDER_ALERT`).

### **Example Automation**
Show a "MAILBOX" notification for 60 seconds with a sound.

```yaml
trigger:
  - platform: state
    entity_id: binary_sensor.mailbox_sensor
    to: 'on'
action:
  - service: notify.bttf_time_circuits
    data:
      message: "\nMAILBOX"
      data:
        duration: 60
        sound_effect: "REMINDER_ALERT"
```

---

## ⚙️ Advanced Services

For maximum power and flexibility, two custom services are provided to replicate the most advanced blueprint functionality.

### **Custom Status Display**
The `bttf_time_circuits.set_status_display` service lets you use the clock as a 12-segment status panel for your smart home.

```yaml
# Example: Show temperatures and humidity
trigger:
  - platform: time_pattern
    seconds: '/59'
action:
  - service: bttf_time_circuits.set_status_display
    data:
      destination_month: "OUT"
      destination_day: "{{ states('sensor.outside_temperature') | round(0) }}°"
      present_month: "IN"
      present_day: "{{ states('sensor.living_room_temperature') | round(0) }}°"
      last_departed_month: "HUMID"
      last_departed_day: "{{ states('sensor.living_room_humidity') | round(0) }}%"
```

### **Custom Sequences**
The `bttf_time_circuits.run_sequence` service allows you to create complex, multi-step animations by sending a JSON payload directly to the device's sequencer.

#### **Example Script**
Flash the "Destination Year" display, play an alarm, and show "INTRUDER ALERT".

```yaml
alias: Intruder Alert Sequence
sequence:
  - service: bttf_time_circuits.run_sequence
    data:
      sequence: >
        [
          { "command": "flash", "segment": "dest_year" },
          { "command": "sound", "effect": "ALARM_SOUND" },
          { "command": "message", "display": "destination", "month": "INTRUDER", "day": "ALERT", "year": "!!", "time": "" },
          { "command": "delay", "duration": 5000 },
          { "command": "message", "display": "destination", "month": "", "day": "", "year": "", "time": "" }
        ]
mode: single
```

---

## 🔄 Firmware Updates

The `update.bttf_time_circuits_firmware` entity makes updates effortless.
*   Home Assistant will automatically notify you when a new firmware version is released.
*   The entity will show the latest version number and a link to the release notes.
*   Simply click the **INSTALL** button on the entity to trigger the Over-the-Air (OTA) update. The device will update and reboot automatically.

---

## 💡 Automation Examples

Here are a few ideas to get you started with the new integration.

<details>
<summary><strong>1. "It's 10:04 PM!" - The Lightning Strike</strong></summary>

```yaml
alias: "BTTF - Lightning Strike at 10:04 PM"
trigger:
  - platform: time
    at: "22:04:00"
action:
  - service: button.press
    target:
      entity_id: button.bttf_time_circuits_trigger_animation
```
</details>

<details>
<summary><strong>2. "Now Playing" Marquee</strong></summary>

```yaml
alias: "BTTF - Now Playing Marquee"
trigger:
  - platform: template
    value_template: "{{ state_attr('media_player.spotify', 'media_title') }}"
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_time_circuits_data_point_1_marquee
    data:
      value: "♪ {{ state_attr('media_player.spotify', 'media_title') }}"
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_time_circuits_data_point_1_enabled
```
</details>

<details>
<summary><strong>3. Low Memory Reboot Warning</strong></summary>

```yaml
alias: "BTTF - Low Memory Reboot"
trigger:
  - platform: numeric_state
    entity_id: sensor.bttf_time_circuits_status
    attribute: free_heap
    below: 20000  # 20 KB
action:
  - service: notify.bttf_time_circuits
    data:
      message: "REBOOTING\nLOW MEMORY\nSTAND BY"
      data:
        duration: 10
        sound_effect: "REBOOT_SOUND"
  - delay: "00:00:10"
  - service: button.press
    target:
      entity_id: button.bttf_time_circuits_reboot_device
```
</details>

---

## Troubleshooting

> ⚠️ **Device Not Appearing in Home Assistant?**
> * Double-check the MQTT broker IP, port, and credentials in the clock's web UI.
> * Verify that "Enable discovery" is turned on for your MQTT integration in Home Assistant.
> * Use a tool like [MQTT Explorer](http://mqtt-explorer.com/) to see if the clock is publishing topics under `homeassistant/`.

> ⚠️ **Entities are 'Unavailable'?**
> * Check the clock's Wi-Fi connection.
> * In MQTT Explorer, check the `BTTF_TC/<UNIQUE_ID>/status` topic. It should have a retained message of `online`.