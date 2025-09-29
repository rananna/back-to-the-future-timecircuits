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

## 🛑 Prerequisites

* A running Home Assistant instance.
* A configured and running MQTT broker that is connected to Home Assistant.
* The Time Circuits Clock is powered on and connected to your Wi-Fi network.
* In the clock's web UI, ensure the MQTT broker details are correctly configured under the **Data Link** tab.
* [HACS](https://hacs.xyz/) (Home Assistant Community Store) installed.

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

### **Step 1: Install the Custom Component**

There are two ways to install the custom component: via HACS (recommended) or manually.

> ***Image Placeholder:*** *A screenshot of the HACS "Custom repositories" dialog with the repository URL and "Integration" category selected.*

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

### **Step 2: Add the Integration**
1.  Navigate to **Settings > Devices & Services**.
2.  Click **Add Integration** and search for "**Back to the Future Time Circuits**".
3.  Follow the on-screen instructions. The integration will be added without any further configuration needed.

Your Time Circuits clock will now appear as a new device in Home Assistant, with all its entities automatically created.

> ***Image Placeholder:*** *A screenshot of the Home Assistant device page for the Time Circuits clock, showing the collection of entities (switches, numbers, buttons, etc.).*

---

## 🎮 Entities & Controls

The integration creates a device with a rich set of entities to control every aspect of the clock.

| Entity Type | Name | Description |
| :--- | :--- | :--- |
| **Switch** | `Stock Ticker Mode` | Enables the real-time stock ticker on the bottom display row. |
| **Switch** | `Weather Mode` | Enables the live weather forecast on the bottom display row. |
| **Switch** | `Data Link Mode` | Enables the custom marquee on the bottom display row. |
| **Text** | `(all 22 text inputs)` | Provides direct control over every display segment and marquee for custom messages. |
| **Number** | `Brightness` | Adjusts the brightness of the displays (0-15). |
| **Number** | `Volume` | Adjusts the volume of the speaker (0-100). |
| **Button** | `Trigger Animation` | Manually starts the full time travel animation sequence. |
| **Button** | `Reboot Device` | Restarts the clock. |
| **Sensor** | `Status` | Monitors the clock's current state (e.g., `Idle`, `Animating`, `Rebooting`). |
| **Sensor** | `Audio Status` | Monitors the speaker's state (e.g., `Idle`, `Playing`, `Streaming`). |
| **Update** | `Firmware` | Notifies you when a new firmware version is available and allows for one-click OTA updates. |

---

## 🔊 Using the Media Player

The `media_player.bttf_time_circuits_speaker` entity is your central hub for all audio.

### **Playing Sound Effects**

You can play any of the built-in sound effects directly from the media player entity.

1.  Navigate to the `media_player.bttf_time_circuits_speaker` entity in your Home Assistant dashboard.
2.  Click the three dots to open the media controls.
3.  Select a sound from the **Source** dropdown menu.

> ***Image Placeholder:*** *A screenshot of the media player entity's controls in Home Assistant, with the "Source" dropdown expanded to show the list of available sound effects.*

You can also use the `media_player.select_source` service in scripts or automations:

```yaml
# Example: Play an alarm sound in an automation
- service: media_player.select_source
  target:
    entity_id: media_player.bttf_time_circuits_speaker
  data:
    source: "ALARM_SOUND"
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

## ⚙️ Advanced Services

The integration provides a rich set of services for advanced control and automation.

### **Notification Service**
This service sends a temporary notification message to the clock's display. It is the only service that uses the `notify` domain.

> ***Image Placeholder:*** *A screenshot of the Home Assistant Developer Tools > Services view, showing a call to the `notify.bttf_time_circuits` service with example message and data.*

#### **`notify.bttf_time_circuits`**
| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`message`** | `string` | Yes | The text to display. Use `\n` to separate lines (e.g., `LINE 1\nLINE 2`). |
| **`data.duration`** | `integer`| No | How long the message should be displayed, in seconds. (Default: 10) |
| **`data.sound_effect`**| `string` | No | The name of a sound effect to play from the device's library (e.g., `REMINDER_ALERT`). |

**Example:** Show a "MAILBOX" notification for 60 seconds with a sound.
```yaml
- service: notify.bttf_time_circuits
  data:
    message: "\nMAILBOX"
    data:
      duration: 60
      sound_effect: "REMINDER_ALERT"
```

### **Display & Sequence Services**
These services, part of the `bttf_time_circuits` domain, offer direct control over the display content and animation sequences.

#### **`bttf_time_circuits.set_status_display`**
This service turns the clock into a 12-segment status panel, allowing you to map individual sensor values to specific display segments.

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`(all 12 segments)`** | `string` | No | The value to display in a specific segment. Accepts any of the 12 segment names (e.g., `destination_month`, `present_day`, `last_departed_year`). |

**Example:** Create a live weather and temperature dashboard.
```yaml
- service: bttf_time_circuits.set_status_display
  data:
    destination_month: "OUT"
    destination_day: "{{ states('sensor.outside_temperature') | round(0) }}°"
    present_month: "IN"
    present_day: "{{ states('sensor.living_room_temperature') | round(0) }}°"
    last_departed_month: "HUMID"
    last_departed_day: "{{ states('sensor.living_room_humidity') | round(0) }}%"
```

#### **`bttf_time_circuits.run_sequence`**
This service offers advanced control, allowing you to run a multi-step script of animations and sounds directly on the device.

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`sequence`** | `list` | Yes | A list of command dictionaries for the sequencer. See the table below for available commands. |
| **`target_row`**| `integer`| No | The display row to run the sequence on (0=Destination, 1=Present, 2=Last Departed). Defaults to `2`. |

#### **Available Commands**

| Command | Parameter | Description | Example |
| :--- | :--- | :--- | :--- |
| **`sound`** | `effect` | Plays a sound effect from the device's library. | `{ "command": "sound", "effect": "ALARM_SOUND" }` |
| **`delay`** | `duration` | Pauses the sequence for a specified time in milliseconds. | `{ "command": "delay", "duration": 2000 }` |
| **`flash`** | `segment`, `duration` | Flashes a specific display segment for a duration. | `{ "command": "flash", "segment": "destination_year", "duration": 500 }` |
| **`pulse`** | `segment`, `duration` | Pulses a segment's brightness for a duration. | `{ "command": "pulse", "segment": "present_day", "duration": 3000 }` |
| **`fade_in`**| `duration` | Fades in the entire display row over a duration. | `{ "command": "fade_in", "duration": 1500 }` |
| **`fade_out`**| `duration` | Fades out the entire display row over a duration. | `{ "command": "fade_out", "duration": 1500 }` |
| **`marquee`**| `text` | Scrolls a message across the display row. | `{ "command": "marquee", "text": "GREAT SCOTT!" }` |

> ⚠️ **Note on `message`:** The `message` command is not supported in sequences. To set static text on the displays, use the `text.set_value` service for individual segments or the `bttf_time_circuits.set_status_display` service to update multiple segments at once.

#### **Pre-defined Sequences**
In addition to building sequences from scratch, you can trigger a number of pre-defined, named sequences by passing the sequence name directly to the `sequence` parameter. These sequences are also available in the device's web UI dropdown. They are designed to be visually striking and make use of the full range of the sequencer's capabilities.

| Sequence Name | Description |
| :--- | :--- |
| **Intruder Alert** | A classic alert sequence. The top display scrolls "INTRUDER ALERT," the middle display scrambles "BREACH DETECTED," and the bottom display scrolls "LOCKDOWN INITIATED," all while pulsing and accompanied by an electrical spark sound. |
| **Time Travel** | The signature time travel sequence. The top display shows an "ACCELERATING" bar graph, the middle display announces "TIME TRAVEL ACTIVATED" followed by "88 MPH," and the bottom display flashes intensely for 8 seconds, all set to the iconic time travel sound. |
| **Party Mode** | Turns the clock into a party machine. All three rows display pulsing, dancing text and marquees like "PARTY TIME!", "DANCE", and "WOOHOO". |
| **Countdown** | A 10-second countdown timer is displayed on the middle row, ending with a "LIFTOFF!" message and an engine revving sound. Great for dramatic entrances. |
| **Knight Rider** | A smooth, red scanning light moves back and forth across the bottom display, just like KITT's scanner. |
| **Cylon** | A red scanning light, similar to Knight Rider but with a wider trail, moves across the middle display, mimicking a Cylon from Battlestar Galactica. |
| **Rainbow** | The middle display cycles through the colors of the rainbow, showing the name of each color. |
| **Lightning** | Simulates a lightning storm. All displays flicker randomly and flash brightly, accompanied by the sound of electric sparks. |
| **Loading** | A progress bar fills up on the middle display with the text "LOADING...". |
| **Error** | A system malfunction sequence. The top display scrambles the word "ERROR," the middle display scrolls "SYSTEM MALFUNCTION," and error beeps play. |
| **Flux Capacitor Charge-Up** | The bottom display shows a "CHARGING" bar graph that fills up, accompanied by the flux capacitor sound. It culminates in a bright flash across the top two displays. |
| **Tachyons Detected** | Simulates temporal interference. The middle display scrambles to reveal the message "TACHYONS DETECTED" while a mysterious hum plays. |
| **Data Stream** | A "Matrix"-style digital rain effect, with random characters flickering down all three display rows. |
| **Wormhole Collapse** | A chaotic sequence where all displays flicker violently and then fade to black one by one, accompanied by the sound of electrical sparks, simulating a collapsing wormhole. |

#### **Example: Triggering a Named Sequence**
This example triggers the **Intruder Alert** sequence.
```yaml
- service: bttf_time_circuits.run_sequence
  data:
    sequence: "Intruder Alert"
```

### **Time Control Services**
These services allow for direct, programmatic control over the three main time displays.

#### **`bttf_time_circuits.set_destination_time`**
Sets the top "Destination Time" display to a specific date and time.

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`datetime`** | `datetime` | Yes | The full date and time to display. Can be a `datetime` object or a string (e.g., `"2015-10-21 07:28:00"`). |

**Example:** Set the destination to the date of the first moon landing.
```yaml
- service: bttf_time_circuits.set_destination_time
  data:
    datetime: "1969-07-20 20:17:00"
```

#### **`bttf_time_circuits.set_present_time`**
Sets the middle "Present Time" display to a specific date and time. Note that this overrides the automatic NTP-synced time until the next reboot.

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`datetime`** | `datetime` | Yes | The full date and time to display. |

#### **`bttf_time_circuits.set_last_departed_time`**
Sets the bottom "Last Time Departed" display to a specific date and time.

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`datetime`** | `datetime` | Yes | The full date and time to display. |

#### **`bttf_time_circuits.time_travel`**
Executes a full time travel sequence. It reads the current "Present Time", sets it as the new "Last Time Departed", updates the "Destination Time" to the specified `datetime`, and then triggers the main time travel animation.

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| **`datetime`** | `datetime` | Yes | The target destination date and time for the sequence. |

**Example:** Create an automation to time travel to next Christmas morning.
```yaml
- service: bttf_time_circuits.time_travel
  data:
    datetime: "{{ (now().year ~ '-12-25 08:00:00') | as_datetime | as_local }}"
```

### **Media Player Services**
These services extend the functionality of the `media_player` entity.

#### **`bttf_time_circuits.favorite_radio_station`**
Saves the currently playing radio stream to the device's list of favorite stations. This service takes no parameters.

#### **`bttf_time_circuits.clear_favorite_radio_stations`**
Clears all saved favorite radio stations from the device's memory. This service takes no parameters.

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