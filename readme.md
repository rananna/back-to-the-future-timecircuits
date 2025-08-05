# Back to the Future - ESP32 Time Circuits Display

<p align="center">
  <img alt="License" src="https://img.shields.io/badge/License-MIT-yellow.svg">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-ESP32-purple.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Arduino-00979D.svg">
  <img alt="Power" src="https://img.shields.io/badge/Power-1.21_Gigawatts!-orange.svg">
</p>

<p align="center">
  <img src="20250722_134713.jpg" alt="BTTF Clock Prop" width="800">
</p>

> **Great Scott!** You've found the schematics for a fully-functional, WiFi-enabled Time Circuits display. While it can't *actually* travel through time (the flux capacitor technology is still a bit tricky), it brings the iconic look, feel, and sounds of the DeLorean's dashboard right to your desk. Using an ESP32, 12 alphanumeric displays, and a little bit of 1.21-gigawatt... I mean, 5-volt... ingenuity, this display connects to your network to show the Destination Time, Present Time, and Last Time Departed, all fully configurable from a slick, mobile-friendly web interface.

---

### Who is this for?

* **BTTF Fans & Prop Makers**: If you love the movies and want a highly accurate and interactive prop, this is for you.
* **Electronics Hobbyists**: A fantastic project to practice soldering, wiring, and ESP32 programming.
* **Beginners**: The guide is written to be as clear as possible, but some experience with Arduino and soldering is recommended.
* **Advanced Makers**: The project is a great base for customization. Add more animations, IoT integrations, or your own unique features!

## Features

<p align="center">
  <img src="webui.png" alt="Web UI Screenshot" width="800">
</p>
*[Image: A screenshot of the web interface showing the three time circuit displays and various settings tabs.]*

* **Three-Row BTTF Display Layout**: Three full rows of displays for Destination Time, Present Time, and Last Time Departed.
* **Accurate & Automatic Time**:
    * **NTP Synchronization**: Automatically fetches time from NTP servers for perfect accuracy.
    * **Full Time Zone Support**: Includes automatic Daylight Saving Time adjustments based on a predefined list of timezones.
* **Complete Web Interface**:
    * **Thematic Header**: A screen-accurate header that displays all three times in real-time.
    * **Live Preview Mode**: See changes on the physical clock instantly as you adjust settings.
    * **WiFi Manager**: Simple initial WiFi setup using a captive portal.
* **Authentic Experience**:
    * **Physical Time Travel Animations**: Trigger a physical animation where all displays flicker with random dates and times.
    * **Sound Effects**: An integrated DFPlayer Mini MP3 module plays iconic movie sounds for events.
    * **Multiple Animation Styles**: Choose from several animation styles, including "Sequential", "Random", "Wave", and a "Glitch" effect.
* **Customization & Convenience**:
    * **Live Speedometer Mode**: Switch the "Last Time Departed" row into a real-time speedometer showing the wind speed for your location.
    * **Power Saving "Sleep" Mode**: Displays automatically turn off during user-defined hours.
    * **Over-the-Air (OTA) Updates**: Update the firmware wirelessly over your WiFi network.
    * **Web UI Themes**: Change the color scheme of the web interface.

---

## Doc's Component Checklist (BOM)

| Category | Component | Qty |
| :--- | :--- | :--: |
| **Microcontroller** | [ESP32 Dev Module](https://www.aliexpress.com/item/1005006212080137.html) | 1 |
| **Audio** | [DFPlayer Mini MP3 Module](https://www.aliexpress.com/item/1005008228039985.html) | 1 |
| | [MicroSD Card (1GB+)](https://www.aliexpress.com/item/1005008978876553.html) | 1 |
| | [Small 8 Ohm Speaker](https://www.aliexpress.com/item/1005006682079525.html) | 1 |
| **Displays** | **Adafruit HT16K33 14-Segment Alphanumeric Displays** | **12** |
| **Indicators** | [5mm LEDs (Any Color)](https://www.aliexpress.com/item/1005003912454852.html) | 6 |
| **Passive Comp.** | [220-330Ω Resistors](https://www.aliexpress.com/item/1005002091320103.html) | 6 |
| **Prototyping** | [Dupont Jumper Wires](https://www.aliexpress.com/item/1005003641187997.html) | 1 set|
| | 5V Power Supply (2A+) | 1 |

---

## Time Circuit Schematics (Wiring)

This project uses two separate I2C buses to manage all 12 displays. This simplifies wiring but requires careful attention to the I2C addresses.

### Visual Component Overview