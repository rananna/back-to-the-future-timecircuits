# 🤖 Advanced MQTT Control Guide

Beyond the basic controls exposed in Home Assistant, the Time Circuits clock has a powerful, low-level MQTT API that allows for advanced scripting and direct control over the display. This guide covers two key features: the **Command Sequencer** for creating complex, timed animations, and the **Manual Display Override** for directly writing text to the displays.

These features are intended for advanced users who are comfortable with MQTT and want to create custom automations or integrate the clock with systems other than Home Assistant.

---

### **Command Sequencer**

The command sequencer is a powerful engine that allows you to script complex, multi-step, and even parallel animations. You can create custom alerts, intricate visual effects, and timed sequences by sending a JSON payload or a named command to a single MQTT topic.

This feature is highly capable, with over 20 commands and a dozen pre-programmed named sequences.

**For a complete guide on the sequencer, including all commands, parameters, and examples, please see the [🤖 Command Sequencer Deep Dive](./SEQUENCER.md) documentation.**

---

### **Manual Display Override**

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