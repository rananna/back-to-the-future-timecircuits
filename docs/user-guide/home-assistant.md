# 🏠 Home Assistant Integration Guide

This project features a native Home Assistant integration that provides a seamless and powerful way to control your Time Circuits clock.

### **Table of Contents**
1. [Features](#-features)
2. [Prerequisites](#-prerequisites)
3. [Setup & Installation](#-setup--installation)
4. [Core Entities & Controls](#-core-entities--controls)
5. [Using the Media Player](#-using-the-media-player)
6. [Sending Notifications](#-sending-notifications)
7. [Advanced Control via MQTT](#-advanced-control-via-mqtt)
8. [Automation Examples](#-automation-examples)
9. [Troubleshooting](#troubleshooting)

---
## ✨ Features

The custom component provides a rich, native Home Assistant experience:

*   **Simple Setup**: Add the integration directly from the Home Assistant UI. No more YAML for setup!
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
2.  Click the three dots in the top-right corner and select **Custom repositories**.
3.  In the "Repository" field, paste the URL to this GitHub repository: `https://github.com/rananna/back-to-the-future-timecircuits`
4.  Select **Integration** for the category and click **ADD**.
5.  Close the "Custom repositories" window.
6.  The "Back to the Future Time Circuits" integration will now appear in your HACS list. Click on it and then click **INSTALL**.
7.  Restart Home Assistant as prompted.

### **Step 2: Add the Integration in Home Assistant**
1.  Navigate to **Settings > Devices & Services**.
2.  Click **Add Integration** and search for "**Back to the Future Time Circuits**".
3.  Follow the on-screen instructions. The integration will automatically discover your clock on the network via its MQTT messages.

Your Time Circuits clock will now appear as a new device in Home Assistant, with all its entities automatically created.

---

## 🎮 Core Entities & Controls

The integration creates a device with a rich set of entities to control every aspect of the clock. A few key entities are highlighted below.

| Entity Type | Name | Description |
| :--- | :--- | :--- |
| **Switch** | `Override Switch` | Enables the full-display override mode for custom messages. |
| **Text** | `Override Line 1-3` | Sets the text for the three display rows when the override switch is on. |
| **Select** | `Display Mode` | Sets the main operating mode of the clock (`Normal Clock`, `Stock Ticker`, `Weather`, `Data Link`). |
| **Text** | `(all 12 segments)` | Provides direct control over every display segment (e.g., `Destination Month`, `Present Day`). |
| **Number** | `Brightness` | Adjusts the brightness of the displays (0-7). |
| **Number** | `Volume` | Adjusts the volume of the speaker (0-21). |
| **Button** | `Trigger Animation` | Manually starts the full time travel animation sequence. |
| **Button** | `Reboot Device` | Restarts the clock. |
| **Sensor** | `Status` | Monitors the clock's current state (e.g., `Idle`, `Animating`, `Asleep`) and has useful attributes like `free_heap` and `wifi_rssi`. |
| **Update** | `Firmware` | Notifies you when a new firmware version is available and allows for one-click OTA updates. |

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

## ⚙️ Advanced Control via MQTT

For the most advanced automations, you can bypass the standard entities and publish directly to the clock's raw MQTT topics. This gives you access to the powerful **Command Sequencer**.

*   **Topic**: `bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload**: A string with the name of a built-in animation, or a JSON array for a custom sequence.

> **For a complete guide on the sequencer, including all commands, parameters, and examples, please see the [🤖 Command Sequencer API Reference](./sequencer-api.md).**

#### **Available Built-in Animations**
The following named animations can be triggered with a simple string payload.

| Sequence Name | Description |
| :--- | :--- |
| `Time Circuits Lock-In` | The classic BTTF effect. All three rows simultaneously scramble and lock in the current time, character by character. |
| `Lightning` | A chaotic, multi-stage lightning storm effect with loud crackling sounds and intense, random flashes across all displays. |
| `Scanner` | A Cylon-style red scanner (`---`) that sweeps back and forth across all three display rows in unison. |
| `Time Travel Tunnel` | Simulates traveling through a time vortex by repeatedly scrolling the current time in from the right on all three rows. |
| `Flux Capacitor Overload` | All displays pulse with intense energy, simulating a Flux Capacitor overload with a synchronized, slow pulsing effect. |
| `Fire Trails` | "Burns" the current time onto the display with a fiery `WIPE` effect that reveals the text from left to right on all three rows. |
| `Sparkle Reveal` | A subtle reveal where the time appears out of a field of sparkling lights. The display flickers with random dots before wiping to reveal the current time. |
| `Countdown` | Displays "COUNTDOWN" on the middle row, then shows a 10-second countdown (spelling out the numbers), ending with a "LIFTOFF!" marquee. |
| `System Error` | A system malfunction theme. The top row scrambles to show "ERROR" while the middle row scrolls "SYSTEM MALFUNCTION". |

#### **Example: Triggering a Named Sequence via MQTT**
This automation triggers the **Intruder Alert** sequence.
```yaml
- service: mqtt.publish
  data:
    topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command"
    payload: "Intruder Alert"
```

---

## 💡 Automation Examples

Here are a few ideas to get you started with the new integration.

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
      topic: "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command"
      payload: "Lightning"
```
</details>

<details>
<summary><strong>2. "Now Playing" Marquee</strong></summary>
This automation displays the currently playing song from a media player on the bottom display row.

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
This automation monitors the clock's free memory and, if it gets too low, displays a warning and then safely reboots the device.

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
        sound_effect: "REBOOT_SOUND.mp3"
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
> * In MQTT Explorer, check the `bttf-time-circuits/<UNIQUE_ID>/status` topic. It should have a retained message of `online`.