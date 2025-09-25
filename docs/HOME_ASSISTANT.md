# 🏠 Home Assistant Integration Guide

This project includes deep, "headless" integration with Home Assistant using the MQTT protocol. This allows you to control every aspect of the clock and use it as a dynamic notification display for your smart home.

This guide will walk you through setup, using blueprints, and finding more advanced information.

### **Table of Contents**
1. [Getting Started: Setup & Blueprints](#getting-started-setup--blueprints)
2. [Guide to Using Blueprints](#guide-to-using-blueprints)
3. [Troubleshooting](#troubleshooting)
4. [Where to Go Next](#where-to-go-next)

---

## Getting Started: Setup & Blueprints

### **Step 1: Prerequisites**
> Before you begin, please ensure you have the following:
> * A running Home Assistant instance.
> * A configured and running MQTT broker that is connected to Home Assistant.
> * The Time Circuits Clock is powered on and connected to your Wi-Fi network.

### **Step 2: Connect the Clock to MQTT**
Setting up the connection is straightforward.
1.  Open the clock's web interface and navigate to the **Data Link** tab.
2.  Enter your MQTT broker's details (IP address, port, and credentials).
3.  Press the **"Engage Time Circuits"** button to save the settings.

The device will now use **MQTT auto-discovery** to announce itself to your Home Assistant instance. A new device named "**Time Circuits Display**" will automatically appear in your MQTT integration.

### **Step 3: Install the Blueprints**
Blueprints are the easiest way to create powerful automations.
1.  In your Home Assistant configuration directory, find the `config/blueprints/automation` folder. If it doesn't exist, create it.
2.  Copy the `.yaml` files from the `home-assistant` directory of this project into that `blueprints/automation` folder.
3.  Reload your automations in Home Assistant by navigating to **Developer Tools > YAML Configuration** and clicking the "Automations" button.

---

## Guide to Using Blueprints

Once installed, the Time Circuits blueprints will be available when you create a new automation (**Settings > Automations & Scenes**).

Many of the included blueprints are **"callable,"** meaning they are on-demand actions that you call from your own automations. This provides maximum flexibility. A common pattern is:
1.  **Your Automation's Trigger:** A sensor changes, a specific time is reached, etc.
2.  **Your Automation's Action:** Call the desired Time Circuits blueprint.

### **Popular Blueprints**

*   **BTTF - Advanced Notifier**: Display a temporary, multi-line message on the clock, with an optional sound effect. Perfect for alerts like "Mailbox" or "Door Open."
*   **BTTF - Home Assistant Status Display**: Use the main displays as a highly customizable, 12-segment status panel for your smart home. Show temperatures, humidity, or any other sensor value.
*   **BTTF - TTS Notifier**: Play audio announcements from Home Assistant's Text-to-Speech (TTS) services on the clock's speaker.
*   **BTTF - Cinematic Scene Trigger**: A complete automation that sets a destination year and immediately triggers the full time travel animation sequence.
*   **BTTF - Sequencer**: A powerful tool for creating custom, multi-step animations. You can flash specific display segments, play sounds, and show temporary messages in a coordinated sequence. This is perfect for building unique alerts, like a visual alarm for an intruder warning.

    *Example*: Create a script that flashes the "Destination Year" display, plays an alarm, and shows "INTRUDER ALERT" when a door sensor is triggered.

*   **BTTF - Dynamic Marquee Display**: Show a scrolling line of text on one of the five data link display slots. It supports Home Assistant's templating engine, allowing you to display dynamic information like "Outside temp is 21°C."

    *Example*: Set up an automation that updates a marquee every 5 minutes with the current temperature from a weather sensor.

*   **BTTF - Radio Streamer**: Play an internet radio stream through the clock's speaker. You can start a stream by providing a URL or stop it with a simple command.

    *Example*: Create a scene that starts your favorite 80s radio station stream when you say "Hey Google, it's 80s time."

---

## Troubleshooting

If you encounter issues, here are some common solutions:

> ⚠️ **Device Not Appearing in Home Assistant?**
> * Double-check the MQTT broker IP, port, and credentials in the clock's web UI.
> * Verify that "Enable discovery" is turned on for your MQTT integration in Home Assistant.
> * Use a tool like [MQTT Explorer](http://mqtt-explorer.com/) to see if the clock is publishing topics under `homeassistant/`.

> ⚠️ **Entities are 'Unavailable'?**
> * Check the clock's Wi-Fi connection.
> * In MQTT Explorer, check the `BTTF_TC/<UNIQUE_ID>/status` topic. It should have a retained message of `online`.

---

## Where to Go Next

*   **[Home Assistant Advanced Guide](guides/HOME_ASSISTANT_ADVANCED.md)**: For advanced automation examples, dashboard configurations, and deep dives into the integration's features.
*   **[MQTT Entity Reference](reference/MQTT_ENTITIES.md)**: A complete list of all available Home Assistant entities and device triggers for the clock.
*   **[30+ Automation Examples](../HOME_ASSISTANT_EXAMPLES.md)**: Get inspired with a huge list of creative ideas.
*   **[Developer's Guide](../DEVELOPMENT.md)**: For a full technical breakdown of the firmware and MQTT implementation.