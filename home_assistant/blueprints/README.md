# BTTF Time Circuits - Home Assistant Blueprints

Welcome, time traveler! This guide is your flux capacitor for integrating the BTTF Time Circuits clock with Home Assistant. These blueprints are designed to make it incredibly simple to display information, create alerts, and run animations, turning your device from a cool clock into a dynamic, interactive part of your smart home.

Whether you want to see the outside temperature, get a visual alert when the doorbell rings, or run a countdown to movie night, you've come to the right place.

---

## Installation

Getting started is as easy as a 1.21-gigawatt lightning strike (and much safer).

1.  **Copy Blueprints**: Copy the `.yaml` files from this directory into your Home Assistant's `/config/blueprints/script/` directory. The easiest way to do this is with the Samba Share, FTP, or File Editor add-ons. You can copy all of them, or just the ones you need.
2.  **Reload Blueprints**: In the Home Assistant UI, go to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the corner and select **Reload Blueprints**.
3.  **Create a Script**: The blueprints will now appear in your list. Click **Create Script** on the one you want to use, fill in the options, and save it. This script is now ready to be called from your dashboards or, more powerfully, from automations.

---

## Core Concepts: Scripts vs. Automations

To unlock the full potential of your Time Circuits, it's crucial to understand how Home Assistant uses scripts and automations together.

**1. Scripts are "One-Shot" Actions**

Think of a blueprint script as a single, specific mission: "Show this text now." When you run the script, it executes its commands (e.g., display a temperature reading) and then it's done. It doesn't keep running or update automatically.

**2. Automations Provide the Brains**

Automations are the engine of your smart home. They watch for triggers (like a sensor changing, a time of day, or a button press) and then perform actions, such as running one of your blueprint scripts.

> **The Pattern:** You create a **Script** from a blueprint to define *what* you want to happen. Then, you create an **Automation** to decide *when* it should happen.

---

## Blueprint Guide: Display Text

This is your workhorse for displaying any custom message. It's perfect for alerts, notifications, and simple status messages.

*   **Inputs:**
    *   `Device ID`: The unique identifier for your Time Circuits clock.
    *   `Display Row`: Which of the three rows (Top, Middle, Bottom) to use.
    *   `Text`: The message to display. You can use Home Assistant templates here!
    *   `Effect`: An optional visual effect (e.g., Pulse, Flash, Marquee).
    *   `Duration (seconds)`: How long the text should remain visible.
    *   `Sound Effect`: An optional sound to play from the device's library.
    *   `Restore Row`: If checked, the row will return to its previous state (e.g., the clock) after the duration.

*   **Automation Example: Front Door Alert**
    This automation displays "FRONT DOOR OPEN" on the middle row when the front door is opened. It stays visible for 10 seconds, then the row reverts to the clock.

    ```yaml
    alias: Alert - Front Door Opened
    trigger:
      - platform: state
        entity_id: binary_sensor.front_door_contact
        to: 'on'
    action:
      - service: script.your_display_text_script_name # <-- Change this!
        data:
          row: Middle
          text: FRONT DOOR OPEN
          duration: 10
          effect: Flash
    ```

## Blueprint Guide: Display Entity

This blueprint is the easiest way to show the state or value of any Home Assistant entity on your display.

*   **Inputs:**
    *   `Device ID`, `Display Row`, `Effect`, `Duration`, `Sound`, `Restore Row`: Same as the Display Text blueprint.
    *   `Entity`: The sensor or entity you want to display.
    *   `Prefix / Suffix`: Optional text to add before or after the entity's value (e.g., a "°F" suffix for temperature).

*   **Automation Example: Dynamic Temperature Display**
    This automation runs the script whenever the outside temperature sensor changes, keeping the display on the top row always up-to-date.

    ```yaml
    alias: Update Outside Temperature Display
    trigger:
      - platform: state
        entity_id: sensor.outside_temperature
    action:
      - service: script.your_display_entity_script_name # <-- Change this!
        data:
          row: Top
          entity: sensor.outside_temperature
          suffix: " F"
          restore_row: false # Keep the value on screen
    ```

## Blueprint Guide: Countdown Timer

Perfect for building anticipation for movie night, a gaming session, or just counting down to dinner time.

*   **Inputs:**
    *   `Device ID`, `Display Row`, `Sound`: Same as the other blueprints.
    *   `Duration (seconds)`: The total length of the countdown.
    *   `Prefix`: Optional text to show before the countdown number (e.g., "T-MINUS").
    *   `Completion Text`: A message to display when the countdown hits zero.

*   **Automation Example: Movie Night Countdown**
    When you turn on the "Movie Night" switch, this automation starts a 10-second countdown on the bottom row, ending with the message "SHOWTIME!".

    ```yaml
    alias: Movie Night Countdown
    trigger:
      - platform: state
        entity_id: input_boolean.movie_night
        to: 'on'
    action:
      - service: script.your_countdown_script_name # <-- Change this!
        data:
          row: Bottom
          duration_seconds: 10
          prefix: "STARTING IN"
          completion_text: "SHOWTIME!"
          sound: Time Travel
    ```

---

## Advanced Usage: Built-in Animations

For more cinematic flair, you can trigger the device's spectacular built-in animations directly. These are perfect for special scenes or alerts where you want maximum visual impact.

This is done by calling the `mqtt.publish` service in your automation. You send the name of the animation as the payload.

*   **Automation Example: Trigger Intruder Alert**
    If the alarm is armed and a door opens, this triggers the high-energy "Intruder Alert" sequence.

    ```yaml
    alias: Trigger Intruder Alert on Break-in
    trigger:
      - platform: state
        entity_id: binary_sensor.front_door_contact
        to: 'on'
    condition:
      - condition: state
        entity_id: alarm_control_panel.home_alarm
        state: armed_away
    action:
      - service: mqtt.publish
        data:
          topic: "bttf-time-circuits/YOUR_DEVICE_ID/cmnd/sequencer"
          payload: "Intruder Alert"
    ```

For a complete list of all animation names, see the official developer documentation.

**[Developer Docs: Built-in Animations List](../../docs/developer/sequencer-api.md#built-in-animations)**

---

## FAQ & Troubleshooting

*   **How do I find my Device ID?**
    Your clock's unique Device ID can be found in two places:
    1.  In the device's Web Interface, under **Settings -> Device**.
    2.  It's the same as the `base_topic` you configured in the device's `config.json` file.

*   **Why does my text disappear immediately?**
    This usually happens when using the `SET_TEXT` effect (or no effect at all). This command is non-blocking, meaning it executes and immediately moves on. The blueprint's `Duration (seconds)` input solves this by automatically adding a `WAIT` command. If your text isn't staying visible, ensure you have set a `Duration` greater than zero. Effects like `PULSE`, `MARQUEE`, and `COUNTDOWN` are blocking; they run for their own specific length and do not require an additional `WAIT`.

*   **Can I combine sensor data with my own text?**
    Absolutely! All text fields in the blueprints support Home Assistant templates. This lets you build rich, dynamic strings.

    *Example for the "Display Text" blueprint:*
    ```jinja
    Garage is {{ states('cover.garage_door') }}. Temp: {{ states('sensor.garage_temp') }}°F
    ```

*   **Where can I learn about all the sequencer commands?**
    If you want to go beyond the blueprints and build your own complex sequences from scratch, the full command reference is the place to start.
    **[Developer Docs: Sequencer API Command Reference](../../docs/developer/sequencer-api.md#command-reference)**