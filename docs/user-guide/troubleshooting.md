# 🔬 Troubleshooting Guide

This guide provides solutions to common issues you might encounter while building or using your Time Circuits display.

### **Table of Contents**
1. [Hardware & Wiring Issues](#hardware--wiring-issues)
2. [Software & Firmware Issues](#software--firmware-issues)
3. [Connectivity Issues](#connectivity-issues)
4. [Data Link & API Issues](#data-link--api-issues)

---

## Hardware & Wiring Issues

#### **Problem: One or more displays are blank or not working.**

This is the most common issue and is almost always caused by an I2C address conflict or a wiring problem.

*   **Solution 1: Check I2C Addresses**
    *   Carefully re-read the **[I2C Display Addresses](INSTALLATION.md#step-3-set-i2c-display-addresses)** section in the installation guide.
    *   Ensure that you have set the solder bridges on the back of each of the 12 displays correctly. Even a single incorrect address can cause the entire I2C bus to fail.
    *   Remember that the "Destination" and "Present" displays share one I2C bus, and the "Last Time Departed" displays are on a separate bus.

*   **Solution 2: Check Wiring**
    *   Verify that the SDA and SCL lines for each I2C bus are connected to the correct pins on the ESP32.
    *   Ensure all displays have a solid connection to power (+5V) and ground (GND).
    *   Check for any loose connections or bad solder joints.

#### **Problem: The AM/PM LEDs are not lighting up.**

*   **Solution 1: Check Polarity**
    *   LEDs are diodes, meaning they only allow current to flow in one direction. Ensure the longer leg (the anode) is connected to the ESP32's GPIO pin and the shorter leg (the cathode) is connected to the resistor and then to ground.

*   **Solution 2: Check Resistor Value**
    *   Make sure you are using a current-limiting resistor (220-330Ω is recommended) in series with each LED. Without a resistor, the LED may burn out.

#### **Problem: No sound is coming from the speaker.**

*   **Solution 1: Check I2S Wiring**
    *   The I2S amplifier has several pins (DIN, BCLK, LRC). Double-check that they are all wired to the correct GPIO pins on the ESP32 as specified in the **[Wiring & Schematics](INSTALLATION.md#-wiring--schematics)**.

*   **Solution 2: Check SD Pin**
    *   The I2S amplifier's "SD" (Shutdown) pin must be connected to the correct GPIO pin on the ESP32. If this pin is left floating, the amplifier will remain in a low-power shutdown state.

*   **Solution 3: Check Volume**
    *   In the web interface, navigate to the **Temporal Controls** tab and ensure the sound volume is not set to zero.

---

## Software & Firmware Issues

#### **Problem: The "Custom (partitions.csv)" option is missing in the Arduino IDE.**

*   **Solution:** This happens when the IDE cannot find the `partitions.csv` file.
    *   Ensure that `partitions.csv` is in the **exact same folder** as your `back-to-the-future-timecircuits.ino` file.
    *   Restart the Arduino IDE after moving the file.

#### **Problem: The web interface is not loading or looks broken.**

*   **Solution:** This means the data files (HTML, CSS, JS) were not uploaded correctly.
    *   Follow the **[Upload Files to Filesystem](INSTALLATION.md#step-5-upload-files-to-filesystem)** instructions carefully.
    *   You must use the "ESP32 LittleFS Data Upload" tool, not the standard "Upload" button, for the data files.

#### **Problem: Arduino IDE fails to compile the code.**

*   **Solution 1: Install All Libraries**
    *   Ensure you have installed all seven required libraries from the Library Manager and the one manual library (`ESP32-audioI2S`) as described in the **[Install Required Libraries](INSTALLATION.md#step-2-install-required-libraries)** section.

*   **Solution 2: Check Board Selection**
    *   Make sure you have selected the **"ESP32-S3 Dev Module"** from the **Tools > Board** menu. This specific board is required for the project.

---

## Connectivity Issues

#### **Problem: The "BTTF-Clock-Setup" WiFi hotspot is not appearing.**

*   **Solution:** This can happen if the device was previously connected to a network that is no longer available.
    *   The device will automatically enter setup mode if it fails to connect after a few minutes.
    *   To force setup mode, you can perform a full reset by navigating to the **System** tab in the web UI and clicking "Reset All Settings." If you cannot access the UI, you will need to re-flash the firmware and erase the device's memory.

#### **Problem: The clock is not connecting to my WiFi network.**

*   **Solution 1: Check Credentials**
    *   Double-check that you entered the correct WiFi password in the setup portal.
    *   2.4GHz vs 5GHz: Ensure your WiFi network is a 2.4GHz network, as the ESP32 does not support 5GHz.

*   **Solution 2: Check Signal Strength**
    *   The ESP32 may have a weak signal. Try moving the clock closer to your WiFi router. You can check the signal strength (RSSI) in the **System** tab of the web UI.

#### **Problem: I can't access the web interface at `http://bttf-clock.local`.**

*   **Solution:** This is an mDNS issue, which can sometimes be unreliable.
    *   Find the clock's IP address in your router's client list or by monitoring the Serial Monitor in the Arduino IDE during boot.
    *   Access the web interface using the IP address directly (e.g., `http://192.168.1.123`).

---

## Data Link & API Issues

#### **Problem: Weather or Stock data is not loading.**

*   **Solution 1: Check Internet Connection**
    *   Ensure the clock is connected to a WiFi network with a working internet connection.

*   **Solution 2: Check API Keys**
    *   The Stock Ticker mode requires a valid API key from Financial Modeling Prep. Ensure the key is entered correctly in the **Data Link** tab. An incorrect or expired key will cause data fetches to fail. The error will be shown in the web UI.

*   **Solution 3: API Rate Limits**
    *   Free API services have usage limits. If you refresh the data too often, you may be temporarily rate-limited. The UI shows the number of API calls made for the day. Wait for the limit to reset (usually daily).

#### **Problem: Home Assistant entities are "unavailable."**

*   **Solution 1: Check MQTT Broker Settings**
    *   Verify the MQTT broker IP, port, and credentials are correct in the **Data Link** tab of the web UI.
    *   Ensure your MQTT broker is running and accessible from the clock.

*   **Solution 2: Check MQTT Discovery**
    *   In Home Assistant, go to **Settings > Devices & Services > MQTT**. Click **Configure** and ensure "Enable discovery" is checked.
    *   Use a tool like MQTT Explorer to verify the clock is publishing discovery messages to the `homeassistant/` topic.