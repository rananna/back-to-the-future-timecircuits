# Back to the Future Time Circuits: Home Assistant Automations

Here are 10 useful and well-thought-out automations to demonstrate how you can integrate the Time Circuits display into your smart home. These examples assume your device is named `bttf_timecircuits_01` in Home Assistant.

---

<details>
<summary><strong>1. Good Morning, Hill Valley</strong></summary>

*When you dismiss your morning alarm, the clock wakes from sleep mode and displays the current weather on the marquee.*

<pre><code>
alias: "BTTF - Good Morning"
trigger:
  - platform: state
    entity_id: input_boolean.morning_alarm_dismissed
    to: "on"
action:
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_timecircuits_01_power
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_0_marquee
    data:
      value: "TEMP {{ states('weather.home', 'temperature') }}°"
</code></pre>
</details>

---

<details>
<summary><strong>2. "OUTATIME" - Leaving Home</strong></summary>

*When the last person leaves the house, put the clock into sleep mode to save power.*

<pre><code>
alias: "BTTF - Everyone Left"
trigger:
  - platform: state
    entity_id: group.family
    to: "not_home"
action:
  - service: switch.turn_off
    target:
      entity_id: switch.bttf_timecircuits_01_power
</code></pre>
</details>

---

<details>
<summary><strong>3. "Welcome to the Future" - Arriving Home</strong></summary>

*When the first person arrives home, wake the clock up and set the destination to the current year.*

<pre><code>
alias: "BTTF - Welcome Home"
trigger:
  - platform: state
    entity_id: group.family
    to: "home"
action:
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_timecircuits_01_power
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_dest_year
    data:
      value: "{{ now().year }}"
</code></pre>
</details>

---

<details>
<summary><strong>4. Movie Night Ambiance</strong></summary>

*When you start a movie, dim the clock's brightness and turn off any distracting marquee messages.*

<pre><code>
alias: "BTTF - Movie Night"
trigger:
  - platform: state
    entity_id: media_player.living_room_tv
    to: "playing"
action:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_brightness
    data:
      value: "1"
  - service: switch.turn_off
    target:
      entity_id: switch.bttf_timecircuits_01_datapoint_0_enabled
</code></pre>
</details>

---

<details>
<summary><strong>5. "The Libyans!" - Security Alert</strong></summary>

*If a door or window is opened while the security system is armed, flash a warning message on the display.*

<pre><code>
alias: "BTTF - Security Alert"
trigger:
  - platform: state
    entity_id: binary_sensor.front_door_contact
    to: "on"
condition:
  - condition: state
    entity_id: alarm_control_panel.home_alarm
    state: "armed_away"
action:
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_timecircuits_01_override_switch
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_override_message
    data:
      value: "SECURITY\nALERT\nFRONT DOOR"
</code></pre>
</details>

---

<details>
<summary><strong>6. Severe Weather Warning</strong></summary>

*If a severe weather alert is active, override the display to show the warning.*

<pre><code>
alias: "BTTF - Weather Alert"
trigger:
  - platform: state
    entity_id: binary_sensor.severe_weather_alert
    to: "on"
action:
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_timecircuits_01_override_switch
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_override_message
    data:
      value: "SEVERE\nWEATHER\n{{ states('sensor.weather_alert_type') }}"
</code></pre>
</details>

---

<details>
<summary><strong>7. "Save the Clock Tower!" - Countdown to an Event</strong></summary>

*Display a countdown to your next important calendar event on the marquee.*

<pre><code>
alias: "BTTF - Calendar Countdown"
trigger:
  - platform: time_pattern
    minutes: "/1"
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_1_marquee
    data:
      value: >
        EVENT {{ state_attr('calendar.your_calendar', 'message') }} IN {{ ((state_attr('calendar.your_calendar', 'start_time') | as_timestamp - now() | as_timestamp) / 60) | round(0) }} MIN
</code></pre>
</details>

---

<details>
<summary><strong>8. Garbage Day Reminder</strong></summary>

*The night before garbage day, display a persistent reminder on the marquee.*

<pre><code>
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
      entity_id: text.bttf_timecircuits_01_datapoint_2_marquee
    data:
      value: "TRASH NIGHT"
</code></pre>
</details>

---

<details>
<summary><strong>9. "1.21 Gigawatts!" - High Power Consumption</strong></summary>

*Trigger a "malfunction" effect and a visual alert when the house's power consumption spikes.*

<pre><code>
alias: "BTTF - High Power Usage"
trigger:
  - platform: numeric_state
    entity_id: sensor.home_power_usage
    above: 5000 # 5kW
action:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_malfunction_chance
    data:
      value: "1" # Guarantee a malfunction
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_override_message
    data:
      value: "1.21 GW\nPOWER SURGE"
</code></pre>
</details>

---

<details>
<summary><strong>10. "Fluxing" with the Music - Synced Ambiance</strong></summary>

*When you play a specific song (e.g., "The Power of Love"), increase the "temporal instability" for a cool visual effect.*

<pre><code>
alias: "BTTF - Music Glitch Effect"
trigger:
  - platform: state
    entity_id: media_player.spotify
    attribute: media_title
    to: "The Power of Love"
action:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_glitch_instability
    data:
      value: "75"
  - wait_for_trigger:
      - platform: state
        entity_id: media_player.spotify
        not_to: "playing"
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_glitch_instability
    data:
      value: "0"
</code></pre>
</details>