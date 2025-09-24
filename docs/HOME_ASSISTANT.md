# 🏠 Home Assistant Integration Guide

This project includes deep, "headless" integration with Home Assistant using the MQTT protocol. This allows you to control every aspect of the clock and use it as a dynamic notification display for your smart home without ever needing the web interface.

This guide will walk you through setup, features, troubleshooting, and advanced usage.

> ### ⚡ Quick Start
>
> Already familiar with MQTT and Home Assistant? Here's the fast track:
> 1.  **Configure:** Add your MQTT Broker details in the clock's **Data Link** web UI and save the settings.
> 2.  **Discover:** The clock will auto-discover in Home Assistant under the MQTT integration.
> 3.  **Control:** Use the `button.YOUR_CLOCK_ID_trigger_animation` entity to test an animation and the `text.YOUR_CLOCK_ID_dest_year` to set the year.
> 4.  **Automate:** Check out the **[Guide to Blueprints](#guide-to-blueprints)** for the easiest way to create automations.

***

### **Table of Contents**
1. [Prerequisites](#prerequisites)
2. [Setup and Auto-Discovery](#setup-and-auto-discovery)
3. [Available Entities & Features](#available-entities-and-features)
4. [Blueprint Installation & Usage Guide](#blueprint-installation-and-usage-guide)
5. [Guide to Blueprints](#guide-to-blueprints)
6. [Advanced Usage & Guides](#advanced-usage-and-guides)
7. [Frequently Asked Questions (FAQ)](#frequently-asked-questions-faq)
8. [Troubleshooting](#troubleshooting)
9. [Next Steps](#next-steps)

***

## **✅ Prerequisites**

> Before you begin, please ensure you have the following:
> * A running Home Assistant instance.
> * A configured and running MQTT broker that is connected to Home Assistant.
> * The Time Circuits Clock is powered on and connected to your Wi-Fi network.
> * A basic understanding of how MQTT works in Home Assistant.

***

## **Setup and Auto-Discovery**

Setting up the connection is straightforward. Once you enter your MQTT broker's details (IP address, port, and credentials) in the clock's web interface under the **Data Link** tab, the device handles the rest.

It uses **MQTT auto-discovery** to announce itself to your Home Assistant instance. A new device named "**Time Circuits Display**" will automatically appear in your MQTT integration, ready to be used with all its features and controls.

<p align="center">
  <br><em>The Time Circuits device page in Home Assistant, showing all its entities.</em>
</p>

### **A Note on Entity IDs**
To ensure that multiple Time Circuits clocks can coexist on the same network, all entities are created with a unique identifier based on the clock's MAC address. This means your entity IDs will look something like this:

*   `switch.bttf_tc_123456_override_switch`
*   `text.bttf_tc_123456_override_message`

Throughout this document, we will use a placeholder format like `switch.YOUR_CLOCK_ID_override_switch`. **You must replace `YOUR_CLOCK_ID` with the actual ID of your device.**

> **How to Find Your Clock's ID:**
> 1.  In Home Assistant, go to **Settings > Devices & Services > Devices**.
> 2.  Find your "Time Circuits Display" device and click on it.
> 3.  Click on any entity, like the "Status" sensor.
> 4.  The Entity ID will be shown, revealing your clock's unique ID (e.g., `sensor.bttf_tc_123456_status`).

<details>
<summary><strong>How it Works: The MQTT Flow</strong></summary>
This Mermaid diagram visualizes how the clock, the MQTT broker, and Home Assistant communicate.
<pre><code lang="mermaid">
graph TD
    A[Time Circuits Clock] -- Publishes States --> B(MQTT Broker);
    B -- Pushes States --> C[Home Assistant];
    C -- Sends Commands --> B;
    B -- Forwards Commands --> A;
</code></pre>
</details>

***

## **Available Entities & Features**

Entities are grouped by function to make them easy to find.

#### **Monitoring & Sensors**
*   **`sensor.YOUR_CLOCK_ID_status`**: The primary sensor reporting the clock's state (`Idle`, `Animating`, `Asleep`).
    > **💡 Pro Tip:** This sensor has useful diagnostic attributes like `wifi_rssi`, `free_heap`, and `uptime_seconds` that you can view by clicking on the entity in Home Assistant.
*   **`sensor.YOUR_CLOCK_ID_audio_stream_status`**: Shows the state of the audio player (`IDLE` or `PLAYING`). Useful for automations involving TTS or radio streams.
*   **`binary_sensor.YOUR_CLOCK_ID_is_animating`**: `On` when an animation is playing.
*   **`binary_sensor.YOUR_CLOCK_ID_is_asleep`**: `On` when the clock is in sleep mode.

#### **Direct Display Control**
Twelve `text` entities give you direct, granular control over each segment of the three main displays. This is a powerful feature for creating custom information dashboards. You can push any text or template result to these entities.

*   **Destination Display**: `text.YOUR_CLOCK_ID_dest_month`, `text.YOUR_CLOCK_ID_dest_day`, `text.YOUR_CLOCK_ID_dest_year`, `text.YOUR_CLOCK_ID_dest_time`
*   **Present Display**: `text.YOUR_CLOCK_ID_pres_month`, `text.YOUR_CLOCK_ID_pres_day`, `text.YOUR_CLOCK_ID_pres_year`, `text.YOUR_CLOCK_ID_pres_time`
*   **Last Departed Display**: `text.YOUR_CLOCK_ID_last_month`, `text.YOUR_CLOCK_ID_last_day`, `text.YOUR_CLOCK_ID_last_year`, `text.YOUR_CLOCK_ID_last_time`

> **💡 Pro Tip:** Use the **BTTF - Home Assistant Status Display** blueprint to easily control these entities without writing any YAML.

#### **Notifications & Alerts**
These entities are the building blocks for the `Advanced Notifier`, `TTS Notifier`, and other notification-based blueprints. They allow you to temporarily override the main display with a custom message and play a sound.

*   **`switch.YOUR_CLOCK_ID_override_switch`**: A master switch to enable or disable the override mode. When `On`, the `Override Message` is displayed. When `Off`, the clock returns to its normal operation.
*   **`text.YOUR_CLOCK_ID_override_message`**: A text input for the content of your alert. Use `\n` to separate lines for the three displays.
*   **`select.YOUR_CLOCK_ID_play_sound`**: A dropdown to play one of the pre-defined sound effects on command. After a sound is selected, it plays immediately and the entity resets to `None`.

#### **Core Controls**
*   **`select.YOUR_CLOCK_ID_last_departed_preset`**: Choose from movie-based or your custom presets.
*   **`number.YOUR_CLOCK_ID_preset_cycle_interval`**: How often the "Last Time Departed" display cycles through presets (in minutes, 0=off).
*   **`number.YOUR_CLOCK_ID_brightness`**: Controls the display brightness (0-7).
*   **`number.YOUR_CLOCK_ID_volume`**: Adjusts the sound effect volume (0-21).
*   **`switch.YOUR_CLOCK_ID_24h_format`**: Toggles 24-hour time format.
*   **`select.YOUR_CLOCK_ID_profile`**: A powerful feature that applies a pre-configured bundle of settings at once. Profiles include `Standard`, `Cinematic`, `Silent Night`, and `Unstable`.
*   **`time.YOUR_CLOCK_ID_sleep_time`**: Sets the time for the display to turn off automatically.
*   **`time.YOUR_CLOCK_ID_wake_time`**: Sets the time for the display to turn on automatically.

> **Note on Power Control:** This integration does not provide a direct power `switch`. The clock's display is designed to be always on, but you can schedule it to turn off and on at specific times using the `sleep_time` and `wake_time` entities. The `binary_sensor.YOUR_CLOCK_ID_is_asleep` will reflect the display's state.

#### **Animation & Effects**
*   **`button.YOUR_CLOCK_ID_trigger_animation`**: Starts the full time travel sequence.
*   **`select.YOUR_CLOCK_ID_animation_style`**: Choose from a curated list of 10 popular animation styles. For the full list of 20+ styles, please use the web interface.
*   **`number.YOUR_CLOCK_ID_animation_interval`**: How often to auto-play the animation (in minutes, 0=off).
*   **`number.YOUR_CLOCK_ID_animation_duration`**: Sets the length of the time travel effect (in milliseconds).
*   **`switch.YOUR_CLOCK_ID_temporal_echo`**: A fun, experimental feature that creates a "ghosting" effect on the displays.

#### **Marquee & DataLink**
*   **`number.YOUR_CLOCK_ID_datalink_refresh`**: Sets the refresh interval for all API-based DataLink points.
*   **`switch.YOUR_CLOCK_ID_datapoint_0_enabled`**: A switch to enable or disable the marquee in Data Point slot 1. There are 4 others, one for each data point.
*   **`text.YOUR_CLOCK_ID_datapoint_0_marquee`**: A text input for setting the scrolling marquee message in Data Point slot 1. There are 4 others, one for each data point.

#### **Stock Ticker Mode**
The Stock Ticker mode transforms the bottom display row into a scrolling marquee of financial data.

> **Note on Configuration:** The stock ticker's assets and API key are configured in the clock's Web Interface. The following Home Assistant entities are provided for basic control over the feature.

*   **`switch.YOUR_CLOCK_ID_stock_ticker_mode`**: Activates or deactivates the Stock Ticker display mode.
*   **`button.YOUR_CLOCK_ID_stock_next`**: Manually advances to the next page of the current asset.
*   **`button.YOUR_CLOCK_ID_stock_previous`**: Manually goes back to the previous page of the current asset.

#### **Live Weather Mode**
The clock's weather display can also be controlled from Home Assistant. This allows you to toggle the weather display, change the city, and force a refresh directly from your dashboard or automations. The following entities are automatically discovered when you connect the clock to your MQTT broker:

*   **`switch.YOUR_CLOCK_ID_weather_mode`** (Friendly name: `Live Weather Mode`): Toggles the live weather display on or off.
*   **`text.YOUR_CLOCK_ID_weather_city`** (Friendly name: `Weather City`): Sets the city for which to retrieve weather data. After setting a new city, you may need to press the refresh button to see the change.
*   **`button.YOUR_CLOCK_ID_refresh_weather_data`** (Friendly name: `Refresh Weather Data`): Forces an immediate refresh of the weather data for the currently configured city. This is only active after a city has been successfully looked up from the web interface at least once.

#### **System & Actions**
*   **`button.YOUR_CLOCK_ID_reboot_device`**: Restarts the ESP32.
*   **`button.YOUR_CLOCK_ID_force_ntp_sync`**: Manually syncs the clock with time servers.
*   **`button.YOUR_CLOCK_ID_save_all_settings`**: Manually triggers a save of all current settings to the device's memory.
*   **`button.YOUR_CLOCK_ID_factory_reset`**: **Use with caution!** Resets all settings to their original factory defaults.

### **Device Triggers**
The integration also creates several "device triggers" in Home Assistant, which are perfect for starting automations based on the clock's activity.

*   **Animation Started**: Fires when any time travel animation begins.
*   **Animation Completed**: Fires when an animation finishes.
*   **Sleep Mode Entered**: Fires when the clock enters its scheduled sleep mode.
*   **Sleep Mode Exited**: Fires when the clock wakes up.
*   **Preset Changed**: Fires when the "Last Time Departed" display cycles to a new preset.

***

## **Blueprint Installation & Usage Guide**

To make the most powerful features easy to use, this project includes several Home Assistant Blueprints.

1.  **Locate the `blueprints` Folder:** In your Home Assistant configuration directory, find the `config/blueprints/automation` folder. If it doesn't exist, create it. You can typically access this using the Samba or VS Code add-on.
2.  **Copy the Files:** Copy the `.yaml` files from the `home-assistant` directory of this project into that `blueprints/automation` folder.
3.  **Reload Automations:** In Home Assistant, navigate to **Developer Tools > YAML Configuration** and click the "Automations" button under the "YAML configuration reloading" section to load the new blueprints.
4.  **Create an Automation:** Go to **Settings > Automations & Scenes**, click "Create Automation," and you will see the new Time Circuits blueprints listed as options.

***

## **Guide to Blueprints**

Many of the included blueprints are **"callable,"** which means they don't have their own trigger. Instead, you use them as a building block inside your own automations. This makes them incredibly flexible.

A common pattern is:
1.  **Your Automation Trigger:** A sensor changes, a time is reached, etc.
2.  **Your Automation Action:** Call a service, and select "Blueprint" as the action type. Choose one of the BTTF blueprints and fill in its inputs.

### **1. BTTF - Advanced Notifier**
> **Purpose:** To display a temporary, multi-line message on the clock, with an optional sound effect.

* **How to Use:** In your automation's `action` section, choose the "BTTF - Advanced Notifier" blueprint and configure its inputs.
* **Example Scenario: "Mailbox Alert"**
  ```yaml
  trigger:
    - platform: state
      entity_id: binary_sensor.mailbox_sensor
      to: 'on'
  action:
    - blueprint:
        path: home-assistant/bttf_advanced_notifier_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          message: "\nMAIL\n"
          sound_effect: "CONFIRM_ON"
          duration: 60
  ```

### **2. BTTF - Home Assistant Status Display**
> **Purpose:** To use the main displays as a highly customizable, 12-segment status panel for your smart home.

* **How to Use:** This is a "callable" blueprint. In your automation's `action` block, call this blueprint and fill in any of the 12 segment fields with static text or templates.
* **Important Note on Text Length:** The physical displays have character limits for each segment.
    - **Month:** 3 characters (right-justified)
    - **Day:** 2 characters (center-justified)
    - **Year & Time:** 4 characters each
    If you provide text that exceeds these limits, the entire row will automatically begin to scroll the full text.
* **Example Scenario: "Living Room Dashboard"**
  ```yaml
  trigger:
    - platform: state
      entity_id:
        - sensor.living_room_temperature
        - sensor.living_room_humidity
  action:
    - blueprint:
        path: home-assistant/bttf_status_display_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          destination_month: "LIV"
          destination_day: "ING"
          destination_year: "ROOM"
          present_month: "TEMP"
          present_day: "{{ states('sensor.living_room_temperature') }}"
          present_year: "°F"
          last_departed_month: "HUM"
          last_departed_day: "{{ states('sensor.living_room_humidity') }}"
          last_departed_year: "%"
  ```

### **3. BTTF - Dynamic Marquee Display**
> **Purpose:** To display a single, scrolling line of text on one of the five DataLink marquee slots.

* **How to Use:** Call this "callable" blueprint from your automation's `action` block.
* **Example Scenario: "Display Power and Temp"**
  ```yaml
  trigger:
    - platform: state
      entity_id: sensor.home_power_usage
  action:
    - blueprint:
        path: home-assistant/bttf_dynamic_marquee_display_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          data_point_slot: "1"
          text: "PWR {{ states('sensor.home_power_usage') }} W"
  ```

### **4. BTTF - Cinematic Scene Trigger**
> **Purpose:** A simple way to create an automation that sets a destination year and immediately triggers the full time travel animation sequence.

* **How to Use:** This blueprint is self-contained and includes its own trigger, so you can create an automation directly from it without needing to call it from another.
* **Example Scenario: "Lightning Strike at 10:04 PM"**
    1. Go to **Settings > Automations & Scenes** and create a new automation.
    2. Select the "BTTF - Cinematic Scene Trigger" blueprint.
    3. Set the trigger to be a Time trigger at `22:04:00`.
    4. Choose your Time Circuits device and set the **Destination Year** to `1955`.

### 5. BTTF - Radio Streamer
> **Purpose:** To start or stop an internet radio stream on the clock's speaker.

* **How to Use:** This is a "callable" blueprint. From your automation's `action` block, call this blueprint and provide a radio stream URL or the command `stop`.
* **Example Scenario: "Play Radio on Demand"**
  ```yaml
  trigger:
    # Triggered by a helper button on our dashboard
    - platform: state
      entity_id: input_button.play_radio
  action:
    - blueprint:
        path: home-assistant/bttf_radio_streamer_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          radio_command: "http://stream.url/your_station"
  ```

### 6. BTTF - TTS Notifier
> **Purpose:** To play audio announcements from Home Assistant's Text-to-Speech (TTS) services on the clock's speaker.

* **How to Use:** This is a "callable" blueprint. Call it from your automation's `action` block to have the clock speak a message.
* **Example Scenario: "Announce Driveway Alert"**
  ```yaml
  trigger:
    - platform: state
      entity_id: binary_sensor.driveway_motion
      to: 'on'
  action:
    - blueprint:
        path: home-assistant/bttf_tts_notifier_blueprint.yaml
        input:
          target_device: "YOUR_DEVICE_ID_HERE"
          message_text: "Someone is in the driveway."
          display_text: "ALERT\nDRIVE\nWAY"
          volume: 90
  ```

> **💡 Pro Tip:** For more complex visual alerts that change based on sensor states, we recommend calling the **BTTF - Home Assistant Status Display** blueprint right before calling this one to create rich, informative notifications.

***

## **Advanced Usage & Guides**

### **Example Dashboard Configuration**
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

### **Voice Assistant Integration**
Trigger the time travel sequence with a voice command to Google Assistant or Alexa.

1.  **Create a Script:** Go to **Settings > Automations & Scenes > Scripts**. Create a new script named "Activate Time Circuits" and set its action to call the `button.press` service on the `button.YOUR_CLOCK_ID_trigger_animation` entity.
2.  **Expose the Script:** If you use the Home Assistant Cloud (Nabu Casa), expose the new "Activate Time Circuits" script to your voice assistants. If you have a manual setup, add the script to your exposed entities.
3.  **Create a Routine:**
    * **In the Google Home App:** Create a new routine. For the starter, use a voice command like "Activate the time circuits." For the action, choose "Try adding your own" and enter "Activate Time Circuits."
    * **In the Alexa App:** Create a new routine. For "When this happens," choose "Voice" and enter a phrase like "Great Scott." For the action, choose "Smart Home" and select the "Activate Time Circuits" script.
### **Advanced Templating Examples**
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

### **Deep Dive: Creating Custom Audio-Visual Alerts**
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

### **How the Integration Works**
This integration uses Home Assistant's MQTT Auto-Discovery protocol. When the clock connects to your MQTT broker, it publishes a special message to a specific topic that tells Home Assistant how to create and configure all the entities (switches, sensors, etc.).

Once discovered, all communication happens through these standard Home Assistant entities. For example, when you change the "Destination Year" in your dashboard, Home Assistant sends a command to the clock via MQTT. The clock then updates its display and reports the new state back to Home Assistant.

This approach ensures that the state is always synchronized and that you can rely on standard Home Assistant services and UI components to control the clock. For more advanced control, such as audio playback, some blueprints may use direct MQTT communication for features that do not yet have a dedicated entity.

<details>
<summary><strong>Advanced: MQTT Topic Reference</strong></summary>

> For debugging or use in other applications (like Node-RED), you can interact with the clock's MQTT topics directly. The device's command and state topics are under `timecircuits/<UNIQUE_ID>/`. For example, `timecircuits/BTTF_TC_.../destination_year/command`.
>
> The auto-discovery configuration topics are published under `homeassistant/`.
>
> | Topic Suffix (under `timecircuits/<UNIQUE_ID>/`) | Type | Description |
> | :--- | :--- | :--- |
> | `status` | State | Publishes `online` or `offline`. Used for availability. |
> | `status/state` | State | Publishes the clock's current state (e.g., `Idle`, `Animating`). |
> | `destination_year/command`| Command | Send a 4-digit year to set the destination time. |
> | `destination_year/state` | State | Publishes the current destination year. |
> | `...and many more` | | *(Refer to `MqttManager.cpp` for a full list)* |

</details>

***

## **Frequently Asked Questions (FAQ)**

> **Q: Why don't my custom presets appear in the Home Assistant preset selector?**
> **A:** After adding a new preset in the web UI, you must restart Home Assistant for it to be re-discovered and added to the entity's options list. The clock sends its configuration only on connection.

> **Q: Can I control the AM/PM LEDs from Home Assistant?**
> **A:** Not directly. The AM/PM indicators are automatically controlled by the clock's firmware based on the time being displayed and the 12/24 hour format setting.

> **Q: How much traffic does this add to my MQTT broker?**
> **A:** Very little. The device only publishes state changes when they occur (e.g., an animation starts) and a summary of all states every 5 seconds. The traffic is minimal and should not impact your network.

***

## **Troubleshooting**

If you encounter issues, here are some common solutions:

> ⚠️ **Device Not Appearing in Home Assistant?**
> * Double-check the MQTT broker IP, port, and credentials in the clock's web UI.
> * Verify that "Enable discovery" is turned on for your MQTT integration in Home Assistant.
> * Use a tool like [MQTT Explorer](http://mqtt-explorer.com/) to connect to your broker. You should see topics under `homeassistant/` being published by the clock.

> ⚠️ **Entities are 'Unavailable'?**
> * Check the clock's Wi-Fi connection from its web UI or your router.
> * In MQTT Explorer, check the `timecircuits/<UNIQUE_ID>/status` topic (e.g., `timecircuits/BTTF_TC_.../status`). It should have a retained message of `online`. If it says `offline`, the clock has disconnected.

> ⚠️ **Commands Not Working?**
> * Ensure the clock is `online` and the entities are available in Home Assistant.
> * Use MQTT Explorer to listen for commands being sent from Home Assistant. When you toggle a switch, you should see a message published to the corresponding `/command` topic.

***

## **Next Steps**

Now that you've mastered the Home Assistant integration, why not explore more?
* **Get Inspired:** Check out our list of **[30+ Automation Examples](../HOME_ASSISTANT_EXAMPLES.md)** for creative ideas.
* **Dive Deeper:** For those looking to modify the firmware or understand the code, the **[Developer's Guide](../DEVELOPMENT.md)** provides a full technical breakdown.