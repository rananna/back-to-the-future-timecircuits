# MQTT API Reference

This document provides a comprehensive reference for the two ways to interact with the Time Circuits clock via MQTT:
1.  **Home Assistant Integration:** High-level entities and services automatically discovered by Home Assistant. This is the recommended method for most users.
2.  **Advanced MQTT API:** A low-level API for direct control, intended for developers, advanced scripting, or integration with other platforms.

---

## 1. Home Assistant Integration

This integration provides a rich set of entities and device triggers in Home Assistant, created via MQTT auto-discovery.

> **A Note on Entity IDs**
> To ensure that multiple Time Circuits clocks can coexist on the same network, all entities are created with a unique identifier based on the clock's MAC address. Your entity IDs will look something like this: `switch.bttf_tc_123456_override_switch`.
>
> Throughout this document, we use a placeholder format like `switch.YOUR_CLOCK_ID_override_switch`. **You must replace `YOUR_CLOCK_ID` with the actual ID of your device.**

### Entities

Entities are grouped by function to make them easy to find.

#### **Monitoring & Sensors**
*   **`sensor.YOUR_CLOCK_ID_status`**: The primary sensor reporting the clock's state (`Idle`, `Animating`, `Asleep`).
    > **💡 Pro Tip:** This sensor has useful diagnostic attributes like `wifi_rssi`, `free_heap`, and `uptime_seconds`.
*   **`sensor.YOUR_CLOCK_ID_audio_stream_status`**: Shows the state of the audio player (`IDLE` or `PLAYING`).
*   **`binary_sensor.YOUR_CLOCK_ID_is_animating`**: `On` when an animation is playing.
*   **`binary_sensor.YOUR_CLOCK_ID_is_asleep`**: `On` when the clock is in sleep mode.

#### **Direct Display Control**
Twelve `text` entities give you direct, granular control over each segment of the three main displays.
*   **Destination Display**: `text.YOUR_CLOCK_ID_dest_month`, `text.YOUR_CLOCK_ID_dest_day`, `text.YOUR_CLOCK_ID_dest_year`, `text.YOUR_CLOCK_ID_dest_time`
*   **Present Display**: `text.YOUR_CLOCK_ID_pres_month`, `text.YOUR_CLOCK_ID_pres_day`, `text.YOUR_CLOCK_ID_pres_year`, `text.YOUR_CLOCK_ID_pres_time`
*   **Last Departed Display**: `text.YOUR_CLOCK_ID_last_month`, `text.YOUR_CLOCK_ID_last_day`, `text.YOUR_CLOCK_ID_last_year`, `text.YOUR_CLOCK_ID_last_time`

#### **Notifications & Alerts**
*   **`switch.YOUR_CLOCK_ID_override_switch`**: A master switch to enable or disable the override mode.
*   **`text.YOUR_CLOCK_ID_override_line_1`**: Sets the text for the top display row.
*   **`text.YOUR_CLOCK_ID_override_line_2`**: Sets the text for the middle display row.
*   **`text.YOUR_CLOCK_ID_override_line_3`**: Sets the text for the bottom display row.
*   **`select.YOUR_CLOCK_ID_play_sound`**: A dropdown to play one of the pre-defined sound effects on command.

#### **Core Controls**
*   **`select.YOUR_CLOCK_ID_last_departed_preset`**: Choose from movie-based or your custom presets.
*   **`number.YOUR_CLOCK_ID_preset_cycle_interval`**: How often the "Last Time Departed" display cycles through presets (in minutes, 0=off).
*   **`number.YOUR_CLOCK_ID_brightness`**: Controls the display brightness (0-7).
*   **`number.YOUR_CLOCK_ID_volume`**: Adjusts the sound effect volume (0-21).
*   **`switch.YOUR_CLOCK_ID_24h_format`**: Toggles 24-hour time format.
*   **`select.YOUR_CLOCK_ID_display_mode`**: The primary control for the clock's function (`Normal Clock`, `Stock Ticker`, `Weather`, etc.).
*   **`select.YOUR_CLOCK_ID_profile`**: Applies a pre-configured bundle of settings at once (`Standard`, `Cinematic`, etc.).
*   **`time.YOUR_CLOCK_ID_sleep_time`**: Sets the time for the display to turn off automatically.
*   **`time.YOUR_CLOCK_ID_wake_time`**: Sets the time for the display to turn on automatically.

#### **Animation & Effects**
*   **`button.YOUR_CLOCK_ID_trigger_animation`**: Starts the full time travel sequence.
*   **`select.YOUR_CLOCK_ID_animation_style`**: Choose from a curated list of popular animation styles.
*   **`number.YOUR_CLOCK_ID_animation_interval`**: How often to auto-play the animation (in minutes, 0=off).
*   **`number.YOUR_CLOCK_ID_animation_duration`**: Sets the length of the time travel effect.
*   **`switch.YOUR_CLOCK_ID_temporal_echo`**: An experimental "ghosting" effect.

#### **System & Actions**
*   **`button.YOUR_CLOCK_ID_reboot_device`**: Restarts the ESP32.
*   **`button.YOUR_CLOCK_ID_force_ntp_sync`**: Manually syncs the clock with time servers.
*   **`button.YOUR_CLOCK_ID_save_all_settings`**: Saves all current settings to the device's memory.
*   **`button.YOUR_CLOCK_ID_factory_reset`**: **Use with caution!** Resets all settings to factory defaults.

### Device Triggers
The integration also creates several "device triggers" in Home Assistant, which are perfect for starting automations.
*   **Animation Started / Completed**
*   **Sleep Mode Entered / Exited**
*   **Preset Changed**

---

## 2. Advanced MQTT API

For advanced users, the clock exposes a powerful, low-level MQTT API that allows for direct scripting and control.

### Command Sequencer

The command sequencer is a powerful engine that allows you to script complex, multi-step, and even parallel animations. You can create custom alerts, intricate visual effects, and timed sequences by sending a JSON payload or a named command to a single MQTT topic.

This feature is highly capable, with over 20 commands and a dozen pre-programmed named sequences.

**For a complete guide on the sequencer, including all commands, parameters, and examples, please see the [Command Sequencer API Reference](./sequencer-api.md).**

### Manual Display Override

This feature gives you direct, granular control over the text shown on each of the 12 display segments. When you send a command to this endpoint, it will override whatever is currently being shown on that segment (e.g., the time, weather, or stock data) and display your custom text instead.

This override is **persistent** until you clear it by sending an empty string.

*   **MQTT Topic**: `bttf-time-circuits/[DEVICE_ID]/display/manual/command`
*   **Payload**: A JSON object specifying the target and the text.

The JSON payload must contain three fields:
*   `row`: The display row to target (0-2).
*   `segment`: The segment of the row to target (0-3).
*   `text`: The string to display. The text will be automatically converted to uppercase and truncated to fit the segment. To clear an override, send an empty string (`""`).

#### **Example Override**

This example will write the text "FAIL" to the segment that normally shows the current year (middle row, third segment).

*   **Topic**: `bttf-time-circuits/ab12cd34ef56/display/manual/command`
*   **Payload**:
    ```json
    {"row":1, "segment":2, "text":"FAIL"}
    ```

To clear this override and return the segment to its normal function, you would send:

*   **Topic**: `bttf-time-circuits/ab12cd34ef56/display/manual/command`
*   **Payload**:
    ```json
    {"row":1, "segment":2, "text":""}
    ```