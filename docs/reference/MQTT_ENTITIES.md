#  MQTT Entities & Device Triggers Reference

This document provides a complete reference for all the Home Assistant entities and device triggers created by the Time Circuits clock via MQTT auto-discovery.

> ### A Note on Entity IDs
> To ensure that multiple Time Circuits clocks can coexist on the same network, all entities are created with a unique identifier based on the clock's MAC address. This means your entity IDs will look something like this:
>
> *   `switch.bttf_tc_123456_override_switch`
> *   `text.bttf_tc_123456_override_message`
>
> Throughout this document, we will use a placeholder format like `switch.YOUR_CLOCK_ID_override_switch`. **You must replace `YOUR_CLOCK_ID` with the actual ID of your device.**

---

## Entities

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

*   **`switch.YOUR_CLOCK_ID_weather_mode`** (Friendly name: `Live Weather Mode`): Toggles the live aweather display on or off.
*   **`text.YOUR_CLOCK_ID_weather_city`** (Friendly name: `Weather City`): Sets the city for which to retrieve weather data. After setting a new city, you may need to press the refresh button to see the change.
*   **`button.YOUR_CLOCK_ID_refresh_weather_data`** (Friendly name: `Refresh Weather Data`): Forces an immediate refresh of the weather data for the currently configured city. This is only active after a city has been successfully looked up from the web interface at least once.

#### **System & Actions**
*   **`button.YOUR_CLOCK_ID_reboot_device`**: Restarts the ESP32.
*   **`button.YOUR_CLOCK_ID_force_ntp_sync`**: Manually syncs the clock with time servers.
*   **`button.YOUR_CLOCK_ID_save_all_settings`**: Manually triggers a save of all current settings to the device's memory.
*   **`button.YOUR_CLOCK_ID_factory_reset`**: **Use with caution!** Resets all settings to their original factory defaults.

---

## Device Triggers
The integration also creates several "device triggers" in Home Assistant, which are perfect for starting automations based on the clock's activity.

*   **Animation Started**: Fires when any time travel animation begins.
*   **Animation Completed**: Fires when an animation finishes.
*   **Sleep Mode Entered**: Fires when the clock enters its scheduled sleep mode.
*   **Sleep Mode Exited**: Fires when the clock wakes up.
*   **Preset Changed**: Fires when the "Last Time Departed" display cycles to a new preset.