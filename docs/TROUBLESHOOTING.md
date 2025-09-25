# 🔧 Troubleshooting Guide

This guide provides solutions to common problems you might encounter with your Time Circuits display.

### **Table of Contents**
1. [Display & Hardware Issues](#display--hardware-issues)
2. [WiFi & Network Connectivity](#wifi--network-connectivity)
3. [Data Fetching Errors (Weather/Stocks)](#data-fetching-errors-weatherstocks)
4. [Web Interface Problems](#web-interface-problems)

---

## Display & Hardware Issues

#### **Problem: One or more display segments are not lighting up.**

*   **Solution 1: Check I2C Addresses.** This is the most common cause of display failures. Each of the 12 display modules must have a unique I2C address configured by soldering the address jumpers on the back. Carefully review the address table in the **[Installation Guide](INSTALLATION.md#step-3-set-i2c-display-addresses)** and ensure each display is soldered correctly. An unsoldered bridge or an accidental solder splash can cause address conflicts.

*   **Solution 2: Verify Wiring.** Double-check all wiring, especially the SDA and SCL lines for both I2C buses. Ensure that the "Destination" and "Present" displays are on one bus, and the "Last Time Departed" displays are on the other. A single incorrect connection can cause the entire bus to fail.

*   **Solution 3: Check Power.** Ensure your 5V power supply is providing at least 2A. Insufficient power can cause displays to behave erratically or not light up at all, especially at higher brightness levels.

#### **Problem: The displays show `INIT FAIL` or `HW INIT FAILED` on boot.**

*   **Cause**: This message indicates that the ESP32 was unable to communicate with one or more of the I2C display modules when it first powered on.

*   **Solution**: The firmware is designed to be resilient to this. It will automatically retry the hardware initialization every 30 seconds. Often, this issue can be caused by a temporary power fluctuation. If the message persists, it points to a more permanent hardware problem. Re-check your I2C addresses and wiring as described above.

#### **Problem: The AM/PM indicator LEDs are not working.**

*   **Solution 1: Check LED Polarity.** LEDs are diodes and only work in one direction. Ensure the longer leg (the anode) is connected to the ESP32's GPIO pin and the shorter leg (the cathode) is connected to the resistor and then to Ground.

*   **Solution 2: Verify Resistor.** Make sure you are using a current-limiting resistor (220-330Ω is recommended) for each LED. Connecting an LED directly to a GPIO pin without a resistor can burn it out instantly.

---

## WiFi & Network Connectivity

#### **Problem: The `BTTF-Clock-Setup` WiFi hotspot does not appear.**

*   **Solution**: The hotspot is only created if the device fails to connect to a previously configured WiFi network after 15 seconds. If you have successfully configured WiFi before, the device will try to connect to that network. To force the setup portal to appear, you can either:
    1.  Temporarily turn off your home WiFi router.
    2.  Use the "Reset to Factory Defaults" option in the web interface's **System** tab, which will erase all saved settings, including WiFi credentials.

#### **Problem: The clock doesn't connect to my WiFi network.**

*   **Solution 1: Check Credentials.** Use the setup portal to carefully re-enter your WiFi SSID and password. Remember that both are case-sensitive.

*   **Solution 2: Network Compatibility.** The ESP32 works best with 2.4GHz WiFi networks. It does not support 5GHz networks. Ensure your router is broadcasting a 2.4GHz signal.

#### **Problem: I can't access the web interface at `http://bttf-clock.local`.**

*   **Cause**: This `mDNS` address relies on your router and computer supporting it. Some network configurations or firewalls can block it.

*   **Solution 1: Use the IP Address.** The most reliable way to connect is by using the clock's IP address directly. You can find this in your router's list of connected devices or by monitoring the output in the **Arduino IDE's Serial Monitor** when the clock boots up.

*   **Solution 2: Check Network Isolation.** Some "Guest" WiFi networks have a feature called "Client Isolation" that prevents devices on the network from communicating with each other. Ensure this feature is disabled for the WiFi network the clock is connected to.

---

## Data Fetching Errors (Weather/Stocks)

#### **Problem: The Weather or Stock display shows `CONNECTION FAILED` or `API FAILED`.**

*   **Cause**: This is a general error indicating the clock could not reach the required API server over the internet.

*   **Solution 1: Check WiFi Connection.** Ensure the clock is successfully connected to your WiFi network and has internet access.

*   **Solution 2: Firewall Issues.** If you have a firewall on your network (like Pi-hole or a custom router setup), make sure it is not blocking access to the following domains:
    *   **Weather**: `api.open-meteo.com`
    *   **Stocks**: `financialmodelingprep.com`

#### **Problem: The Stock Ticker shows `INVALID API KEY`.**

*   **Solution**: This means the API key you entered for the Financial Modeling Prep service is incorrect, has expired, or has been disabled.
    1.  Log in to your account on the [Financial Modeling Prep website](https://site.financialmodelingprep.com/login).
    2.  Navigate to your dashboard and copy your API key again.
    3.  Paste the new key into the "Financial Modeling Prep API Key" field in the clock's web interface and save your settings.

#### **Problem: The Stock Ticker shows `RATE LIMITED`.**

*   **Cause**: The free tier of the Financial Modeling Prep API has a daily limit on the number of requests you can make. You have exceeded this limit.
*   **Solution**: The system will automatically stop trying and will resume fetching data the next day. To avoid this, you can increase the "Refresh Interval" in the Stock Ticker settings to a higher value (e.g., 30 or 60 minutes) to make fewer requests per day.

---

## Web Interface Problems

#### **Problem: I've uploaded new UI files, but the web interface looks the same.**

*   **Solution**: After uploading new files via the "UI File Update" tool, you **must perform a hard refresh** in your web browser to clear its cache.
    *   **On most browsers**: Hold `Ctrl` and press `F5` (or `Cmd + Shift + R` on Mac).

#### **Problem: I forgot the OTA update password.**

*   **Solution**: The default password is `1.21gigawatts`. If you have changed it in the code and forgotten it, you will need to reconnect the device to your computer and re-flash the firmware via the Arduino IDE's standard "Upload" button to regain access. You can find the password setting in the `web_server.cpp` file.