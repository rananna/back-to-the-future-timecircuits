# 🏠 Home Assistant Integration Guide

This project includes deep, "headless" integration with Home Assistant using the MQTT protocol. This allows you to control every aspect of the clock and use it as a dynamic notification display for your smart home.

This guide will walk you through setup, using blueprints, and finding more advanced information.

### **Table of Contents**
1. [Getting Started: Setup & Blueprints](#getting-started-setup--blueprints)
2. [Guide to Using Blueprints](#guide-to-using-blueprints)
   - [Advanced Notifier](#bttf---advanced-notifier)
   - [Cinematic Scene Trigger](#bttf---cinematic-scene-trigger)
   - [Dynamic Marquee Display](#bttf---dynamic-marquee-display)
   - [Home Assistant Status Display](#bttf---home-assistant-status-display)
   - [Radio Streamer](#bttf---radio-streamer)
   - [Sequencer](#bttf---sequencer)
   - [TTS Notifier](#bttf---tts-notifier)
3. [Troubleshooting](#troubleshooting)
4. [Where to Go Next](#where-to-go-next)

---

## Getting Started: Setup & Blueprints

### **Step 1: Prerequisites**
> Before you begin, please ensure you have the following:
> * A running Home Assistant instance.
> * A configured and running MQTT broker that is connected to Home Assistant.
> * The Time Circuits Clock is powered on and connected to your Wi-Fi network.

### **Step 2: Connect the Clock to MQTT**
Setting up the connection is straightforward.
1.  Open the clock's web interface and navigate to the **Data Link** tab.
2.  Enter your MQTT broker's details (IP address, port, and credentials).
3.  Press the **"Engage Time Circuits"** button to save the settings.

The device will now use **MQTT auto-discovery** to announce itself to your Home Assistant instance. A new device named "**Time Circuits Display**" will automatically appear in your MQTT integration.

### **Step 3: Install the Blueprints**
Blueprints are the easiest way to create powerful automations.
1.  In your Home Assistant configuration directory, find the `config/blueprints/automation` folder. If it doesn't exist, create it.
2.  Copy the `.yaml` files from the `home-assistant` directory of this project into that `blueprints/automation` folder.
3.  Reload your automations in Home Assistant by navigating to **Developer Tools > YAML Configuration** and clicking the "Automations" button.

---

## Guide to Using Blueprints

Once installed, the Time Circuits blueprints will be available when you create a new automation (**Settings > Automations & Scenes**).

Many of the included blueprints are **"callable,"** meaning they are on-demand actions that you call from your own automations. This provides maximum flexibility. A common pattern is:
1.  **Your Automation's Trigger:** A sensor changes, a specific time is reached, etc.
2.  **Your Automation's Action:** Call the desired Time Circuits blueprint.

Below is a detailed guide to each blueprint.

---

### BTTF - Advanced Notifier
Displays a temporary, multi-line message on the clock with an optional sound. Perfect for alerts like "Mailbox" or "Door Open."

#### **Inputs**
*   **Time Circuits Display**: Select the clock device.
*   **Message**: The text to display. Use `\n` for new lines (e.g., `LINE 1\nLINE 2\nLINE 3`).
*   **Display Duration (seconds)**: How long the message should be displayed.
*   **Sound Effect**: (Optional) Select a sound to play with the notification.

#### **Example Usage**
Here is an example of an automation that shows a "MAILBOX" notification when a binary sensor is triggered.
```yaml
trigger:
  - platform: state
    entity_id: binary_sensor.mailbox_sensor
    to: 'on'
action:
  - service: automation.trigger
    target:
      entity_id: automation.bttf_advanced_notifier # Or whatever you named your blueprint automation
    data:
      message: "\nMAILBOX"
      duration: 60
      sound_effect: "REMINDER_ALERT"
```

---

### BTTF - Cinematic Scene Trigger
A simple way to trigger the full, cinematic time travel animation for a specific destination year.

#### **Inputs**
*   **Time Circuits Device**: Select the clock device.
*   **Destination Year**: The four-digit year to travel to.

#### **Example Usage**
This blueprint is perfect for scenes. For example, you could create a "Movie Time" scene that dims the lights, turns on the TV, and sends the clock to 1955.
```yaml
- id: 'movie_time_scene'
  name: 'Movie Time'
  actions:
    - service: automation.trigger
      target:
        entity_id: automation.bttf_cinematic_scene_trigger
      data:
        destination_year: "1955"
    # ... other scene actions
```

---

### BTTF - Dynamic Marquee Display
Shows a scrolling line of text on one of the five data link display slots. It supports Home Assistant's templating engine.

#### **Inputs**
*   **Time Circuits Device**: Select the clock device.
*   **Data Point Slot**: Which of the five marquee slots to use (1-5).
*   **Marquee Text**: The text to display. Supports templates. Max 255 characters.

#### **Example Usage**
Display the current outside temperature, updating every 5 minutes.
```yaml
trigger:
  - platform: time_pattern
    minutes: '/5'
action:
  - service: automation.trigger
    target:
      entity_id: automation.bttf_dynamic_marquee_display
    data:
      data_point_slot: 1
      text: "Outside temp is {{ states('sensor.outside_temperature') }}°C"
```

---

### BTTF - Home Assistant Status Display
Use the main displays as a highly customizable, 12-segment status panel for your smart home. Show temperatures, humidity, or any other sensor value.

#### **Inputs**
*   **Time Circuits Device**: Select the clock device.
*   **12x Segment Inputs**: One input for each of the 12 display segments (e.g., Destination Month, Present Day, etc.). Accepts static text or templates. Any field left blank will be ignored.

#### **Example Usage**
Create an automation that runs every minute to show various sensor data on the clock.
```yaml
trigger:
  - platform: time_pattern
    seconds: '/59'
action:
  - service: automation.trigger
    target:
      entity_id: automation.bttf_home_assistant_status_display
    data:
      destination_month: "OUT"
      destination_day: "{{ states('sensor.outside_temperature') | round(0) }}°"
      present_month: "IN"
      present_day: "{{ states('sensor.living_room_temperature') | round(0) }}°"
      last_departed_month: "HUMID"
      last_departed_day: "{{ states('sensor.living_room_humidity') | round(0) }}%"
```

---

### BTTF - Radio Streamer
Starts or stops an internet radio stream on the clock's speaker.

#### **Inputs**
*   **Time Circuits Display**: Select the clock device.
*   **Radio Command**: The URL of the live radio stream, or the command `stop` to end the stream.

#### **Example Usage**
Create a script to start your favorite 80s radio station.
```yaml
alias: Play 80s Radio
sequence:
  - service: automation.trigger
    target:
      entity_id: automation.bttf_radio_streamer
    data:
      radio_command: "http://d.liveatc.net/kcrw_eclectic" # Example Stream URL
mode: single
```

---

### BTTF - Sequencer
A powerful tool for creating custom, multi-step animations. You can flash specific display segments, play sounds, and show temporary messages in a coordinated sequence.

> **NOTE**: This is an advanced blueprint that requires crafting a JSON payload and uses direct MQTT communication with the device.

#### **Inputs**
*   **Time Circuits Display**: Select the clock device.
*   **Sequence Payload**: A JSON array of command objects. See the blueprint's description for the full list of commands and their parameters.

#### **Example Usage**
Create a script that flashes the "Destination Year" display, plays an alarm, and shows "INTRUDER ALERT" when a door sensor is triggered.
```json
[
  { "command": "flash", "segment": "dest_year" },
  { "command": "sound", "effect": "ALARM_SOUND" },
  { "command": "message", "display": "destination", "month": "INTRUDER", "day": "ALERT", "year": "!!", "time": "" },
  { "command": "delay", "duration": 5000 },
  { "command": "message", "display": "destination", "month": "", "day": "", "year": "", "time": "" }
]
```

---

### BTTF - TTS Notifier
Play audio announcements from Home Assistant's Text-to-Speech (TTS) services on the clock's speaker.

#### **Inputs**
*   **Time Circuits Display**: Select the clock device.
*   **TTS Service**: The TTS service to use (e.g., `tts.google_en_com`).
*   **Message Text**: The text you want the clock to say.
*   **Playback Volume**: The volume for the TTS message (0-100).
*   **Display Text (Optional)**: A message to show on the clock's display during playback.

#### **Example Usage**
Announce when the washer is finished and display a message on the clock.
```yaml
trigger:
  - platform: state
    entity_id: sensor.washing_machine_status
    to: 'finished'
action:
  - service: automation.trigger
    target:
      entity_id: automation.bttf_tts_notifier
    data:
      message_text: "The washer is finished."
      display_text: "\nWASHER\nDONE"
      volume: 90
```

---

## Troubleshooting

If you encounter issues, here are some common solutions:

> ⚠️ **Device Not Appearing in Home Assistant?**
> * Double-check the MQTT broker IP, port, and credentials in the clock's web UI.
> * Verify that "Enable discovery" is turned on for your MQTT integration in Home Assistant.
> * Use a tool like [MQTT Explorer](http.mqtt-explorer.com/) to see if the clock is publishing topics under `homeassistant/`.

> ⚠️ **Entities are 'Unavailable'?**
> * Check the clock's Wi-Fi connection.
> * In MQTT Explorer, check the `BTTF_TC/<UNIQUE_ID>/status` topic. It should have a retained message of `online`.

---

## Where to Go Next

*   **[Home Assistant Advanced Guide](guides/HOME_ASSISTANT_ADVANCED.md)**: For advanced automation examples, dashboard configurations, and deep dives into the integration's features.
*   **[MQTT Entity Reference](reference/MQTT_ENTITIES.md)**: A complete list of all available Home Assistant entities and device triggers for the clock.
*   **[30+ Automation Examples](../HOME_ASSISTANT_EXAMPLES.md)**: Get inspired with a huge list of creative ideas.
*   **[Developer's Guide](../DEVELOPMENT.md)**: For a full technical breakdown of the firmware and MQTT implementation.