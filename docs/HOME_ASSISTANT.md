# 🏠 Home Assistant Integration Guide

This project includes deep, "headless" integration with Home Assistant using the MQTT protocol. This allows you to control every aspect of the clock and use it as a dynamic notification display for your smart home.

## Setup

1.  **Configure MQTT**: In the clock's web interface, go to the **Data Link** tab and enter the IP address, port, and credentials for your MQTT broker.
2.  **Auto-Discovery**: Once saved, the clock will automatically announce itself to your Home Assistant instance. A new device named "Time Circuits Display" will appear in your MQTT integration.

<p align="center">
  <img src="../images/ha_screenshot.png" alt="Home Assistant Screenshot" width="800">
</p>

## Available Entities

The integration creates a rich set of entities for complete control.

#### Configuration Entities
* **`select.bttf_timecircuits_01_profile`**: Select an on-device profile (`Standard`, `Cinematic`, `Silent Night`, `Unstable`) to instantly apply a bundle of pre-configured settings.
* **`number.bttf_timecircuits_01_destination_year`**: Set the destination year.
* **`number.bttf_timecircuits_01_brightness`**: Control the display brightness.
* **`number.bttf_timecircuits_01_volume`**: Adjust the sound effect volume.
* **`select.bttf_timecircuits_01_animation_style`**: Choose the visual style for the time travel animation.
* `...and many more for every setting in the web UI.`

#### Control & Action Entities
* **`text.bttf_timecircuits_01_dest_year` (and 11 others)**: Granular entities to control the text of all **12 individual display segments**.
* **`text.bttf_timecircuits_01_run_sequence`**: A powerful entity that accepts a simple script to run perfectly timed, non-blocking audio-visual sequences on the device.
* **`text.bttf_timecircuits_01_marquee_temp_override`**: Send a temporary, scrolling message with a duration (in JSON format, e.g., `{"text": "ALERT", "duration": 60}`).
* **`button.bttf_timecircuits_01_trigger_animation`**: A button to start the full time travel sequence.
* **`button.bttf_timecircuits_01_reboot_device`**: A button to restart the clock.

#### Sensor Entities
* **`sensor.bttf_timecircuits_01_status`**: The main sensor that reports the clock's current state (`Idle`, `Animating`, `Asleep`) and exposes a rich set of diagnostic data as attributes (`WiFi Strength`, `Uptime`, `IP Address`, etc.).
* **`binary_sensor.bttf_timecircuits_01_is_animating`**: A binary sensor that is `on` when an animation is playing.

---

## 🚀 Home Assistant Blueprints

To make the most powerful features easy to use, this project includes several Home Assistant Blueprints. Copy the `.yaml` files from the `home-assistant` directory of this project into your Home Assistant `blueprints/automation` folder.

| Blueprint | Description |
| :--- | :--- |
| **Advanced Notifier** | A user-friendly UI to create perfectly timed audio-visual alerts using the on-device sequencer. |
| **Dynamic Data Display** | Easily display the state of any Home Assistant sensor on one of the marquee slots. |
| **Cinematic Scene Trigger** | A simple way to set a destination year and trigger the full time travel animation. |

---

## 💡 Example Automations

For a list of useful and creative automations, please see the dedicated examples file:

**➡️ [View Home Assistant Automation Examples](../HOME_ASSISTANT_EXAMPLES.md)**