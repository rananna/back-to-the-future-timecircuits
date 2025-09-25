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