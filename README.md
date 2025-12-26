# HAIoT_SmartMeter

An Arduino-based smart meter integration for Home Assistant, supporting Eastron SDM series (e.g., SDM120M) via Modbus RTU. This project features hardware-bound encrypted storage for secrets and an interactive serial-based configuration system.

## Key Features

*   **Modbus RTU over RS485**: Robust communication with industry-standard smart meters.
*   **Home Assistant Native Integration**: Uses the `ArduinoHA` library for seamless discovery and control.
*   **Security First**: Secrets (Wi-Fi, MQTT credentials) are stored encrypted (AES-CCM) and bound to the hardware device key.
*   **Interactive Setup**: Configure your device via the Serial Monitor—no credentials in source code.
*   **Dual Architecture Support**: Designed for Arduino Nano 33 IoT (SAMD) and Nano ESP32 S3 (ESP32).

## Hardware Requirements

### Microcontrollers
*   **Arduino Nano 33 IoT** (SAMD21)
*   **Arduino Nano ESP32 S3** (ESP32-S3)

### Components
*   **Smart Meter**: Eastron SDM120M (or compatible SDM series via Modbus).
*   **RS485 Transceiver**: MAX485 module or similar TTL-to-RS485 converter.

### Wiring Diagram (MAX485 to Arduino)

| MAX485 Pin | Arduino Pin (Nano 33 IoT) | Arduino Pin (Nano ESP32 S3) |
| :--- | :--- | :--- |
| **VCC** | 5V | 5V |
| **GND** | GND | GND |
| **RO** (RX) | D0 | D3 |
| **DI** (TX) | D1 | D2 |
| **DE** | D4 | D4 |
| **RE** | D5 | D5 |

> [!NOTE]
> For ESP32, pins D2/D3 are used for Serial1 remapping. For SAMD, standard Serial1 pins (0/1) are used.

## Software Requirements

### Libraries
Install these via the Arduino Library Manager:
*   `ArduinoHA` (v2.1.0+)
*   `ArduinoModbus` & `ArduinoRS485`
*   `WiFiNINA` (for Nano 33 IoT) or `WiFi` (for ESP32)
*   `ArduinoLog`
*   `ezTime`
*   `Thread` (ArduinoThread)

### Critical Library Patch (ESP32 Only)
Due to a known issue in the `ArduinoRS485` library for ESP32, you must manually patch the library to enable hardware flow control or use the custom pins. See [this issue](https://github.com/arduino-libraries/ArduinoRS485/issues/54) for details.

## Home Assistant Integration

The device will automatically appear in Home Assistant via MQTT Discovery. Each meter instance provides the following entities:

*   **Energy Dashboard Compatible**: `Total Active Energy (kWh)`
*   **Real-time Monitoring**: Voltage, Current, Active Power, Reactive Power, Power Factor, Frequency.
*   **Demand Stats**: Current Demand, Max Power Demand.

## Getting Started

1.  **Clone & Open**: Open `HAIoT_SmartMeter.ino` in the Arduino IDE.
2.  **Upload**: Select your board and port, then upload the sketch.
3.  **Initial Configuration**:
    *   Open the **Serial Monitor** (9600 baud).
    *   When prompted "Do you want to configure the device?", type **Y**.
    *   Follow the prompts to enter your Wi-Fi SSID, Password, and MQTT Broker IP.
    *   The device will save these securely to Flash/EEPROM.
4.  **Verify**: Check Home Assistant for a new device named after your configured `Device ID`.

## Project Structure

*   `HAIoT_SmartMeter.ino`: Main entry point characterizing the system setup and main execution loops.
*   `sys_config.h`: Manages persistent settings and the interactive serial configuration system.
*   `sys_crypto.h`: Provides hardware-bound AES-256 encryption for sensitive secrets.
*   `sys_wifi.h`: Manages resilient WiFi connectivity and secure network client initialization.
*   `sys_time.h`: Handles NTP time synchronization and local time management.
*   `sys_logStatus.h`: Centralized logging, status reporting, and variadic debugging macros.
*   `sys_serial_utils.h`: Low-level serial communication and non-blocking line reading helpers.
*   `home_assistant.h`: Defines the Home Assistant device entities and manages MQTT communication.
*   `sensor_eastron_smart_meter.h`: Handles Modbus RTU communication and register mapping for the smart meter.

## Disclaimer

This project is provided as-is. Use it at your own risk. The author is not responsible for any damages or issues that might arise from using this code. Ensure you understand the implications of monitoring mains power equipment.

## License

GNU General Public License v3.0 (GPLv3). See [LICENSE](LICENSE) for details.
