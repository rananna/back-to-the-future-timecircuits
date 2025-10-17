# Controlling the Time Circuits with MQTT

Beyond the web interface and Home Assistant integration, you can directly control your Time Circuits device by publishing messages to it using the MQTT protocol. This allows for advanced and custom integrations with any system that can speak MQTT.

This guide assumes you have a basic understanding of MQTT and have an MQTT client, like [MQTT Explorer](http://mqtt-explorer.com/), ready to use.

## Finding Your Device ID

All MQTT topics for your device are unique to that device. You will need to replace `YOUR_DEVICE_ID` in the topics below with the actual ID of your clock.

You can find this ID on the **Connectivity** page of the device's web interface. It is typically the device's MAC address.

## Command Topics

There are two primary topics you can publish commands to:

*   `bttf_time_circuits/YOUR_DEVICE_ID/sequencer/command`: Used for running built-in animations or complex, custom sequences defined in a JSON payload.
*   `bttf_time_circuits/YOUR_DEVICE_ID/display/command`: A simpler topic for writing plain text directly to a display row, bypassing the animation engine.

---

## Example 1: Trigger a Built-in Animation

This is the simplest way to run one of the pre-programmed animations. You just send the name of the animation as a plain text string.

*   **Topic:** `bttf_time_circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload (as plain text/string):** `Intruder Alert`

You can replace `"Intruder Alert"` with any other animation name from the web UI dropdown, such as `"Time Circuits Lock-In"`, `"Knight Rider"`, or `"Party Mode"`.

---

## Example 2: Run a Custom JSON Animation Sequence

For more advanced control, you can define your own animation sequences using a JSON payload. This allows you to combine different commands, target specific rows, and run effects in parallel.

### JSON Structure

A custom sequence is defined by a `targetRow` and a `commands` array.

*   `targetRow`: Specifies which display row to target. This can be an integer (`0` for Top, `1` for Middle, `2` for Bottom) or a descriptive string (`"TOP"`, `"MIDDLE"`, `"BOTTOM"`).
*   `commands`: A JSON array where each object is a step in the sequence. Each step has a `command` name and optional parameters (`intParam`, `intParam2`, `stringParam`).

### Custom Single-Row Animation

This example will write "MQTT ROCKS" to the middle display row and then make it pulse for 10 seconds.

*   **Topic:** `bttf_time_circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload (as JSON):**
    ```json
    {
      "targetRow": "MIDDLE",
      "commands": [
        {
          "command": "SET_TEXT",
          "stringParam": "MQTT ROCKS"
        },
        {
          "command": "PULSE",
          "intParam2": 10000
        }
      ]
    }
    ```

### Complex Parallel Animation

To run animations on multiple rows at the same time, the payload must be a **JSON array**, where each object in the array is a complete sequence definition for a specific `targetRow`.

*   **Topic:** `bttf_time_circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload (as JSON):**
    ```json
    [
      {
        "targetRow": "TOP",
        "commands": [
          {
            "command": "SET_TEXT",
            "stringParam": "I FLASH"
          },
          {
            "command": "FLASH",
            "intParam2": 10000
          }
        ]
      },
      {
        "targetRow": "MIDDLE",
        "commands": [
          {
            "command": "SET_TEXT",
            "stringParam": "I PULSE"
          },
          {
            "command": "PULSE",
            "intParam2": 10000
          }
        ]
      },
      {
        "targetRow": "BOTTOM",
        "commands": [
          {
            "command": "SCANNER",
            "stringParam": "---",
            "intParam": 80,
            "intParam2": 10000
          }
        ]
      }
    ]
    ```

---

## Example 3: Trigger a "Time Travel" Event

This is a special command that first updates the "Last Time Departed" display and then runs the full time travel animation sequence. Note that this command does not use the `targetRow`/`commands` structure.

*   **Topic:** `bttf_time_circuits/YOUR_DEVICE_ID/sequencer/command`
*   **Payload (as JSON):**
    ```json
    {
      "command": "time_travel",
      "lastTimeDeparted": {
        "month": "OCT",
        "day": 26,
        "year": 1985,
        "hour": 1,
        "minute": 22
      }
    }
    ```

---

## Example 4: Write Text Directly to a Display Row

If you want to bypass the animation sequencer entirely and just write text to the display, you can use the direct display command topic. The payload is a plain string, prefixed with the target row (`TOP:`, `MIDDLE:`, or `BOTTOM:`).

*   **Topic:** `bttf_time_circuits/YOUR_DEVICE_ID/display/command`
*   **Payload (as plain text/string):** `MIDDLE:DIRECT WRITE`