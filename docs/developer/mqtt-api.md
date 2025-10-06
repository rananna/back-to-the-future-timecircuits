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

## 2. Raw MQTT Topics API

For advanced users, developers, or integration with systems other than Home Assistant, the clock exposes a powerful, low-level MQTT API. This API allows for direct scripting and control of nearly every feature of the device.

> **Topic Structure**
> All topics follow the format: `bttf-time-circuits/YOUR_DEVICE_ID/COMMAND_NAME/command`.
> You must replace `YOUR_DEVICE_ID` with the actual ID of your device (e.g., its MAC address).

---

### **Command Reference**

#### **Primary Control Topics**

| Command Name | Payload | Description |
| :--- | :--- | :--- |
| `sequencer` | String or JSON | **The most powerful topic.** Triggers a built-in named animation (string payload) or a custom multi-track sequence (JSON payload). See the [**Sequencer API Reference**](./sequencer-api.md) for full details. |
| `display_mode` | String | Sets the main operating mode of the clock. Accepts `"Normal Clock"`, `"Stock Ticker"`, `"Weather"`, or `"Data Link"`. |
| `profile` | String | Applies a pre-configured bundle of settings. Accepts `"Standard"`, `"Cinematic"`, `"Silent Night"`, `"Unstable"`, or `"Custom"`. |
| `override_switch` | `ON` or `OFF` | Enables or disables the full-display override mode. When `ON`, the clock will display the text from the `override_line_X` topics instead of the time. |
| `override_line_1` | String | Sets the 13-character text for the top display row when the override switch is on. |
| `override_line_2` | String | Sets the 13-character text for the middle display row when the override switch is on. |
| `override_line_3` | String | Sets the 13-character text for the bottom display row when the override switch is on. |

---
#### **Settings & Configuration Topics**

| Command Name | Payload | Description |
| :--- | :--- | :--- |
| `brightness` | Number (0-7) | Sets the display brightness. |
| `volume` | Number (0-21) | Sets the audio volume. |
| `24h_format` | `ON` or `OFF` | Toggles 24-hour time format. |
| `sound_toggle` | `ON` or `OFF` | Enables or disables the main time travel animation sounds. |
| `temporal_echo` | `ON` or `OFF` | Toggles the "temporal echo" visual effect after an animation. |
| `animation_interval` | Number | Sets the interval in minutes for the animation to auto-play (0 = off). |
| `animation_duration` | Number | Sets the duration in milliseconds for the main time travel animation. |
| `weather_city` | String | Sets the city for the weather display mode. |
| `stock_refresh` | Number | Sets the refresh interval in minutes for the stock ticker mode (1-60). |

---
#### **Direct Action Topics**

| Command Name | Payload | Description |
| :--- | :--- | :--- |
| `trigger_animation` | `PRESS` | Triggers the main cinematic time travel animation. |
| `reboot_device` | `PRESS` | Reboots the ESP32. |
| `force_ntp_sync` | `PRESS` | Manually forces a time sync with NTP servers. |
| `save_all_settings`| `PRESS` | Saves all current settings from RAM to persistent memory. |
| `factory_reset` | `PRESS` | **Use with caution!** Resets all settings to factory defaults. |
| `weather_refresh` | `PRESS` | Manually triggers a refresh of the weather data. |
| `discover` | `ON` | Triggers the Home Assistant discovery process again. |

---
#### **Direct Display Text Topics**

You can write text to any of the 12 individual display segments. This is a low-level override that is active until cleared with an empty string. The `COMMAND_NAME` is a combination of the row and segment.

*   **Rows**: `dest`, `pres`, `last`
*   **Segments**: `month`, `day`, `year`, `time`

**Example Topic:** `bttf-time-circuits/YOUR_DEVICE_ID/pres_year/command`
*   **Payload**: A string of text to display. The text will be automatically converted to uppercase and truncated to fit the segment.

---
#### **Audio & TTS Topics**

| Command Name | Payload | Description |
| :--- | :--- | :--- |
| `play_sound` | String | Plays one of the built-in sound effects by its filename (e.g., `REMOTE.mp3`). |
| `tts` | String (URL or JSON) | Plays audio from a URL. Can be a raw URL or a JSON object from Home Assistant's `tts.google_translate_say` service (`{"media_id": "URL"}`). |
| `radio` | String (URL or `stop`) | Starts playing an internet radio stream from a URL, or stops the current stream. |
| `radio_stations` | JSON Array | Sends a list of radio stations to the device for use in the web UI. |

---
### **Practical Examples**

Here are some copy-and-paste examples for common actions using `mosquitto_pub`. Remember to replace `YOUR_DEVICE_ID` and `YOUR_BROKER_IP`.

#### **Example 1: Display a "High Temp" Alert**

This example uses the override feature to display a persistent alert across all three rows.

```bash
# Enable the override mode
mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/override_switch/command" -m "ON"

# Set the text for each line
mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/override_line_1/command" -m "  HIGH TEMP  "
mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/override_line_2/command" -m "    ALERT    "
mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/override_line_3/command" -m "  INSIDE     "

# To clear the alert, simply turn the override switch off
mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/override_switch/command" -m "OFF"
```

#### **Example 2: Set the Brightness to Maximum**

```bash
mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/brightness/command" -m "7"
```

#### **Example 3: Trigger the "Lightning" Animation**

This uses the sequencer topic with a string payload.

```bash
mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/sequencer/command" -m "Lightning"
```

#### **Example 4: Play a TTS Message**

This example shows how to send a URL (e.g., from a TTS service) to be played on the clock's speaker.

```bash
# This URL would typically be generated by your home automation system
TTS_URL="http://your-home-assistant:8123/api/tts_proxy/a1b2c3d4e5_google_translate.mp3"

mosquitto_pub -h YOUR_BROKER_IP -t "bttf-time-circuits/YOUR_DEVICE_ID/tts/command" -m "$TTS_URL"
```