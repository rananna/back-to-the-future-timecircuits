# 🎮 Home Assistant: Core Entities & Controls

The integration creates a device with a rich set of entities for granular control. You can use these in your own automations if you prefer not to use the blueprints. This section provides a detailed look at each entity.

### **Selects (Dropdowns)**

#### **`select.display_mode`**
Sets the main operating mode of the clock. The text for these modes is **static** and will remain on the display.
*   **`Normal Clock`**: The default mode. Displays the current time and date.
*   **`Stock Ticker`**: Displays data for a configured stock symbol.
*   **`Weather`**: Displays current weather information from a configured source.
*   **`Data Link`**: Displays custom, user-defined data points configured in the web UI.

**Example Automation:** Change the display to show the weather every morning at 8 AM.
```yaml
trigger:
  - platform: time
    at: "08:00:00"
action:
  - service: select.select_option
    target:
      entity_id: select.bttf_time_circuits_display_mode
    data:
      option: "Weather"
```

#### **`select.default_animation_sequence`**
Sets the animation that plays when you click the "Save and Engage Time Circuits" button in the device's web UI. This does not affect automations or scripts from Home Assistant.

#### **`select.run_animation`**
This is a special "fire-and-forget" dropdown. Selecting any animation from this list will immediately trigger it on the device. It's designed for quick manual tests or for use in automations where you want to run a specific, self-contained animation. The display will automatically revert to its previous state after the animation completes.

**Example Script:** Create a script that plays the "Intruder Alert" animation, which you can then call from an automation.
```yaml
alias: Time Circuits - Intruder Alert
sequence:
  - service: select.select_option
    target:
      entity_id: select.bttf_time_circuits_run_animation
    data:
      option: "Intruder Alert"
mode: single
```

---
### **Switches**

#### **`switch.override_switch`**
This is a master switch that allows you to take manual control of the display. When this switch is **ON**, the clock's normal display mode is ignored, and the device will instead show whatever text you have set in the `text` entities (see below). This is powerful for creating persistent, custom status displays. The text is **static** and will remain until the switch is turned **OFF**.

#### **`switch.24h_format`**
Toggles the main clock display between 12-hour and 24-hour time formats.

---
### **Numbers (Sliders)**

#### **`number.brightness`**
Adjusts the brightness of the LED displays.
*   **Range:** `0` (dimmest) to `7` (brightest).

#### **`number.stock_refresh`**
Sets the refresh interval for the stock ticker mode.
*   **Range:** `1` to `60` minutes.

---
### **Buttons**

These entities perform a single action when pressed.

*   **`button.time_travel`**: Starts the full, iconic "Time Travel" animation sequence with sound.
*   **`button.favorite_radio_station`**: Plays the favorite radio station you have configured in the device's web UI. Pressing it again while playing will stop the stream.
*   **`button.reboot_device`**: Restarts the clock.
*   **`button.force_ntp_sync`**: Manually forces the clock to synchronize its time with an internet time server.
*   **`button.factory_reset`**: Resets all device settings to their defaults.
*   **`button.refresh_weather_data`**: Manually fetches the latest data for the weather display mode.

---
### **Text Inputs**

These entities allow you to write custom, **static** text directly to the display. This text will **only be shown when the `switch.override_switch` is turned ON**.

There are two types of text entities:

1.  **Individual Segments (`text.destination_year`, `text.present_month`, etc.)**
    These 12 entities correspond to each of the individual display blocks (e.g., the 4-digit year, the 3-character month). They are useful for setting specific parts of the display.

2.  **Full Row Overrides (`text.override_line_1`, `text.override_line_2`, `text.override_line_3`)**
    These 3 entities allow you to write a string of up to 13 characters to an entire row, giving you more flexibility than the individual segments. Line 1 corresponds to the top row (Destination Time), Line 2 to the middle (Present Time), and Line 3 to the bottom (Last Time Departed). The text will be displayed exactly as you type it. For text shorter than 13 characters, it will be left-justified.

**Example Script:** Create a custom "Welcome Home" message that displays static text on all three rows.
```yaml
alias: Time Circuits - Welcome Home Message
sequence:
  # Set the text for each line
  - service: text.set_value
    target:
      entity_id: text.bttf_time_circuits_override_line_1
    data:
      value: "WELCOME HOME"
  - service: text.set_value
    target:
      entity_id: text.bttf_time_circuits_override_line_2
    data:
      value: "SYSTEMS ARE"
  - service: text.set_value
    target:
      entity_id: text.bttf_time_circuits_override_line_3
    data:
      value: "ALL GREEN"
  # Turn on the override switch to display the text
  - service: switch.turn_on
    target:
      entity_id: switch.bttf_time_circuits_override_switch
mode: single
```

To turn the message off and return to the normal clock, you would simply call `switch.turn_off` on the `switch.override_switch`.

---
### **Other Entities**

*   **`sensor.status`**: Monitors the clock's current state (e.g., `Idle`, `Animating`) and has useful attributes like `free_heap` and `wifi_rssi`.
*   **`update.firmware`**: Notifies you when a new firmware version is available and allows for one-click OTA updates from the Home Assistant UI.