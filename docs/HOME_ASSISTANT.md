# 🏠 Home Assistant Integration Guide

This project includes deep, "headless" integration with Home Assistant using the MQTT protocol. This allows you to control every aspect of the clock and use it as a dynamic notification display for your smart home without ever needing the web interface.

This guide will walk you through setup, features, troubleshooting, and advanced usage.

> ### ⚡ Quick Start
>
> Already familiar with MQTT and Home Assistant? Here's the fast track:
> 1.  **Configure:** Add your MQTT Broker details in the clock's **Data Link** web UI and save the settings.
> 2.  **Discover:** The clock will auto-discover in Home Assistant under the MQTT integration.
> 3.  **Control:** Use the `button.time_circuits_display_trigger_animation` entity to test an animation and the `number.time_circuits_display_destination_year` to set the year.
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
  <img src="../images/ha_screenshot.png" alt="Home Assistant Screenshot" width="800">
  <br><em>The Time Circuits device page in Home Assistant, showing all its entities.</em>
</p>

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
*   **`sensor.time_circuits_display_status`**: The primary sensor reporting the clock's state (`Idle`, `Animating`, `Asleep`).
    > **💡 Pro Tip:** This sensor has useful diagnostic attributes like `wifi_rssi`, `free_heap`, and `uptime_seconds` that you can view by clicking on the entity in Home Assistant.
*   **`sensor.time_circuits_display_audio_stream_status`**: Shows the state of the audio player (`IDLE` or `PLAYING`). Useful for automations involving TTS or radio streams.
*   **`sensor.time_circuits_display_destination_time`**: A `timestamp` sensor for the top display row.
*   **`sensor.time_circuits_display_present_time`**: A `timestamp` sensor for the middle display row.
*   **`sensor.time_circuits_display_last_time_departed`**: A `timestamp` sensor for the bottom display row.
*   **`binary_sensor.time_circuits_display_is_animating`**: `On` when an animation is playing.
*   **`binary_sensor.time_circuits_display_is_asleep`**: `On` when the clock is in sleep mode.

#### **Core Controls**
*   **`number.time_circuits_display_destination_year`**: Sets the destination year.
*   **`select.time_circuits_display_last_departed_preset`**: Choose from movie-based or your custom presets.
*   **`number.time_circuits_display_brightness`**: Controls the display brightness (0-7).
*   **`number.time_circuits_display_volume`**: Adjusts the sound effect volume (0-30).
*   **`switch.time_circuits_display_24h_format`**: Toggles 24-hour time format.
*   **`select.time_circuits_display_profile`**: A powerful feature that applies a pre-configured bundle of settings at once. Profiles include `Standard`, `Cinematic`, `Silent Night`, and `Unstable`.

#### **Animation & Effects**
*   **`button.time_circuits_display_trigger_animation`**: Starts the full time travel sequence.
*   **`select.time_circuits_display_animation_style`**: Choose from a curated list of 10 popular animation styles. For the full list of 20+ styles, please use the web interface.
*   **`number.time_circuits_display_animation_interval`**: How often to auto-play the animation (in minutes, 0=off).
*   **`number.time_circuits_display_animation_duration`**: Sets the length of the time travel effect (in milliseconds).
*   **`switch.time_circuits_display_temporal_echo`**: A fun, experimental feature that creates a "ghosting" effect on the displays.

#### **Marquee & DataLink**
*   **`text.time_circuits_display_marquee_temp_override`**: Sends a temporary scrolling message. Use plain text or JSON for a timed message: `{"text": "ALERT", "duration": 60}`.
*   **`number.time_circuits_display_datalink_refresh`**: Sets the refresh interval for all API-based DataLink points.
*   **`select.time_circuits_display_datapoint_0_source`**: Sets the data source for marquee slot 1 (and 4 others).

#### **Stock Ticker Mode**
*   **`switch.time_circuits_display_stock_ticker_mode`**: Activates the 3-row stock ticker display.
*   **`text.time_circuits_display_stock_row_1`**: Sets the symbol for the top display row (e.g., `AAPL`).
*   **`text.time_circuits_display_stock_row_2`**: Sets the symbol for the middle display row (e.g., `^GSPC`).
*   **`text.time_circuits_display_stock_row_3`**: Sets the symbol for the bottom display row.
*   **`text.time_circuits_display_alpha_vantage_api_key`**: Sets the API key for the stock data provider.
    > ⚠️ **Important Note:** The stock data is sourced from **Financial Modeling Prep (FMP)**, not Alpha Vantage. Please register for a free API key at the [FMP website](https://site.financialmodelingprep.com/developer/docs).

#### **System & Actions**
*   **`button.time_circuits_display_reboot_device`**: Restarts the ESP32.
*   **`button.time_circuits_display_force_ntp_sync`**: Manually syncs the clock with time servers.
*   **`button.time_circuits_display_save_all_settings`**: Manually triggers a save of all current settings to the device's memory.
*   **`button.time_circuits_display_factory_reset`**: **Use with caution!** Resets all settings to their original factory defaults.
*   **`text.time_circuits_display_run_sequence`**: The advanced script runner for custom alerts. See the deep dive below.

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

### **1. Advanced Notifier Blueprint**
> **Purpose:** To create sophisticated, multi-step audio-visual alerts on the clock using the on-device sequencer, without writing any code.

* **How to Use:**
    1.  Create a new automation and select the "Advanced Notifier" blueprint.
    2.  **Trigger:** Define what event should trigger this notification (e.g., a door opening, a weather alert).
    3.  **Time Circuits Display:** Select your `Time Circuits Display` device from the dropdown.
    4.  **Notification Steps (1-3):** The blueprint provides three optional steps. For each step, you can define:
        * **Text:** The message to display on one of the three rows.
        * **Sound Effect:** An optional sound to play with the text.
        * **Flash Effect:** An optional effect to flash the text for emphasis.
        * **Delay:** A pause (in milliseconds) before the next step executes.
* **Example Scenario: "Mailbox Alert"**
    * **Trigger:** The `binary_sensor.mailbox_sensor` changes to `on`.
    * **Step 1:**
        * **Text (Present Time Row):** `MAIL`
        * **Sound Effect:** `CONFIRM_ON`
        * **Flash Effect:** Enabled
    * **Result:** When the mailbox is opened, the middle row of the clock will flash the word "MAIL" while playing a confirmation chime.

### **2. Dynamic Data Display Blueprint**
> **Purpose:** To easily display the state of any Home Assistant sensor on one of the five DataLink marquee slots.

* **How to Use:**
    1.  Create a new automation and select the "Dynamic Data Display" blueprint.
    2.  **Trigger:** Choose a trigger that determines when the display should update (e.g., a specific sensor changing state, or a time pattern).
    3.  **Time Circuits Display:** Select your clock device.
    4.  **Marquee Slot:** Choose which of the five DataLink slots you want to control.
    5.  **Display Text:** Enter the text you want to display. You can use templates to dynamically insert sensor states.
* **Example Scenario: "Display Current Power Usage"**
    * **Trigger:** The state of `sensor.home_power_usage` changes.
    * **Marquee Slot:** Slot 1
    * **Display Text:** `POWER {{ states('sensor.home_power_usage') }} W`
    * **Result:** Whenever your home's power consumption changes, the first marquee slot on the clock will automatically update to show the new value (e.g., `POWER 1210 W`).

### **3. Cinematic Scene Trigger Blueprint**
> **Purpose:** A simple way to create an automation that sets a destination year and immediately triggers the full time travel animation sequence.

* **How to Use:**
    1.  Create a new automation and select the "Cinematic Scene Trigger" blueprint.
    2.  **Trigger:** Define the event that should start the scene (e.g., a button press, a specific time).
    3.  **Time Circuits Display:** Select your clock device.
    4.  **Destination Year:** Enter the four-digit year you want to travel to.
* **Example Scenario: "Lightning Strike at 10:04 PM"**
    * **Trigger:** Time is `22:04:00`.
    * **Destination Year:** `1955`
    * **Result:** Every night at 10:04 PM, the clock will automatically set its destination to 1955 and play the full, cinematic time travel animation, complete with sound effects.

### **4. TTS Notifier Blueprint**
> **Purpose:** To use the clock as a powerful, themed notification device for your smart home, playing audio announcements from Home Assistant's Text-to-Speech (TTS) services.

Recent improvements have made this feature even more dynamic and integrated.

*   **Dynamic Volume Control**: Each TTS announcement can have its own volume level, set directly in your Home Assistant automation. This allows for context-aware alerts:
    *   A critical security warning can be set to **100% volume**.
    *   A routine announcement, like "The laundry is finished," can be set to a quieter **40% volume**.
*   **Visual Notification Mode**: To make announcements feel more polished, the clock now enters a special "Notification Mode":
    1.  **Intro**: When a TTS message is triggered, the display will show "INCOMING MESSAGE" for a moment to get your attention.
    2.  **Custom Message**: Your optional custom text (e.g., a speaker icon `🔊`) is then displayed.
    3.  **Audio Playback**: The audio message plays at your specified volume.
    4.  **Outro**: Once finished, the clock automatically returns to its normal display.

This creates a seamless and professional notification experience, turning your clock into a true smart home information hub.

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
      - entity: sensor.time_circuits_display_status
        name: Current State
      - entity: sensor.time_circuits_display_destination_time
      - entity: sensor.time_circuits_display_present_time
      - entity: sensor.time_circuits_display_last_time_departed
  - type: grid
    columns: 2
    cards:
      - type: button
        tap_action:
          action: call-service
          service: button.press
          target:
            entity_id: button.time_circuits_display_trigger_animation
        name: Trigger Animation
        icon: mdi:movie-play
      - type: button
        tap_action:
          action: call-service
          service: button.press
          target:
            entity_id: button.time_circuits_display_reboot_device
        name: Reboot Clock
        icon: mdi:restart
  - type: entities
    title: Core Controls
    entities:
      - entity: number.time_circuits_display_destination_year
      - entity: select.time_circuits_display_last_departed_preset
      - entity: number.time_circuits_display_brightness
      - entity: number.time_circuits_display_volume
</code></pre>
</details>

### **Voice Assistant Integration**
Trigger the time travel sequence with a voice command to Google Assistant or Alexa.

1.  **Create a Script:** Go to **Settings > Automations & Scenes > Scripts**. Create a new script named "Activate Time Circuits" and set its action to call the `button.press` service on the `button.time_circuits_display_trigger_animation` entity.
2.  **Expose the Script:** If you use the Home Assistant Cloud (Nabu Casa), expose the new "Activate Time Circuits" script to your voice assistants. If you have a manual setup, add the script to your exposed entities.
3.  **Create a Routine:**
    * **In the Google Home App:** Create a new routine. For the starter, use a voice command like "Activate the time circuits." For the action, choose "Try adding your own" and enter "Activate Time Circuits."
    * **In the Alexa App:** Create a new routine. For "When this happens," choose "Voice" and enter a phrase like "Great Scott." For the action, choose "Smart Home" and select the "Activate Time Circuits" script.
### **Advanced Templating Examples**
The "Dynamic Data Display" blueprint can be made even more powerful with templates. Here are some examples for the `Display Text` field:

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

### **Deep Dive: The "Run Sequence" Command**
The `run_sequence` entity is the most powerful feature in the integration, allowing you to create custom, perfectly timed audio-visual alerts. You send it a single string containing a script of commands separated by semicolons.

<details>
<summary><strong>Click to view Sequencer Details and Syntax</strong></summary>

#### **Syntax**
The basic syntax is `command(target, parameter); command2(target, parameter); ...`

#### **Command Reference**

| Command | Target(s) | Parameter(s) | Example | Description |
| :--- | :--- | :--- | :--- | :--- |
| `text` | `dest_year`, `pres_month`, `last_time`, etc. | The text to display | `text(dest_year, 2077)` | Sets the text of a specific display segment. |
| `sound` | N/A | `TIME_TRAVEL`, `ARRIVAL_THUD`, etc. | `sound(ARRIVAL_THUD)` | Plays one of the built-in sound files. |
| `wait` | N/A | Milliseconds | `wait(1500)` | Pauses the sequence for a set duration. |
| `flash`| `dest_year`, `pres_month`, etc. | Milliseconds | `flash(last_day, 500)` | Flashes a display segment for a duration. |

#### **Advanced Scripting Example**
Here is an example of a full security alert sequence. The script performs the following actions:
1.  Displays "ALRT" on the top row and plays the `ARRIVAL_THUD` sound.
2.  Waits for half a second (500ms).
3.  Displays "GRGE" on the middle row and flashes it for 1.5 seconds.
4.  Waits for 2 seconds.
5.  Clears the text from both rows.

<pre><code>text(dest_year, "ALRT"); sound("ARRIVAL_THUD");
wait(500);
text(pres_year, "GRGE"); flash(pres_year, 1500);
wait(2000);
text(dest_year, " "); text(pres_year, " ");
</code></pre>
</details>

<details>
<summary><strong>Advanced: MQTT Topic Reference</strong></summary>

> For debugging or use in other applications (like Node-RED), you can interact with the clock's MQTT topics directly. The base topic is `bttf-clock/bttf_timecircuits_01/`.
>
> | Topic Suffix | Type | Description |
> | :--- | :--- | :--- |
> | `status` | State | Publishes `online` or `offline`. Used for availability. |
> | `status/state` | State | Publishes the clock's current state (e.g., `Idle`, `Animating`). |
> | `destination_year/command`| Command | Send a 4-digit year to set the destination time. |
> | `destination_year/state` | State | Publishes the current destination year. |
> | `run_sequence/command` | Command | Send a script to be executed by the on-device sequencer. |
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
> * In MQTT Explorer, check the `bttf-clock/bttf_timecircuits_01/status` topic. It should have a retained message of `online`. If it says `offline`, the clock has disconnected.

> ⚠️ **Commands Not Working?**
> * Ensure the clock is `online` and the entities are available in Home Assistant.
> * Use MQTT Explorer to listen for commands being sent from Home Assistant. When you toggle a switch, you should see a message published to the corresponding `/command` topic.

***

## **Next Steps**

Now that you've mastered the Home Assistant integration, why not explore more?
* **Get Inspired:** Check out our list of **[30+ Automation Examples](../HOME_ASSISTANT_EXAMPLES.md)** for creative ideas.
* **Dive Deeper:** For those looking to modify the firmware or understand the code, the **[Developer's Guide](../DEVELOPMENT.md)** provides a full technical breakdown.