# Web Interface Guide

The primary way to configure your Time Circuits clock is through its built-in web interface. This guide provides a complete tour of the UI, explaining every tab and every setting.

## Accessing the Web Interface

You can access the web interface in two ways:

1.  **Using the mDNS/Bonjour Address**: If your computer supports it, simply navigate to `http://timecircuits.local` in your web browser.
2.  **Using the IP Address**: Find the clock's IP address from your router's device list and enter that into your web browser (e.g., `http://192.168.1.123`).

## The Tabbed Layout

The web interface is organized into six tabs for managing all aspects of your clock:

*   **[Time Circuits](./time-circuits.md)**: Control the Destination and Last Time Departed displays and manage presets.
*   **[Temporal Controls](./temporal-controls.md)**: Configure sleep schedules, display brightness, animation sequences, and the sound system.
*   **[Connectivity](./connectivity.md)**: Manage network settings, including MQTT for Home Assistant integration and NTP for time synchronization.
*   **[Data Link](./data-link.md)**: Display external data from stocks, weather, or custom MQTT topics.
*   **[System](./system.md)**: View system status, perform firmware updates, and change the UI theme.
*   **[Help](./help.md)**: An in-app quick reference guide.

---

## Saving Settings

The large **"Save and Engage Time Circuits"** button at the bottom of the page saves all changes. It is disabled by default and will only become active when you change a setting on any tab.

Pressing this button sends all configurations to the device, saves them to memory, and triggers the `Time Circuits Lock-In` animation to confirm the new settings have been applied.
