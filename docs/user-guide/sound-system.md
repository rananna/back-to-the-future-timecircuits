# 🔊 Sound System Guide

The Time Circuits clock features a sophisticated sound system that can play built-in sound effects, stream internet radio, and use Text-to-Speech (TTS).

The recommended way to control audio is through the **Home Assistant integration**, which provides a unified `media_player` entity.

### **Table of Contents**
1. [Using the Home Assistant Media Player](#-using-the-home-assistant-media-player)
2. [Available Sound Effects](#-available-sound-effects)
3. [Advanced Control (Raw MQTT)](#-advanced-control-raw-mqtt)

---
## 📢 Using the Home Assistant Media Player

The `media_player.bttf_time_circuits` entity is your central hub for all audio.

### **1. Playing Sound Effects**
You can play any of the built-in sound effects by calling the `media_player.play_media` service with a `media_content_type` of `music`.

**Example:** Play an alarm sound in an automation.
```yaml
- service: media_player.play_media
  target:
    entity_id: media_player.bttf_time_circuits
  data:
    media_content_id: "electric_sparks.mp3"
    media_content_type: "music"
```

### **2. Playing Radio Streams**
Use the same `media_player.play_media` service with a `media_content_type` of `url`.

**Example:** Play an 80s radio station.
```yaml
- service: media_player.play_media
  target:
    entity_id: media_player.bttf_time_circuits
  data:
    media_content_id: "http://d.liveatc.net/kcrw_eclectic" # Example Stream URL
    media_content_type: "url"
```

### **3. Text-to-Speech (TTS)**
Use your favorite TTS service in Home Assistant to make the clock speak.

**Example:** Announce when the washer is done.
```yaml
- service: tts.google_en_com # Or any other TTS service
  data:
    entity_id: media_player.bttf_time_circuits
    message: "Great Scott! The washing machine is finished."
```
---

## 🎶 Available Sound Effects

The following sound files are pre-loaded on the device and can be used with the `media_player.play_media` service. Use the exact filename as the `media_content_id`.

| Filename | Description |
| :--- | :--- |
| `arrival_chime.mp3`| The gentle chime that plays upon arrival. |
| `electric_sparks.mp3`| Generic electrical sounds, used in many animations. |
| `engine_rev.mp3` | The sound of the DeLorean's engine revving. |
| `flux_capacitor_power_on.mp3`| The distinctive hum of the Flux Capacitor powering up. |
| `hum.mp3` | A low, steady electrical hum. |
| `keypad_beeps.mp3`| The sound of the keypad being used to enter a date. |
| `lock_on.mp3` | A confirmation beep. |
| `relay_activation.mp3`| The sound of relays clicking on. |
| `sys_beep.mp3` | A simple system beep. |
| `time_travel.mp3` | The main, iconic time travel sound effect. |

---

## 📻 Finding and Using Radio Streams

The firmware can play most direct, unencrypted MP3 and AAC audio streams. However, it **cannot** play streams from major commercial services like Spotify, Apple Music, or Pandora, which use proprietary or protected formats.

### Where to Find Streams

The best place to find compatible streams is in public internet radio directories. These sites aggregate thousands of free-to-air stations from around the world.

*   **Tip:** When searching, look for a "direct stream link" or "listen link." You may need to right-click a "Play" button and select "Copy Link Address" to get the correct URL.

*   **Recommended Directory:** A great resource is [**Internet-Radio.com**](https://www.internet-radio.com/). It provides direct stream URLs for its listed stations.

### What to Look For

*   **Compatible Formats**: Look for stream URLs that end in `.mp3`, `.aac`, or have a port number (e.g., `http://123.45.67.89:8000/stream`).
*   **Playlist Files (`.m3u`, `.pls`)**: Some stations provide a playlist file. You may need to open this file in a text editor to find the actual stream URL inside.
*   **HTTPS vs. HTTP**: The ESP32 has limited memory for handling secure connections. If a stream is available over both `http://` and `https://`, try the `http://` version first, as it is more likely to work reliably.

---
## ⚙️ Advanced Control (Raw MQTT)

For users not using Home Assistant or for more advanced scripting, you can control the sound system by publishing directly to the device's raw MQTT topics.

**➡️ For a complete list of audio topics and their payloads, please see the [Raw MQTT Topics API Reference](../developer/mqtt-api.md#audio--tts-topics).**