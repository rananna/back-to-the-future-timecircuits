# 🔊 Sound System & TTS Guide

The Time Circuits clock features a sophisticated sound system that can play built-in sound effects or generate speech from text, all controllable via MQTT. This guide explains how to use these features to add an audible dimension to your clock.

---

### **Playing Sound Effects**

The clock comes with a variety of pre-loaded sound effects from the movies. You can trigger any of these sounds by publishing a command to a specific MQTT topic.

*   **MQTT Topic**: `bttf-time-circuits/[DEVICE_ID]/audio/play/command`
*   **Payload**: The full path to the sound file you want to play.

The sound files are stored in the device's internal filesystem. To play a sound, you must provide the exact filename, including the leading slash.

**Example:**
To play the "remote control" sound effect, you would send:

*   **Topic**: `bttf-time-circuits/ab12cd34ef56/audio/play/command`
*   **Payload**: `/REMOTE.mp3`

#### **Finding Available Sounds**
A complete list of the available sound files can be found by browsing the `extra sound files/` directory in the project's source code.

---

### **Text-to-Speech (TTS)**

The clock can also convert text into spoken words using a Text-to-Speech (TTS) engine. This is a powerful feature for creating custom announcements or alerts. The audio is streamed from a public web service, so an active internet connection is required.

*   **MQTT Topic**: `bttf-time-circuits/[DEVICE_ID]/audio/tts/command`
*   **Payload**: The text string you want the clock to say.

**Example:**
To make the clock say "Great Scott!", you would send:

*   **Topic**: `bttf-time-circuits/ab12cd34ef56/audio/tts/command`
*   **Payload**: `Great Scott!`

The system will automatically handle connecting to the TTS service, generating the audio, and playing it through the clock's speaker.

---

### **Monitoring Audio State**

The clock reports its current audio state to an MQTT topic, which is useful for knowing when a sound has finished playing.

*   **MQTT Topic**: `bttf-time-circuits/[DEVICE_ID]/audio/state`
*   **Payloads**:
    *   `PLAYING`: Indicates that a sound effect or TTS message is currently playing.
    *   `IDLE`: Indicates that the audio system is not currently active.

You can subscribe to this topic in your automation system to trigger actions after a sound or speech command has completed.