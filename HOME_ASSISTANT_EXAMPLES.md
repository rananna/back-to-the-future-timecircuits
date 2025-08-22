# Back to the Future Time Circuits: Home Assistant Automations

Here are 30 useful and well-thought-out automations to demonstrate how you can integrate the Time Circuits display into your smart home. These examples assume your device is named `bttf_timecircuits_01` in Home Assistant.

---

### Cinematic & Fun Automations

<details>
<summary><strong>1. "It's 10:04 PM!" - The Lightning Strike</strong></summary>

*Triggers the iconic lightning strike scene every night at 10:04 PM.*

<pre><code>
alias: "BTTF - Lightning Strike"
trigger:
  - platform: time
    at: "22:04:00"
action:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_dest_year
    data:
      value: "1985"
  - delay: "00:00:02"
  - service: button.press
    target:
      entity_id: button.bttf_timecircuits_01_trigger_animation
</code></pre>
</details>

<details>
<summary><strong>2. "Roads? Where We're Going, We Don't Need Roads."</strong></summary>

*Sets the destination to the future and plays the animation when you start your vacuum cleaner.*

<pre><code>
alias: "BTTF - We Don't Need Roads"
trigger:
  - platform: state
    entity_id: vacuum.roomba
    to: "cleaning"
action:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_dest_year
    data:
      value: "2015"
  - service: button.press
    target:
      entity_id: button.bttf_timecircuits_01_trigger_animation
</code></pre>
</details>

<details>
<summary><strong>3. "Sync the Clocks" - Multi-Device Animation</strong></summary>

*When a time travel animation starts, flash your smart lights to match the effect.*

<pre><code>
alias: "BTTF - Sync the Clocks"
trigger:
  - platform: device
    device_id: YOUR_DEVICE_ID_HERE
    domain: bttf-clock
    type: animation_started
action:
  - service: light.turn_on
    target:
      entity_id: light.living_room_lights
    data:
      effect: "flash"
</code></pre>
</details>

<details>
<summary><strong>4. "Temporal Paradox" - Glitch Overload</strong></summary>

*If the clock malfunctions, make your smart lights flicker randomly.*

<pre><code>
alias: "BTTF - Paradox Glitch"
trigger:
  - platform: device
    device_id: YOUR_DEVICE_ID_HERE
    domain: bttf-clock
    type: malfunction_triggered
action:
  - service: light.turn_on
    target:
      entity_id: light.office_lights
    data:
      effect: "strobe"
</code></pre>
</details>

<details>
<summary><strong>5. Birthday Time Jump</strong></summary>

*On your birthday, automatically set the destination year to the year you were born and play the animation.*

<pre><code>
alias: "BTTF - Birthday Time Jump"
trigger:
  - platform: template
    value_template: "{{ now().month == 10 and now().day == 26 }}" # Your birthday
action:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_dest_year
    data:
      value: "1985" # Your birth year
  - service: button.press
    target:
      entity_id: button.bttf_timecircuits_01_trigger_animation
</code></pre>
</details>

---

### Daily Routines & Practical Uses

<details>
<summary><strong>6. Good Morning, Hill Valley</strong></summary>

*When you dismiss your morning alarm, the clock wakes from sleep mode and displays the current weather.*

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
</code></pre>
</details>

<details>
<summary><strong>7. "OUTATIME" - Leaving Home</strong></summary>

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

<details>
<summary><strong>8. "Welcome to the Future" - Arriving Home</strong></summary>

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

<details>
<summary><strong>9. Movie Night Ambiance</strong></summary>

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

<details>
<summary><strong>10. "Your Future is Whatever You Make of It" - Bedtime</strong></summary>

*When you activate your "Goodnight" scene, the clock enters sleep mode.*

<pre><code>
alias: "BTTF - Bedtime"
trigger:
  - platform: state
    entity_id: scene.goodnight
    to: "on"
action:
  - service: switch.turn_off
    target:
      entity_id: switch.bttf_timecircuits_01_power
</code></pre>
</details>

---

### Notifications & Alerts

<details>
<summary><strong>11. "The Libyans!" - Security Alert</strong></summary>

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

<details>
<summary><strong>12. Severe Weather Warning</strong></summary>

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

<details>
<summary><strong>13. Garbage Day Reminder</strong></summary>

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

<details>
<summary><strong>14. "Mr. Fusion" - Low Battery Alert</strong></summary>

*If your phone's battery is low, display a reminder to charge it.*

<pre><code>
alias: "BTTF - Low Battery"
trigger:
  - platform: numeric_state
    entity_id: sensor.phone_battery_level
    below: 20
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_3_marquee
    data:
      value: "CHARGE PHONE"
</code></pre>
</details>

<details>
<summary><strong>15. Guest Welcome Message</strong></summary>

*When a new device joins your guest WiFi network, display a welcome message.*

<pre><code>
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
      entity_id: text.bttf_timecircuits_01_datapoint_0_marquee
    data:
      value: "WELCOME {{ trigger.event.data.host_name }}"
</code></pre>
</details>

---

### Dynamic Data Display

<details>
<summary><strong>16. Stock Ticker</strong></summary>

*Display the current price of a stock on the marquee.*

<pre><code>
alias: "BTTF - Stock Ticker"
trigger:
  - platform: time_pattern
    minutes: "/15"
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_0_marquee
    data:
      value: "AAPL ${{ states('sensor.aapl_stock_price') }}"
</code></pre>
</details>

<details>
<summary><strong>17. "The Sports Almanac" - Live Game Score</strong></summary>

*Show the score of your favorite team's game while it's being played.*

<pre><code>
alias: "BTTF - Game Score"
trigger:
  - platform: state
    entity_id: sensor.favorite_team_score
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_1_marquee
    data:
      value: "SCORE {{ states('sensor.favorite_team_score') }}"
</code></pre>
</details>

<details>
<summary><strong>18. YouTube Subscriber Count</strong></summary>

*Display your YouTube subscriber count and update it periodically.*

<pre><code>
alias: "BTTF - YouTube Subscribers"
trigger:
  - platform: time_pattern
    hours: "/1"
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_2_marquee
    data:
      value: "SUBS {{ states('sensor.youtube_subscriber_count') }}"
</code></pre>
</details>

<details>
<summary><strong>19. "How Many People Are in Space Right Now?"</strong></summary>

*Display the current number of astronauts in space.*

<pre><code>
alias: "BTTF - People in Space"
trigger:
  - platform: time_pattern
    hours: "/6"
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_3_marquee
    data:
      value: "SPACE {{ states('sensor.people_in_space') }}"
</code></pre>
</details>

<details>
<summary><strong>20. Network Status</strong></summary>

*Show your internet download speed on the marquee.*

<pre><code>
alias: "BTTF - Network Speed"
trigger:
  - platform: time_pattern
    minutes: "/5"
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_4_marquee
    data:
      value: "NET {{ states('sensor.speedtest_download') }} Mbps"
</code></pre>
</details>

---

### Advanced & Creative Scripts

<details>
<summary><strong>21. "Save the Clock Tower!" - Countdown Script</strong></summary>

*A script to create a 10-second countdown on the display, ending with a time travel animation.*

<pre><code>
alias: "BTTF - Countdown Script"
sequence:
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_timecircuits_01_override_switch
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_override_message
    data:
      value: "COUNTDOWN\n10"
  - delay: "00:00:01"
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_override_message
    data:
      value: "COUNTDOWN\n09"
  # ... (repeat for each number down to 01)
  - delay: "00:00:01"
  - service: switch.turn_off
    target:
      entity_id: switch.bttf_timecircuits_01_override_switch
  - service: button.press
    target:
      entity_id: button.bttf_timecircuits_01_trigger_animation
</code></pre>
</details>

<details>
<summary><strong>22. "Doc, You're My Only Hope" - NFC Tag Message</strong></summary>

*Tap an NFC tag with your phone to send a pre-set message to the display.*

<pre><code>
alias: "BTTF - NFC Message"
trigger:
  - platform: tag
    tag_id: "YOUR_NFC_TAG_ID_HERE"
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_0_marquee
    data:
      value: "HELP ME DOC"
</code></pre>
</details>

<details>
<summary><strong>23. Change Animation Style Based on Time of Day</strong></summary>

*Use a more subtle animation in the evening and a more energetic one during the day.*

<pre><code>
alias: "BTTF - Dynamic Animation Style"
trigger:
  - platform: sun
    event: sunset
  - platform: sun
    event: sunrise
action:
  - service: select.select_option
    target:
      entity_id: select.bttf_timecircuits_01_animation_style
    data:
      option: >
        {% if trigger.platform == 'sun' and trigger.event == 'sunset' %}
          Wave Flicker
        {% else %}
          Tornado Flicker
        {% endif %}
</code></pre>
</details>

<details>
<summary><strong>24. "Are You Telling Me You Built a Time Machine... Out of a DeLorean?"</strong></summary>

*When your car enters the garage, trigger a welcome animation.*

<pre><code>
alias: "BTTF - Car Arrival"
trigger:
  - platform: state
    entity_id: binary_sensor.garage_car_presence
    to: "on"
action:
  - service: button.press
    target:
      entity_id: button.bttf_timecircuits_01_trigger_animation
</code></pre>
</details>

<details>
<summary><strong>25. "Don't Drive 88!" - Speeding Alert</strong></summary>

*If your connected car is going over 85 mph, display a warning on the clock.*

<pre><code>
alias: "BTTF - Speeding Alert"
trigger:
  - platform: numeric_state
    entity_id: sensor.car_speed
    above: 85
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_0_marquee
    data:
      value: "SLOW DOWN!"
</code></pre>
</details>

<details>
<summary><strong>26. Low Memory Reboot</strong></summary>

*Monitors the clock's free memory and reboots it if it drops to a critical level.*

<pre><code>
alias: "BTTF - Low Memory Reboot"
trigger:
  - platform: numeric_state
    entity_id: sensor.bttf_timecircuits_01_free_memory
    below: 20000  # 20 KB
action:
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_timecircuits_01_override_switch
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_override_message
    data:
      value: "REBOOTING\nLOW MEMORY"
  - delay: "00:00:10"
  - service: homeassistant.restart
    target:
      device_id: YOUR_DEVICE_ID_HERE
</code></pre>
</details>

<details>
<summary><strong>27. "Temporal Instability" Mode Script</strong></summary>

*A script to make the display chaotic for a short period, perfect for showing off the effects.*

<pre><code>
alias: "BTTF - Temporal Instability Mode"
sequence:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_glitch_instability
    data:
      value: 80
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_malfunction_chance
    data:
      value: 10
  - delay: "00:01:00"
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_glitch_instability
    data:
      value: 0
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_malfunction_chance
    data:
      value: 25
</code></pre>
</details>

<details>
<summary><strong>28. Calendar-Driven Destination Time</strong></summary>

*Automatically sets the "Destination Time" to the date of your next calendar event.*

<pre><code>
alias: "BTTF - Next Calendar Event"
trigger:
  - platform: state
    entity_id: calendar.your_calendar
action:
  - service: number.set_value
    target:
      entity_id: number.bttf_timecircuits_01_dest_year
    data:
      value: "{{ state_attr('calendar.your_calendar', 'start_time').split(' ')[0].split('-')[0] }}"
</code></pre>
</details>

<details>
<summary><strong>29. "Save the Clock Tower!" - Fundraising Goal Tracker</strong></summary>

*Display the progress of a fundraising or savings goal on the marquee.*

<pre><code>
alias: "BTTF - Savings Goal Tracker"
trigger:
  - platform: state
    entity_id: input_number.savings_goal_current
action:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_3_marquee
    data:
      value: >
        GOAL ${{ states('input_number.savings_goal_current') }} / ${{ states('input_number.savings_goal_target') }}
</code></pre>
</details>

<details>
<summary><strong>30. "Doc's Notes" - Rotating Reminders Script</strong></summary>

*A script to cycle through a list of reminders or quotes on the marquee.*

<pre><code>
alias: "BTTF - Rotating Reminders Script"
sequence:
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_4_marquee
    data:
      value: "REMINDER: WATER THE PLANTS"
  - delay: "00:01:00"
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_4_marquee
    data:
      value: "QUOTE: THE FUTURE IS WHATEVER YOU MAKE OF IT"
  - delay: "00:01:00"
  - service: text.set_value
    target:
      entity_id: text.bttf_timecircuits_01_datapoint_4_marquee
    data:
      value: "TASK: TAKE OUT THE RECYCLING"
mode: restart
</code></pre>
</details>