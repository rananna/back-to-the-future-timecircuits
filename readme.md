### Time Circuit Schematics (Wiring)

**NOTE:** This project now uses an all-I2C display architecture. This significantly simplifies wiring by eliminating the need for many individual data pins, but requires careful I2C address configuration.

The project utilizes two separate I2C buses on the ESP32 to support all 12 display modules. All displays on a single bus share the same SDA and SCL pins but must have a unique I2C address set via solder jumpers on the back of the display backpack.

#### Power Distribution
A stable 5V source is crucial.
* **5V Power**: Connect the **VIN** pin of the ESP32, all display VCC pins, and the DFPlayer Mini VCC pin to your 5V/2A power source.
* **3.3V Power (Logic Level)**: Connect the **3.3V** output pin from the ESP32 to the **VI2C** pin on **all 12 of the Adafruit HT16K33 display backpacks**. This ensures that the I2C logic levels are matched correctly to the ESP32's 3.3V logic, which is essential for stable communication.
* **GND**: Create a common ground rail. Connect a **GND** pin from the ESP32 and all component GND pins to this rail.

#### I2C Bus 1 (Destination & Present Displays)

This bus controls all eight displays for the Destination and Present rows.
* **SDA**: All eight displays connect to ESP32 **GPIO 21**.
* **SCL**: All eight displays connect to ESP32 **GPIO 22**.
* **I2C Addresses**: Set each display's address using the solder jumpers according to this table.
    * **Destination Row**:
        * Month: `0x70`
        * Day: `0x71`
        * Year: `0x72`
        * Time: `0x73`
    * **Present Row**:
        * Month: `0x74`
        * Day: `0x75`
        * Year: `0x76`
        * Time: `0x77`

#### I2C Bus 2 (Last Departed Displays)

This bus controls the four displays for the Last Departed row. Note: It is possible to reuse the same I2C addresses (0x70, 0x71, 0x72, 0x73) because this bus uses a completely separate set of physical SDA and SCL pins (GPIO 25 and GPIO 26), preventing any conflicts with I2C Bus 1.
* **SDA**: All four displays connect to ESP32 **GPIO 25**.
* **SCL**: All four displays connect to ESP32 **GPIO 26**.
* **I2C Addresses**: Set each display's address using the solder jumpers according to this table.
    * **Last Departed Row**:
        * Month: `0x70`
        * Day: `0x71`
        * Year: `0x72`
        * Time: `0x73`

#### AM/PM Indicators

Each of the six LEDs needs a current-limiting resistor (220-330Ω) on its connection to Ground.

| Row | LED | ESP32 GPIO |
| :------------ | :--: | :--------: |
| Destination | AM | 13 |
| | PM | 14 |
| Present | AM | 32 |
| | PM | 27 |
| Last Departed | AM | 2 |
| | PM | 4 |

#### Audio Module (DFPlayer Mini)

The DFPlayer Mini uses a serial connection to receive commands from the ESP32. This example uses the ESP32's `Serial2` port.
* **DFPlayer RX** → **ESP32 GPIO 17 (TX2)**
* **DFPlayer TX** → **ESP32 GPIO 16 (RX2)**
* Connect your speaker wires to the **SPK_1** and **SPK_2** pins.