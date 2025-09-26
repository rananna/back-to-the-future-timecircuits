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

The device will now use **MQTT auto-discovery** to announce itself to your Home Assistant instance. A new device named "**Time Circuits**" will automatically appear in your MQTT integration.

### **Step 3: Install the Blueprints**
Blueprints are the easiest way to create powerful, callable scripts.
1.  In your Home Assistant configuration directory, find the `config/blueprints/script` folder. If it doesn't exist, create it.
2.  Copy the `.yaml` files from the `home-assistant` directory of this project into that `blueprints/script` folder.
3.  Reload your scripts in Home Assistant by navigating to **Developer Tools > YAML Configuration** and clicking the "Scripts" button.

---

## Guide to Using Blueprints

Once installed, the Time Circuits blueprints will be available when you create a new script (**Settings > Automations & Scenes > Scripts**).

Many of the included blueprints create **"callable" scripts,** meaning they are on-demand actions that you call from your own automations. This provides maximum flexibility. A common pattern is:
1.  **Your Automation's Trigger:** A sensor changes, a specific time is reached, etc.
2.  **Your Automation's Action:** Call the script created from the blueprint.

Below is a detailed guide to each blueprint.

---

### BTTF - Advanced Notifier
Displays a temporary, multi-line message on the clock with an optional sound. Perfect for alerts like "Mailbox" or "Door Open."

#### **Inputs**
*   **Time Circuits**: Select the clock device.
*   **Message**: The text to display. Use `\n` for new lines (e.g., `LINE 1\nLINE 2\nLINE 3`).
*   **Display Duration (seconds)**: How long the message should be displayed.
*   **Sound Effect**: (Optional) Select a sound to play with the notification.

#### **Example Usage**
Here is an example of an automation that calls the script to show a "MAILBOX" notification when a binary sensor is triggered.
```yaml
trigger:
  - platform: state
    entity_id: binary_sensor.mailbox_sensor
    to: 'on'
action:
  - service: script.bttf_advanced_notifier # Or whatever you named the script you created from the blueprint
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
    - service: script.bttf_cinematic_scene_trigger # Or whatever you named your script
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
  - service: script.bttf_dynamic_marquee_display # Or whatever you named your script
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
  - service: script.bttf_home_assistant_status_display # Or whatever you named your script
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
*   **Time Circuits**: Select the clock device.
*   **Radio Command**: The URL of the live radio stream, or the command `stop` to end the stream.

#### **Example Usage**
Create a script to start your favorite 80s radio station.
```yaml
alias: Play 80s Radio
sequence:
  - service: script.bttf_radio_streamer # Or whatever you named your script
    data:
      radio_command: "http://d.liveatc.net/kcrw_eclectic" # Example Stream URL
mode: single
```

---

### BTTF - Sequencer
A powerful tool for creating custom, multi-step animations. You can flash specific display segments, play sounds, and show temporary messages in a coordinated sequence.

> **NOTE**: This is an advanced blueprint that requires crafting a JSON payload and uses direct MQTT communication with the device.

#### **Inputs**
*   **Time Circuits**: Select the clock device.
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
*   **Time Circuits**: Select the clock device.
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
  - service: script.bttf_tts_notifier # Or whatever you named your script
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

# Advanced Usage & Automation Recipes

This guide provides advanced usage examples, automation recipes, and deep dives into the more complex features of the Home Assistant integration.

### **Table of Contents**
1. [Example Dashboard Configuration](#example-dashboard-configuration)
2. [Voice Assistant Integration](#voice-assistant-integration)
3. [Advanced Templating Examples](#advanced-templating-examples)
4. [Deep Dive: Creating Custom Audio-Visual Alerts](#deep-dive-creating-custom-audio-visual-alerts)
5. [How the Integration Works](#how-the-integration-works)

---

## Example Dashboard Configuration
You can create a beautiful and functional control panel for your clock on a Home Assistant dashboard. Go to a dashboard, click the three dots > "Edit Dashboard" > "+" > "Manual" and paste the following YAML:

<details>
<summary><strong>Click to view Dashboard YAML Code</strong></summary>
<pre><code lang="yaml">
type: vertical-stack
cards:
  - type: entities
    title: Time Circuits Status
    entities:
      - entity: sensor.YOUR_CLOCK_ID_status
        name: Current State
      - entity: text.YOUR_CLOCK_ID_dest_time
      - entity: text.YOUR_CLOCK_ID_pres_time
      - entity: text.YOUR_CLOCK_ID_last_time
  - type: grid
    columns: 2
    cards:
      - type: button
        tap_action:
          action: call-service
          service: button.press
          target:
            entity_id: button.YOUR_CLOCK_ID_trigger_animation
        name: Trigger Animation
        icon: mdi:movie-play
      - type: button
        tap_action:
          action: call-service
          service: button.press
          target:
            entity_id: button.YOUR_CLOCK_ID_reboot_device
        name: Reboot Clock
        icon: mdi:restart
  - type: entities
    title: Core Controls
    entities:
      - entity: text.YOUR_CLOCK_ID_dest_year
      - entity: select.YOUR_CLOCK_ID_last_departed_preset
      - entity: number.YOUR_CLOCK_ID_brightness
      - entity: number.YOUR_CLOCK_ID_volume
</code></pre>
</details>

---

## Voice Assistant Integration
Trigger the time travel sequence with a voice command to Google Assistant or Alexa.

1.  **Create a Script:** Go to **Settings > Automations & Scenes > Scripts**. Create a new script named "Activate Time Circuits" and set its action to call the `button.press` service on the `button.YOUR_CLOCK_ID_trigger_animation` entity.
2.  **Expose the Script:** If you use the Home Assistant Cloud (Nabu Casa), expose the new "Activate Time Circuits" script to your voice assistants. If you have a manual setup, add the script to your exposed entities.
3.  **Create a Routine:**
    * **In the Google Home App:** Create a new routine. For the starter, use a voice command like "Activate the time circuits." For the action, choose "Try adding your own" and enter "Activate Time Circuits."
    * **In the Alexa App:** Create a new routine. For "When this happens," choose "Voice" and enter a phrase like "Great Scott." For the action, choose "Smart Home" and select the "Activate Time Circuits" script.

---

## Advanced Templating Examples
The "BTTF - Dynamic Marquee Display" blueprint can be made even more powerful with templates. Here are some examples for the `Display Text` field:

> **Combining Multiple Sensors:**
> ```jinja
> IN {{ states('sensor.living_room_temperature') }}° OUT {{ states('sensor.outside_temperature') }}°
> ```

> **Conditional Messages:**
> ```jinja
> {% if is_state('binary_sensor.washing_machine_running', 'on') %} LAUNDRY {% else %} IDLE {% endif %}
> ```

> **Formatting Numbers and Timestamps:**
> ```jinja
> BUS IN {{ (as_timestamp(states.sensor.next_bus.state) - as_timestamp(now())) | timestamp_custom('%M') }} MIN
> ```

---

## Deep Dive: Creating Custom Audio-Visual Alerts
While the **BTTF - Advanced Notifier** blueprint is the easiest way to create temporary alerts, you can build them manually in your own automations using the dedicated notification entities. This gives you maximum flexibility.

A typical alert sequence involves three service calls:
1.  **Set the Message:** Use the `text.set_value` service to put your desired message (using `\n` for new lines) into the `text.YOUR_CLOCK_ID_override_message` entity.
2.  **Activate the Display:** Use the `switch.turn_on` service to enable the `switch.YOUR_CLOCK_ID_override_switch`. This tells the clock to show your message.
3.  **Play a Sound (Optional):** Use the `select.select_option` service to choose a sound from the `select.YOUR_CLOCK_ID_play_sound` entity.

The display will remain in override mode until you call the `switch.turn_off` service on the `switch.YOUR_CLOCK_ID_override_switch`.

**Example Automation Action:**
```yaml
action:
  # 1. Set the text for the three rows
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_override_message
    data:
      value: "WARNING\nSECURITY ALERT\nGARAGE DOOR"

  # 2. Activate the override display
  - service: switch.turn_on
    target:
      entity_id: switch.YOUR_CLOCK_ID_override_switch

  # 3. Play an alert sound
  - service: select.select_option
    target:
      entity_id: select.YOUR_CLOCK_ID_play_sound
    data:
      option: ALARM_SOUND

  # 4. Wait 15 seconds
  - delay:
      seconds: 15

  # 5. Turn off the override display to return to normal
  - service: switch.turn_off
    target:
      entity_id: switch.YOUR_CLOCK_ID_override_switch
```

---

## How the Integration Works
This integration uses Home Assistant's MQTT Auto-Discovery protocol. When the clock connects to your MQTT broker, it publishes a special message to a specific topic that tells Home Assistant how to create and configure all the entities (switches, sensors, etc.).

Once discovered, all communication happens through these standard Home Assistant entities. For example, when you change the "Destination Year" in your dashboard, Home Assistant sends a command to the clock via MQTT. The clock then updates its display and reports the new state back to Home Assistant.

This approach ensures that the state is always synchronized and that you can rely on standard Home Assistant services and UI components to control the clock. For more advanced control, such as audio playback, some blueprints may use direct MQTT communication for features that do not yet have a dedicated entity.

<details>
<summary><strong>Advanced: MQTT Topic Reference</strong></summary>

> For debugging or use in other applications (like Node-RED), you can interact with the clock's MQTT topics directly. The device's command and state topics are under `BTTF_TC/<UNIQUE_ID>/`. For example, `BTTF_TC/BTTF_TC_.../destination_year/command`.
>
> The auto-discovery configuration topics are published under `homeassistant/`.
>
> | Topic Suffix (under `BTTF_TC/<UNIQUE_ID>/`) | Type | Description |
> | :--- | :--- | :--- |
> | `status` | State | Publishes `online` or `offline`. Used for availability. |
> | `status/state` | State | Publishes the clock's current state (e.g., `Idle`, `Animating`). |
> | `destination_year/command`| Command | Send a 4-digit year to set the destination time. |
> | `destination_year/state` | State | Publishes the current destination year. |
> | `...and many more` | | *(Refer to `MqttManager.cpp` for a full list)* |

</details>

# Back to the Future Time Circuits: Home Assistant Automations

This document provides a list of useful and creative automations to demonstrate how you can integrate the Time Circuits display into your smart home.

> **Important Note on Entity IDs:** In the examples below, you will see placeholders like `text.YOUR_CLOCK_ID_dest_year`. You must replace `YOUR_CLOCK_ID` with the unique ID of your clock device. For instructions on how to find this ID, please refer to the main **[Home Assistant Integration Guide](docs/HOME_ASSISTANT.md)**.

---

### Blueprint-Powered Examples

The easiest way to create powerful actions is by using the provided script blueprints. You can then call the scripts you create from your own automations.

<details>
<summary><strong>1. "Weather Station" Display</strong></summary>

*Use the "BTTF - Home Assistant Status Display" blueprint to turn your clock into a real-time weather dashboard.*

**How It Works:**
This automation is triggered whenever your primary weather sensors change. It then calls the `home_assistant_status_display` blueprint to update the display segments with the latest data.

```yaml
# automation.yaml
- alias: "BTTF - Weather Display"
  trigger:
    - platform: state
      entity_id:
        - sensor.outside_temperature
        - sensor.outside_feels_like_temperature
        - sensor.weather_conditions
  action:
    - blueprint:
        path: bttf_status_display_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          destination_month: "OUT"
          destination_day: "SIDE"
          destination_year: "TEMP"
          destination_time: "{{ states('sensor.outside_temperature') }}°"
          present_month: "FEELS"
          present_day: "LIKE"
          present_year: "{{ states('sensor.outside_feels_like_temperature') }}°"
          last_departed_time: "{{ states('sensor.weather_conditions') }}"
```
</details>

<details>
<summary><strong>2. "Now Playing" Marquee</strong></summary>

*Use the "BTTF - Dynamic Marquee Display" blueprint to show the currently playing song on your favorite media player.*

**How It Works:**
This automation triggers whenever the `media_title` of your media player changes. It then calls the `dynamic_marquee_display` blueprint to send the song title to the specified marquee slot.

```yaml
# automation.yaml
- alias: "BTTF - Now Playing Marquee"
  trigger:
    - platform: template
      value_template: "{{ state_attr('media_player.spotify', 'media_title') }}"
  action:
    - blueprint:
        path: bttf_dynamic_marquee_display_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          data_point_slot: "5"
          text: "♪ {{ state_attr('media_player.spotify', 'media_title') }}"
```
> **Note:** The marquee text has a generous 255-character limit, perfect for song titles or detailed notifications.
</details>

<details>
<summary><strong>3. Advanced Notification Example</strong></summary>

*Use the "BTTF - Advanced Notifier" blueprint to show a critical alert with a sound effect.*

**How It Works:**
This automation triggers when a critical event occurs (like a water leak). It then calls the `advanced_notifier` blueprint to display a prominent, temporary message and play an alarm sound.

```yaml
# automation.yaml
- alias: "BTTF - Water Leak Alert"
  trigger:
    - platform: state
      entity_id: binary_sensor.water_leak_detector
      to: "on"
  action:
    - blueprint:
        path: bttf_advanced_notifier_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          message: "WATER LEAK\nDETECTED\nCHECK BASEMENT"
          duration: 300 # Show for 5 minutes
          sound_effect: "ALARM_SOUND"
```
</details>

---
Here are 30 useful and well-thought-out automations to demonstrate how you can integrate the Time Circuits display into your smart home.

---

### Cinematic & Fun Automations

<details>
<summary><strong>1. "It's 10:04 PM!" - The Lightning Strike</strong></summary>

*Triggers the iconic lightning strike scene every night at 10:04 PM. This is a perfect use for the "BTTF - Cinematic Scene Trigger" blueprint.*

**Automation Setup:**
1.  Create a new script from the "BTTF - Cinematic Scene Trigger" blueprint in the UI. Set the **Destination Year** to `1985` and give it a name (e.g., "BTTF Cinematic Scene 1985").
2.  Create a new, separate automation.
3.  For the **Trigger**, select "Time" and enter `22:04:00`.
4.  For the **Action**, select "Call service" and choose the script you just created (e.g., `script.bttf_cinematic_scene_1985`).

</details>

<details>
<summary><strong>2. "Roads? Where We're Going, We Don't Need Roads."</strong></summary>

*Sets the destination to the future and plays the animation when you start your vacuum cleaner. This is another great use for the "BTTF - Cinematic Scene Trigger" blueprint.*

**Automation Setup:**
1.  Create a new script from the "BTTF - Cinematic Scene Trigger" blueprint. Set the **Destination Year** to `2015`.
2.  Create a new automation.
3.  For the **Trigger**, select "State", use `vacuum.roomba` as the entity, and set the "To" state to `cleaning`.
4.  For the **Action**, call the script you just created.
</details>

<details>
<summary><strong>3. "Sync the Clocks" - Multi-Device Animation</strong></summary>

*When a time travel animation starts, flash your smart lights to match the effect.*

```yaml
alias: "BTTF - Sync the Clocks"
trigger:
  - platform: device
    device_id: YOUR_DEVICE_ID_HERE
    domain: BTTF_TC
    type: animation_started
action:
  - service: light.turn_on
    target:
      entity_id: light.living_room_lights
    data:
      effect: "flash"
```
</details>

<details>
<summary><strong>4. Birthday Time Jump</strong></summary>

*On your birthday, automatically set the destination year to the year you were born and play the animation. The "Cinematic Scene Trigger" blueprint makes this easy.*

**Automation Setup:**
1.  Create a script from the "Cinematic Scene Trigger" blueprint, setting the **Destination Year** to your birth year.
2.  Create an automation that triggers on your birthday using a Template trigger: `{{ now().month == 10 and now().day == 26 }}` (replace with your birthday).
3.  Set the action to call the script you created.
</details>

---

### Daily Routines & Practical Uses

<details>
<summary><strong>5. Good Morning, Hill Valley</strong></summary>

*When you dismiss your morning alarm, the clock wakes from sleep mode and displays the current weather.*

```yaml
alias: "BTTF - Good Morning"
trigger:
  - platform: state
    entity_id: input_boolean.morning_alarm_dismissed
    to: "on"
action:
  - service: button.press
    target:
      entity_id: button.YOUR_CLOCK_ID_force_ntp_sync
  - service: switch.turn_on
    target:
      entity_id: switch.YOUR_CLOCK_ID_weather_mode
```
</details>

<details>
<summary><strong>6. "Welcome to the Future" - Arriving Home</strong></summary>

*When the first person arrives home, set the destination to the current year.*

```yaml
alias: "BTTF - Welcome Home"
trigger:
  - platform: state
    entity_id: group.family
    to: "home"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_dest_year
    data:
      value: "{{ now().year }}"
```
</details>

<details>
<summary><strong>7. Movie Night Ambiance</strong></summary>

*When you start a movie, dim the clock's brightness and turn off any distracting marquee messages.*

```yaml
alias: "BTTF - Movie Night"
trigger:
  - platform: state
    entity_id: media_player.living_room_tv
    to: "playing"
action:
  - service: number.set_value
    target:
      entity_id: number.YOUR_CLOCK_ID_brightness
    data:
      value: "1"
  - service: switch.turn_off
    target:
      entity_id: switch.YOUR_CLOCK_ID_datapoint_0_enabled
```
</details>

<details>
<summary><strong>8. "Your Future is Whatever You Make of It" - Bedtime</strong></summary>

*When you activate your "Goodnight" scene, the clock will follow its automatic sleep schedule. To set the sleep time, use the `time.YOUR_CLOCK_ID_sleep_time` entity in Home Assistant.*
</details>

---

### Notifications & Alerts

<details>
<summary><strong>9. "The Libyans!" - Security Alert</strong></summary>

*If a door or window is opened while the security system is armed, flash a warning message on the display. This is a perfect use for the "BTTF - Advanced Notifier" blueprint.*

**Automation Setup:**
1.  First, create a script from the **BTTF - Advanced Notifier** blueprint. Give it a name, e.g., "BTTF Security Alert".
2.  Create a second automation that triggers when a security sensor is tripped.
3.  This second automation then calls the script, passing the specific message and sound.

```yaml
# automation.yaml
- alias: "BTTF - Security Alert Trigger"
  trigger:
    - platform: state
      entity_id: binary_sensor.front_door_contact
      to: 'on'
  condition:
    - condition: state
      entity_id: alarm_control_panel.home_alarm
      state: armed_away
  action:
    # Call the script created from the blueprint
    - service: script.bttf_security_alert # Or whatever you named your script
      data:
        message: "SECURITY\nALERT\nFRONT DOOR"
        duration: 30
        sound_effect: "ALARM_SOUND"
```
</details>

<details>
<summary><strong>10. Severe Weather Warning</strong></summary>

*If a severe weather alert is active, override the display to show the warning. Use the "BTTF - Advanced Notifier" blueprint for a simple setup.*

**Automation Setup:**
1.  Create a script from the **BTTF - Advanced Notifier** blueprint.
2.  Create a second automation that triggers when the `binary_sensor.severe_weather_alert` turns on.
3.  This automation will call your script and display the alert.

```yaml
# automation.yaml
- alias: "BTTF - Severe Weather Alert Trigger"
  trigger:
    - platform: state
      entity_id: binary_sensor.severe_weather_alert
      to: 'on'
  action:
    - service: script.bttf_advanced_notifier # Or whatever you named your script
      data:
        message: "SEVERE\nWEATHER\n{{ states('sensor.weather_alert_type') }}"
        duration: 600 # 10 minutes
        sound_effect: "ALARM_SOUND"
```
</details>

<details>
<summary><strong>11. Garbage Day Reminder</strong></summary>

*The night before garbage day, display a persistent reminder on the marquee.*

```yaml
alias: "BTTF - Garbage Day Reminder"
trigger:
  - platform: time
    at: "20:00:00"
condition:
  - condition: state
    entity_id: sensor.garbage_day
    state: "Tomorrow"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_2_marquee
    data:
      value: "TRASH NIGHT"
```
</details>

<details>
<summary><strong>12. "Mr. Fusion" - Low Battery Alert</strong></summary>

*If your phone's battery is low, display a reminder to charge it.*

```yaml
alias: "BTTF - Low Battery"
trigger:
  - platform: numeric_state
    entity_id: sensor.phone_battery_level
    below: 20
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_3_marquee
    data:
      value: "CHARGE PHONE"
```
</details>

<details>
<summary><strong>13. Guest Welcome Message</strong></summary>

*When a new device joins your guest WiFi network, display a welcome message.*

```yaml
alias: "BTTF - Guest Welcome"
trigger:
  - platform: event
    event_type: "device_tracker_new_device"
condition:
  - condition: template
    value_template: "{{ trigger.event.data.host_name is defined }}"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_0_marquee
    data:
      value: "WELCOME {{ trigger.event.data.host_name }}"
```
</details>

---

### Dynamic Data Display

<details>
<summary><strong>14. Stock Ticker</strong></summary>

*Display the current price of a stock on the marquee.*

```yaml
alias: "BTTF - Stock Ticker"
trigger:
  - platform: time_pattern
    minutes: "/15"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_0_marquee
    data:
      value: "AAPL ${{ states('sensor.aapl_stock_price') }}"
```
</details>

<details>
<summary><strong>15. "The Sports Almanac" - Live Game Score</strong></summary>

*Show the score of your favorite team's game while it's being played.*

```yaml
alias: "BTTF - Game Score"
trigger:
  - platform: state
    entity_id: sensor.favorite_team_score
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_1_marquee
    data:
      value: "SCORE {{ states('sensor.favorite_team_score') }}"
```
</details>

<details>
<summary><strong>16. YouTube Subscriber Count</strong></summary>

*Display your YouTube subscriber count and update it periodically.*

```yaml
alias: "BTTF - YouTube Subscribers"
trigger:
  - platform: time_pattern
    hours: "/1"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_2_marquee
    data:
      value: "SUBS {{ states('sensor.youtube_subscriber_count') }}"
```
</details>

<details>
<summary><strong>17. "How Many People Are in Space Right Now?"</strong></summary>

*Display the current number of astronauts in space.*

```yaml
alias: "BTTF - People in Space"
trigger:
  - platform: time_pattern
    hours: "/6"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_3_marquee
    data:
      value: "SPACE {{ states('sensor.people_in_space') }}"
```
</details>

<details>
<summary><strong>18. Network Status</strong></summary>

*Show your internet download speed on the marquee.*

```yaml
alias: "BTTF - Network Speed"
trigger:
  - platform: time_pattern
    minutes: "/5"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_4_marquee
    data:
      value: "NET {{ states('sensor.speedtest_download') }} Mbps"
```
</details>

---

### Advanced & Creative Scripts

<details>
<summary><strong>19. "Save the Clock Tower!" - Countdown Script</strong></summary>

*A script to create a 10-second countdown on the display, ending with a time travel animation.*

```yaml
alias: "BTTF - Countdown Script"
sequence:
  - service: switch.turn_on
    target:
      entity_id: switch.YOUR_CLOCK_ID_override_switch
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_override_message
    data:
      value: "COUNTDOWN\n10"
  - delay: "00:00:01"
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_override_message
    data:
      value: "COUNTDOWN\n09"
  # ... (repeat for each number down to 01)
  - delay: "00:00:01"
  - service: switch.turn_off
    target:
      entity_id: switch.YOUR_CLOCK_ID_override_switch
  - service: button.press
    target:
      entity_id: button.YOUR_CLOCK_ID_trigger_animation
```
</details>

<details>
<summary><strong>20. "Doc, You're My Only Hope" - NFC Tag Message</strong></summary>

*Tap an NFC tag with your phone to send a pre-set message to the display.*

```yaml
alias: "BTTF - NFC Message"
trigger:
  - platform: tag
    tag_id: "YOUR_NFC_TAG_ID_HERE"
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_0_marquee
    data:
      value: "HELP ME DOC"
```
</details>

<details>
<summary><strong>21. Change Animation Style Based on Time of Day</strong></summary>

*Use a more subtle animation in the evening and a more energetic one during the day.*

```yaml
alias: "BTTF - Dynamic Animation Style"
trigger:
  - platform: sun
    event: sunset
  - platform: sun
    event: sunrise
action:
  - service: select.select_option
    target:
      entity_id: select.YOUR_CLOCK_ID_animation_style
    data:
      option: >
        {% if trigger.platform == 'sun' and trigger.event == 'sunset' %}
          Wave Flicker
        {% else %}
          Tornado Flicker
        {% endif %}
```
</details>

<details>
<summary><strong>22. "Are You Telling Me You Built a Time Machine... Out of a DeLorean?"</strong></summary>

*When your car enters the garage, trigger a welcome animation.*

```yaml
alias: "BTTF - Car Arrival"
trigger:
  - platform: state
    entity_id: binary_sensor.garage_car_presence
    to: "on"
action:
  - service: button.press
    target:
      entity_id: button.YOUR_CLOCK_ID_trigger_animation
```
</details>

<details>
<summary><strong>23. "Don't Drive 88!" - Speeding Alert</strong></summary>

*If your connected car is going over 85 mph, display a warning on the clock.*

```yaml
alias: "BTTF - Speeding Alert"
trigger:
  - platform: numeric_state
    entity_id: sensor.car_speed
    above: 85
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_0_marquee
    data:
      value: "SLOW DOWN!"
```
</details>

<details>
<summary><strong>24. Low Memory Reboot</strong></summary>

*Monitors the clock's free memory and reboots it if it drops to a critical level.*

**Automation Setup:**
1.  First, create a script from the **BTTF - Advanced Notifier** blueprint. This will be our reboot warning. Let's assume you've named it `bttf_advanced_notifier`.
2.  Then, create a second automation that triggers when the device's `free_heap` attribute drops below a threshold.
3.  This automation will call the notifier script, wait 10 seconds for the message to be seen, and then press the device's reboot button.

```yaml
# automation.yaml
- alias: "BTTF - Low Memory Reboot Trigger"
  trigger:
    - platform: numeric_state
      entity_id: sensor.YOUR_CLOCK_ID_status
      attribute: free_heap
      below: 20000  # 20 KB
  action:
    # 1. Call the notifier script to show a warning
    - service: script.bttf_advanced_notifier # Or whatever you named your script
      data:
        message: "REBOOTING\nLOW MEMORY\nSTAND BY"
        duration: 10
        sound_effect: "REBOOT_SOUND"
    # 2. Wait for the message to be visible
    - delay: "00:00:10"
    # 3. Press the actual reboot button on the device
    - service: button.press
      target:
        entity_id: button.YOUR_CLOCK_ID_reboot_device
```
</details>

<details>
<summary><strong>25. Calendar-Driven Destination Time</strong></summary>

*Automatically sets the "Destination Time" to the date of your next calendar event.*

```yaml
alias: "BTTF - Next Calendar Event"
trigger:
  - platform: state
    entity_id: calendar.your_calendar
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_dest_year
    data:
      value: "{{ state_attr('calendar.your_calendar', 'start_time').split(' ')[0].split('-')[0] }}"
```
</details>

<details>
<summary><strong>26. "Save the Clock Tower!" - Fundraising Goal Tracker</strong></summary>

*Display the progress of a fundraising or savings goal on the marquee.*

```yaml
alias: "BTTF - Savings Goal Tracker"
trigger:
  - platform: state
    entity_id: input_number.savings_goal_current
action:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_3_marquee
    data:
      value: >
        GOAL ${{ states('input_number.savings_goal_current') }} / ${{ states('input_number.savings_goal_target') }}
```
</details>

<details>
<summary><strong>27. "Doc's Notes" - Rotating Reminders Script</strong></summary>

*A script to cycle through a list of reminders or quotes on the marquee.*

```yaml
alias: "BTTF - Rotating Reminders Script"
sequence:
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_4_marquee
    data:
      value: "REMINDER: WATER THE PLANTS"
  - delay: "00:01:00"
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_4_marquee
    data:
      value: "QUOTE: THE FUTURE IS WHATEVER YOU MAKE OF IT"
  - delay: "00:01:00"
  - service: text.set_value
    target:
      entity_id: text.YOUR_CLOCK_ID_datapoint_4_marquee
    data:
      value: "TASK: TAKE OUT THE RECYCLING"
mode: restart
```
</details>