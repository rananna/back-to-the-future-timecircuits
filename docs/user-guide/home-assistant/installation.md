# 🚀 Installation Guide

This guide provides a detailed, step-by-step walkthrough for installing and configuring the Home Assistant integration.

## 🛑 Prerequisites

*   A running Home Assistant instance.
*   [HACS](https://hacs.xyz/) (Home Assistant Community Store) installed.
*   **MQTT Broker Configured in Home Assistant**: You must have the MQTT integration set up and connected to your broker. The Time Circuits clock does not connect directly to HA, but to your MQTT broker.
*   **Clock Connected to WiFi**: The Time Circuits Clock must be powered on and connected to your Wi-Fi network.
*   **Clock Configured for MQTT**: In the clock's web UI, ensure the MQTT broker details are correctly configured under the **Connectivity** tab.

## Step 1: Install the Custom Component via HACS

1.  In Home Assistant, navigate to **HACS > Integrations**.
2.  Click **Explore & Download Repositories**.
3.  Search for "Back to the Future Time Circuits" and install it.
4.  Restart Home Assistant as prompted.

## Step 2: Add the Integration in Home Assistant

1.  Navigate to **Settings > Devices & Services**.
2.  Click **Add Integration** and search for "**Back to the Future Time Circuits**".
3.  You will be prompted for your clock's **Device ID**. You can find this in the clock's web interface under **System -> System Status**.
4.  Click **Submit**.

Your Time Circuits clock will now appear as a new device in Home Assistant, with all its entities automatically created and ready to use.

## Step 3: Importing the Blueprints

The easiest way to add the blueprint is by importing it directly from the project's GitHub repository. This ensures you always have the most up-to-date version.

1.  **Navigate to Blueprints in Home Assistant**: Go to **Settings > Automations & Scenes** and select the **Blueprints** tab.
2.  **Import a Blueprint**: Click the **Import Blueprint** button in the bottom right corner.
3.  **Paste the URL**: In the dialog box, paste the URL below into the "URL of the Blueprint to import" field.
4.  **Preview and Import**: Click **Preview Blueprint**. Home Assistant will show you the details. If it looks correct, click **Import Blueprint**.

#### **Blueprint URL (Click to Copy)**

*   **Display Blueprint:**
    ```
    https://github.com/rananna/back-to-the-future-timecircuits/blob/main/home_assistant/blueprints/display.yaml
    ```
