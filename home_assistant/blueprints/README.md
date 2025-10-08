# BTTF Time Circuits - Home Assistant Blueprints

Welcome, time traveler! This guide is your flux capacitor for integrating the BTTF Time Circuits clock with Home Assistant. These blueprints are designed to make it incredibly simple to display information, create alerts, and run animations, turning your device from a cool clock into a dynamic, interactive part of your smart home.

Whether you want to see the outside temperature, get a visual alert when the doorbell rings, or run a countdown to movie night, you've come to the right place.

---

## 1. Initial Setup (One-Time Only)

Before using the blueprints, you first need to add your Time Circuits clock to Home Assistant. This is a one-time setup that makes all future interactions seamless.

1.  **Install the Integration**:
    *   **Recommended**: Install via [HACS](https://hacs.xyz/) by searching for "Back to the Future Time Circuits".
    *   **Manual**: Copy the contents of the `custom_components/bttf_time_circuits/` directory into your Home Assistant's `/config/custom_components/bttf_time_circuits/` directory.
2.  **Add the Integration**:
    *   In the Home Assistant UI, go to **Settings** -> **Devices & Services**.
    *   Click the **Add Integration** button.
    *   Search for and select **Back to the Future Time Circuits**.
3.  **Configure Your Device**:
    *   You will be prompted to enter your clock's **Device ID**. This is the unique name you assigned to your clock (e.g., `timecircuits-88mph`).
    *   Click **Submit**. Home Assistant will create a new Device representing your clock, with all its controls and sensors.

With the integration set up, you are now ready to use the blueprints!

---

## 2. Using the Blueprints

### Installation

1.  **Copy Blueprints**: Copy the `.yaml` files from this directory into your Home Assistant's `/config/blueprints/script/` directory. The easiest way is with the Samba Share, FTP, or File Editor add-ons.
2.  **Reload Blueprints**: In the Home Assistant UI, go to **Settings** -> **Automations & Scenes** -> **Blueprints**. Click the three-dot menu in the corner and select **Reload Blueprints**.

### Creating a Script

The blueprints will now appear in your list. Click **Create Script** on the one you want to use. Instead of entering MQTT details, you'll see a single dropdown:

*   **Time Circuits Device**: Simply select your clock from the list.

Fill in the other options (like the text to display) and save it. This script is now ready to be called from your dashboards or, more powerfully, from automations.

---

## Core Concepts: Scripts vs. Automations

To unlock the full potential of your Time Circuits, it's crucial to understand how Home Assistant uses scripts and automations together.

**1. Scripts are "One-Shot" Actions**

Think of a blueprint script as a single, specific mission: "Show this text now." When you run the script, it executes its commands (e.g., display a temperature reading) and then it's done. It doesn't keep running or update automatically.

**2. Automations Provide the Brains**

Automations are the engine of your smart home. They watch for triggers (like a sensor changing, a time of day, or a button press) and then perform actions, such as running one of your blueprint scripts.

> **The Pattern:** You create a **Script** from a blueprint to define *what* you want to happen. Then, you create an **Automation** to decide *when* it should happen.

---

## Blueprint Guides

### Display Text

Your workhorse for displaying any custom message. Perfect for alerts, notifications, and simple status messages.

*   **Inputs:**
    *   `Time Circuits Device`: The clock you want to control.
    *   `Target Row`: Which of the three rows (Top, Middle, Bottom) to use.
    *   `Text`: The message to display. Supports templates!
    *   `Effect`: An optional visual effect (e.g., Pulse, Flash, Marquee).
    *   `Duration`: How long the text should remain visible.
    *   `Restore Row`: If checked, the row returns to its normal state afterward.

### Display Entity

The easiest way to show the state or value of any Home Assistant entity.

*   **Inputs:**
    *   `Time Circuits Device`, `Target Row`, `Effect`, `Duration`, `Restore Row`: Same as the Display Text blueprint.
    *   `Entity`: The sensor or entity you want to display.
    *   `Prefix / Postfix`: Optional text to add before or after the entity's value (e.g., a "°F" postfix for temperature).

### Countdown Timer

Perfect for building anticipation for movie night, a gaming session, or just counting down to dinner time.

*   **Inputs:**
    *   `Time Circuits Device`, `Target Row`: Same as the other blueprints.
    *   `Start Number`: The number to start counting down from.
    *   `End Text`: A message to display when the countdown hits zero.
    *   `Restore Row`: If checked, the row returns to its normal state afterward.

---

## Advanced Usage: Built-in Animations

For more cinematic flair, you can trigger the device's spectacular built-in animations directly. This is done by calling the `mqtt.publish` service in an automation.

*   **Automation Example: Trigger Intruder Alert**
    This automation uses a template to dynamically find the correct MQTT topic for your device.

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
          topic: >
            bttf_time_circuits/{{ device_id('YOUR_DEVICE_ENTITY_ID') }}/cmnd/sequencer
          payload: "Intruder Alert"
    ```
    > **Tip:** Replace `YOUR_DEVICE_ENTITY_ID` with the entity ID of your Time Circuits device (e.g., `switch.time_circuits_flux_capacitor`). You can find this on the device's page in Home Assistant.

For a complete list of all animation names, see the official developer documentation.

**[Developer Docs: Built-in Animations List](../../docs/developer/sequencer-api.md#built-in-animations)**

---

## FAQ & Troubleshooting

*   **How do I find my Device ID?**
    Your clock's unique Device ID is needed **only once** during the initial integration setup. You can find it in the device's Web Interface, under **Settings -> Device**.

*   **Why does my text disappear immediately?**
    This usually happens when using the `SET_TEXT` effect. This command is non-blocking, meaning it executes and immediately moves on. The blueprint's `Duration` input solves this by automatically adding a `WAIT` command. If your text isn't staying visible, ensure you have set a `Duration` greater than zero. Effects like `PULSE`, `MARQUEE`, and `COUNTDOWN` are blocking; they run for their own specific length and do not require an additional `WAIT`.

*   **Can I combine sensor data with my own text?**
    Absolutely! All text fields in the blueprints support Home Assistant templates. This lets you build rich, dynamic strings.

    *Example for the "Display Text" blueprint:*
    ```jinja
    Garage is {{ states('cover.garage_door') }}. Temp: {{ states('sensor.garage_temp') }}°F
    ```

*   **Where can I learn about all the sequencer commands?**
    If you want to go beyond the blueprints and build your own complex sequences from scratch, the full command reference is the place to start.
    **[Developer Docs: Sequencer API Command Reference](../../docs/developer/sequencer-api.md#command-reference)**