# HAIoT_SmartMeter

This Arduino project focuses on integrating an Eastron SDM120M smart meter with Home Assistant using MQTT, enabling you to monitor your energy consumption in real-time and potentially trigger automations based on usage patterns.

## Overview

The code in this repository achieves the following:

* **MODBUS Serial Communication with Smart Meter:** Reads data from a smart meter connected to the Arduino's serial port. It assumes a specific data format (as commented in the code) and extracts relevant information like total kWh, import/export values, and current power.
* **MQTT Communication:** Employs the `PubSubClient` library to publish the extracted smart meter data to an MQTT broker.
* **Home Assistant Integration:**  Utilizes the `ArduinoHA` library to create sensor entities in Home Assistant, allowing you to visualize and track your energy usage.
* **Configuration Management:**  Stores Wi-Fi and MQTT settings in the Arduino's persistent storage (EEPROM or Preferences).

## Hardware Requirements

* **Arduino Board:** This code is designed for the Arduino Nano 33 IoT and the Nano ESP32 S3.
* **Smart Meter:** You'll need a compatible smart meter that outputs data over a serial connection. The code assumes a specific data format (see comments for details). 
* **RS485 Connection Board since the Eastron smart meters use RS485 communication, you'll need an RS485 transceiver module (e.g., MAX485) to interface it with the Arduino. Refer to the code comments or the module's documentation for wiring instructions.
* **Wi-Fi Connection:** Ensure your Arduino board has access to a Wi-Fi network.
* **MQTT Broker:**  You'll need a running MQTT broker on your network or a cloud-based MQTT service.

## Software Requirements

* **Arduino IDE:** Install the latest version of the Arduino IDE.
* **Libraries:**
   * Install the following libraries through the Arduino Library Manager:
     * `ArduinoHA`
     * `PubSubClient`
     * `WiFiNINA` (for Nano 33 IoT) or `WiFi` (for Nano ESP32 S3)
     * `ArduinoRS485` (if using RS485 communication)

## How it Works

1. **Configuration:**
   * Wi-Fi and MQTT settings are read from the `config` object (defined in `sys_config.h`) and stored in persistent storage.  These can be set interactively from the serial console so you don't need to store sensitive information in your code.

2. **Wi-Fi Connection:**
   * The `setupWifi()` function attempts to connect to the configured Wi-Fi network, with error handling and a timeout mechanism.

3. **MQTT Connection:**
   * After a successful Wi-Fi connection, the `setupHA()` function establishes an MQTT connection to the broker.
   * It also sets up device availability and last will topics for Home Assistant integration.

4. **Smart Meter Data Reading:**
   * The `loop()` function continuously reads data from the serial port (or RS485 if configured).
   * It parses the incoming data based on the assumed format and extracts relevant information.

5. **MQTT Publishing:**
   * The extracted smart meter data is then published to MQTT topics under the `homeassistant` hierarchy, allowing Home Assistant to create sensor entities and display the data.

## Getting Started

1. **Configure `sys_config.h`:** Do not fill in your Wi-Fi credentials, MQTT broker details, or other configuration settings - use the interactive serial prompts instead.
2. **Connect the Smart Meter:** Connect your smart meter to the Arduino's serial port (or via RS485 if applicable) according to the instructions in the code.
3. **Upload the Sketch:** Compile and upload the `ha_iot.ino` sketch to your Arduino.
4. **Monitor in Home Assistant:** Once the Arduino connects and starts publishing data, you should see the corresponding sensor entities in Home Assistant, allowing you to track your energy usage.

**Disclaimer**

This project is provided as-is. Use it at your own risk. The author is not responsible for any damages or issues that might arise from using this code. Please ensure you understand the code and its implications before deploying it in a production environment.
