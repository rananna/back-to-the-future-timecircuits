# Back to the Future Time Circuits: Home Assistant Automations

This document provides a list of useful and creative automations to demonstrate how you can integrate the Time Circuits display into your smart home. These examples assume your device is named `time_circuits_display` in Home Assistant.

---

### Blueprint-Powered Examples

The easiest way to create powerful automations is by using the provided blueprints. Here are a few examples to get you started.

<details>
<summary><strong>1. "Weather Station" Display</strong></summary>

*Use the "BTTF - Home Assistant Status Display" blueprint to turn your clock into a real-time weather dashboard.*

**Automation Setup:**
1.  **Trigger:** The automation should trigger on any state change of your primary weather sensors (e.g., temperature, humidity, and conditions).
2.  **Action:** Call the "BTTF - Home Assistant Status Display" blueprint.
3.  **Mapping:**
    *   **Destination Month:** `OUT`
    *   **Destination Day:** `SIDE`
    *   **Destination Year:** `TEMP`
    *   **Destination Time:** `{{ states('sensor.outside_temperature') }}°`
    *   **Present Month:** `FEELS`
    *   **Present Day:** `LIKE`
    *   **Present Year:** `{{ states('sensor.outside_feels_like_temperature') }}°`
    *   **Last Departed Text:** `{{ states('sensor.weather_conditions') }}` (This will be a single scrolling line on the bottom display)

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
    - service: automation.trigger
      target:
        entity_id: automation.home_assistant_status_display # The automation you created from the blueprint
      data:
        # You can pass variables to the blueprint if you configured it to accept them
        # For this example, we assume the blueprint is configured with the templates directly.
```
</details>

<details>
<summary><strong>2. "Now Playing" Marquee</strong></summary>

*Use the "BTTF - Dynamic Marquee Display" blueprint to show the currently playing song on your favorite media player.*

**Automation Setup:**
1.  **Web UI:** In the clock's web interface, set Data Point 5's "Data Source" to "Home Assistant Push".
2.  **Trigger:** The automation should trigger whenever the `media_title` attribute of your media player changes.
3.  **Action:** Call the "BTTF - Dynamic Marquee Display" blueprint.
4.  **Configuration:**
    *   **Data Point Slot:** "Data Point 5"
    *   **Marquee Text:** `♪ {{ state_attr('media_player.spotify', 'media_title') }}`
    > **Note:** The marquee text is limited to 16 characters. This example will show the first part of the song title.

```yaml
# automation.yaml
- alias: "BTTF - Now Playing Marquee"
  trigger:
    - platform: template
      value_template: "{{ state_attr('media_player.spotify', 'media_title') }}"
  action:
    - service: automation.trigger
      target:
        entity_id: automation.dynamic_marquee_display # The automation you created from the blueprint
```
</details>

---
Here are 30 useful and well-thought-out automations to demonstrate how you can integrate the Time Circuits display into your smart home. These examples assume your device is named `time_circuits_display` in Home Assistant.

---

### Cinematic & Fun Automations

<details>
<summary><strong>1. "It's 10:04 PM!" - The Lightning Strike</strong></summary>

*Triggers the iconic lightning strike scene every night at 10:04 PM. This is a perfect use for the "BTTF - Cinematic Scene Trigger" blueprint.*

**Automation Setup:**
1.  Create a new automation and select the "BTTF - Cinematic Scene Trigger" blueprint.
2.  **Trigger:** Select "Time" as the trigger type and enter `22:04:00`.
3.  **Time Circuits Device:** Select your clock.
4.  **Destination Year:** Enter `1985`.

*That's it! The blueprint handles setting the year and playing the animation.*
</details>

<details>
<summary><strong>2. "Roads? Where We're Going, We Don't Need Roads."</strong></summary>

*Sets the destination to the future and plays the animation when you start your vacuum cleaner. This is another great use for the "BTTF - Cinematic Scene Trigger" blueprint.*

**Automation Setup:**
1.  Create a new automation and select the "BTTF - Cinematic Scene Trigger" blueprint.
2.  **Trigger:** Select "State" as the trigger type, use `vacuum.roomba` as the entity, and set the "To" state to `cleaning`.
3.  **Time Circuits Device:** Select your clock.
4.  **Destination Year:** Enter `2015`.
</details>

<details>
<summary><strong>3. "Sync the Clocks" - Multi-Device Animation</strong></summary>

*When a time travel animation starts, flash your smart lights to match the effect.*

```yaml
alias: "BTTF - Sync the Clocks"
trigger:
  - platform: device
    device_id: YOUR_DEVICE_ID_HERE
    domain: timecircuits
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
<summary><strong>4. "Temporal Paradox" - Glitch Overload</strong></summary>

*If the clock malfunctions, make your smart lights flicker randomly.*

```yaml
alias: "BTTF - Paradox Glitch"
trigger:
  - platform: device
    device_id: YOUR_DEVICE_ID_HERE
    domain: timecircuits
    type: malfunction_triggered
action:
  - service: light.turn_on
    target:
      entity_id: light.office_lights
    data:
      effect: "strobe"
```
</details>

<details>
<summary><strong>5. Birthday Time Jump</strong></summary>

*On your birthday, automatically set the destination year to the year you were born and play the animation. The "Cinematic Scene Trigger" blueprint makes this easy.*

**Automation Setup:**
1.  Create a new automation and select the "Cinematic Scene Trigger" blueprint.
2.  **Trigger:** Select "Template" as the trigger type and enter `{{ now().month == 10 and now().day == 26 }}` (replace with your birthday).
3.  **Time Circuits Device:** Select your clock.
4.  **Destination Year:** Enter your birth year.
</details>

---

### Daily Routines & Practical Uses

<details>
<summary><strong>6. Good Morning, Hill Valley</strong></summary>

*When you dismiss your morning alarm, the clock wakes from sleep mode and displays the current weather.*

```yaml
alias: "BTTF - Good Morning"
trigger:
  - platform: state
    entity_id: input_boolean.morning_alarm_dismissed
    to: "on"
action:
  - service: switch.turn_on
    target:
      entity_id: switch.time_circuits_display_power
```
</details>

<details>
<summary><strong>7. "OUTATIME" - Leaving Home</strong></summary>

*When the last person leaves the house, put the clock into sleep mode to save power.*

```yaml
alias: "BTTF - Everyone Left"
trigger:
  - platform: state
    entity_id: group.family
    to: "not_home"
action:
  - service: switch.turn_off
    target:
      entity_id: switch.time_circuits_display_power
```
</details>

<details>
<summary><strong>8. "Welcome to the Future" - Arriving Home</strong></summary>

*When the first person arrives home, wake the clock up and set the destination to the current year.*

```yaml
alias: "BTTF - Welcome Home"
trigger:
  - platform: state
    entity_id: group.family
    to: "home"
action:
  - service: switch.turn_on
    target:
      entity_id: switch.time_circuits_display_power
  - service: number.set_value
    target:
      entity_id: number.time_circuits_display_dest_year
    data:
      value: "{{ now().year }}"
```
</details>

<details>
<summary><strong>9. Movie Night Ambiance</strong></summary>

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
      entity_id: number.time_circuits_display_brightness
    data:
      value: "1"
  - service: switch.turn_off
    target:
      entity_id: switch.time_circuits_display_datapoint_0_enabled
```
</details>

<details>
<summary><strong>10. "Your Future is Whatever You Make of It" - Bedtime</strong></summary>

*When you activate your "Goodnight" scene, the clock enters sleep mode.*

```yaml
alias: "BTTF - Bedtime"
trigger:
  - platform: state
    entity_id: scene.goodnight
    to: "on"
action:
  - service: switch.turn_off
    target:
      entity_id: switch.time_circuits_display_power
```
</details>

---

### Notifications & Alerts

<details>
<summary><strong>11. "The Libyans!" - Security Alert</strong></summary>

*If a door or window is opened while the security system is armed, flash a warning message on the display. This is a perfect use for the "BTTF - Advanced Notifier" blueprint.*

**Automation Setup:**
1.  Create a new automation and select the "BTTF - Advanced Notifier" blueprint.
2.  **Trigger:** Select "State" as the trigger type, use `binary_sensor.front_door_contact` as the entity, and set the "To" state to `on`.
3.  **Condition:** Add a "State" condition, use `alarm_control_panel.home_alarm` as the entity, and set the required state to `armed_away`.
4.  **Time Circuits Display:** Select your clock.
5.  **Message:** `SECURITY\nALERT\nFRONT DOOR`
6.  **Display Duration:** `30` (seconds)
7.  **Sound Effect:** `ALARM_SOUND` (or any other you prefer)
</details>

<details>
<summary><strong>12. Severe Weather Warning</strong></summary>

*If a severe weather alert is active, override the display to show the warning. Use the "BTTF - Advanced Notifier" blueprint for a simple setup.*

**Automation Setup:**
1.  Create a new automation and select the "BTTF - Advanced Notifier" blueprint.
2.  **Trigger:** Select "State" as the trigger type, use `binary_sensor.severe_weather_alert` as the entity, and set the "To" state to `on`.
3.  **Time Circuits Display:** Select your clock.
4.  **Message:** `SEVERE\nWEATHER\n{{ states('sensor.weather_alert_type') }}`
5.  **Display Duration:** `600` (10 minutes)
6.  **Sound Effect:** `ALARM_SOUND`
</details>

<details>
<summary><strong>13. Garbage Day Reminder</strong></summary>

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
      entity_id: text.time_circuits_display_datapoint_2_marquee
    data:
      value: "TRASH NIGHT"
```
</details>

<details>
<summary><strong>14. "Mr. Fusion" - Low Battery Alert</strong></summary>

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
      entity_id: text.time_circuits_display_datapoint_3_marquee
    data:
      value: "CHARGE PHONE"
```
</details>

<details>
<summary><strong>15. Guest Welcome Message</strong></summary>

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
      entity_id: text.time_circuits_display_datapoint_0_marquee
    data:
      value: "WELCOME {{ trigger.event.data.host_name }}"
```
</details>

---

### Dynamic Data Display

<details>
<summary><strong>16. Stock Ticker</strong></summary>

*Display the current price of a stock on the marquee.*

```yaml
alias: "BTTF - Stock Ticker"
trigger:
  - platform: time_pattern
    minutes: "/15"
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_0_marquee
    data:
      value: "AAPL ${{ states('sensor.aapl_stock_price') }}"
```
</details>

<details>
<summary><strong>17. "The Sports Almanac" - Live Game Score</strong></summary>

*Show the score of your favorite team's game while it's being played.*

```yaml
alias: "BTTF - Game Score"
trigger:
  - platform: state
    entity_id: sensor.favorite_team_score
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_1_marquee
    data:
      value: "SCORE {{ states('sensor.favorite_team_score') }}"
```
</details>

<details>
<summary><strong>18. YouTube Subscriber Count</strong></summary>

*Display your YouTube subscriber count and update it periodically.*

```yaml
alias: "BTTF - YouTube Subscribers"
trigger:
  - platform: time_pattern
    hours: "/1"
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_2_marquee
    data:
      value: "SUBS {{ states('sensor.youtube_subscriber_count') }}"
```
</details>

<details>
<summary><strong>19. "How Many People Are in Space Right Now?"</strong></summary>

*Display the current number of astronauts in space.*

```yaml
alias: "BTTF - People in Space"
trigger:
  - platform: time_pattern
    hours: "/6"
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_3_marquee
    data:
      value: "SPACE {{ states('sensor.people_in_space') }}"
```
</details>

<details>
<summary><strong>20. Network Status</strong></summary>

*Show your internet download speed on the marquee.*

```yaml
alias: "BTTF - Network Speed"
trigger:
  - platform: time_pattern
    minutes: "/5"
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_4_marquee
    data:
      value: "NET {{ states('sensor.speedtest_download') }} Mbps"
```
</details>

---

### Advanced & Creative Scripts

<details>
<summary><strong>21. "Save the Clock Tower!" - Countdown Script</strong></summary>

*A script to create a 10-second countdown on the display, ending with a time travel animation.*

```yaml
alias: "BTTF - Countdown Script"
sequence:
  - service: switch.turn_on
    target:
      entity_id: switch.time_circuits_display_override_switch
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_override_message
    data:
      value: "COUNTDOWN\n10"
  - delay: "00:00:01"
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_override_message
    data:
      value: "COUNTDOWN\n09"
  # ... (repeat for each number down to 01)
  - delay: "00:00:01"
  - service: switch.turn_off
    target:
      entity_id: switch.time_circuits_display_override_switch
  - service: button.press
    target:
      entity_id: button.time_circuits_display_trigger_animation
```
</details>

<details>
<summary><strong>22. "Doc, You're My Only Hope" - NFC Tag Message</strong></summary>

*Tap an NFC tag with your phone to send a pre-set message to the display.*

```yaml
alias: "BTTF - NFC Message"
trigger:
  - platform: tag
    tag_id: "YOUR_NFC_TAG_ID_HERE"
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_0_marquee
    data:
      value: "HELP ME DOC"
```
</details>

<details>
<summary><strong>23. Change Animation Style Based on Time of Day</strong></summary>

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
      entity_id: select.time_circuits_display_animation_style
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
<summary><strong>24. "Are You Telling Me You Built a Time Machine... Out of a DeLorean?"</strong></summary>

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
      entity_id: button.time_circuits_display_trigger_animation
```
</details>

<details>
<summary><strong>25. "Don't Drive 88!" - Speeding Alert</strong></summary>

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
      entity_id: text.time_circuits_display_datapoint_0_marquee
    data:
      value: "SLOW DOWN!"
```
</details>

<details>
<summary><strong>26. Low Memory Reboot</strong></summary>

*Monitors the clock's free memory and reboots it if it drops to a critical level.*

**Automation Setup:**
1.  First, create a "callable" automation using the **BTTF - Advanced Notifier** blueprint. Set the message to `REBOOTING\nLOW MEMORY` and the duration to 10 seconds.
2.  Then, create a second automation that triggers when the memory is low, calls the notifier automation, waits 10 seconds, and then presses the reboot button.

```yaml
# automation.yaml
- alias: "BTTF - Low Memory Reboot"
  trigger:
    # The 'free_heap' attribute is part of the main status sensor.
    - platform: numeric_state
      entity_id: sensor.time_circuits_display_status
      attribute: free_heap
      below: 20000  # 20 KB
  action:
    # Call the notifier automation you created in step 1
    - service: automation.trigger
      target:
        entity_id: automation.your_bttf_notifier_automation
    - delay: "00:00:10"
    # Press the actual reboot button on the device
    - service: button.press
      target:
        entity_id: button.time_circuits_display_reboot_device
```
</details>


<details>
<summary><strong>28. Calendar-Driven Destination Time</strong></summary>

*Automatically sets the "Destination Time" to the date of your next calendar event.*

```yaml
alias: "BTTF - Next Calendar Event"
trigger:
  - platform: state
    entity_id: calendar.your_calendar
action:
  - service: number.set_value
    target:
      entity_id: number.time_circuits_display_dest_year
    data:
      value: "{{ state_attr('calendar.your_calendar', 'start_time').split(' ')[0].split('-')[0] }}"
```
</details>

<details>
<summary><strong>29. "Save the Clock Tower!" - Fundraising Goal Tracker</strong></summary>

*Display the progress of a fundraising or savings goal on the marquee.*

```yaml
alias: "BTTF - Savings Goal Tracker"
trigger:
  - platform: state
    entity_id: input_number.savings_goal_current
action:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_3_marquee
    data:
      value: >
        GOAL ${{ states('input_number.savings_goal_current') }} / ${{ states('input_number.savings_goal_target') }}
```
</details>

<details>
<summary><strong>30. "Doc's Notes" - Rotating Reminders Script</strong></summary>

*A script to cycle through a list of reminders or quotes on the marquee.*

```yaml
alias: "BTTF - Rotating Reminders Script"
sequence:
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_4_marquee
    data:
      value: "REMINDER: WATER THE PLANTS"
  - delay: "00:01:00"
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_4_marquee
    data:
      value: "QUOTE: THE FUTURE IS WHATEVER YOU MAKE OF IT"
  - delay: "00:01:00"
  - service: text.set_value
    target:
      entity_id: text.time_circuits_display_datapoint_4_marquee
    data:
      value: "TASK: TAKE OUT THE RECYCLING"
mode: restart
```
</details>