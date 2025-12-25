/**
 * @file sys_config.h
 * @author Matthew McNeill
 * @brief Software configurations and persistent preferences management.
 * @version 1.0.0
 * @date 2025-12-25
 * 
 * @section api Public API
 * - `config`: Global instance of `ConfigurationStructType` containing all settings.
 * - `setupConfig()`: Initializes preferences and loads/prompts for configuration.
 * - `getUniqueChipID()`: Returns a unique hardware identifier string.
 * 
 * License: GPLv3 (see project LICENSE file for details)
 */
 
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "sys_logStatus.h"
#include "sys_serial_utils.h"
#include "sys_crypto.h"

// Nano ESP 32 - for chip UID
#ifdef ARDUINO_ARCH_ESP32
  #include <esp_system.h>
#endif

// to help with debugging, if you want to disable the interactive Serial config options
// set this to true and it will not force interactive
#define NO_RECONFIGURE false

#define HEADER_PLAIN_V1 "p1:" // marker for plaintext strings
#define HEADER_SECRET_V1 "s1:" // marker for encrypted strings

Preferences preferences;

/**
 * @brief Metadata for a configuration parameter.
 * 
 * @param key The internal preference key string (max 15 chars for Preferences library).
 * @param mandatory If true, initialization will block until a value is provided.
 * @param isSecret If true, the value is stored encrypted via the sys_crypto module.
 * @param isConfigurable If true, the parameter can be reconfigured interactively.
 * @param prompt The descriptive text shown to the user during interactive configuration.
 */
struct ConfigParam {
  const char* key;
  bool mandatory;
  bool isSecret;
  bool isConfigurable;
  const char* prompt;
};

struct ConfigurationStructType {
  // Metadata definitions             <NAME>                <KEY>            <MANDATORY> <SECRET> <CONFIGURABLE> <PROMPT> 
  static inline constexpr ConfigParam PARAM_DEVICE_ID    = {"dvc_id",         true,       false,   true,          "Enter a unique network device ID that is used when connecting to the WifI and Home Assistant: "};
  static inline constexpr ConfigParam PARAM_MANUFACTURER = {"dvc_manuf",      false,      false,   false,         "Device Manufacturer: this should not need to be configured: "};
  static inline constexpr ConfigParam PARAM_MODEL        = {"dvc_model",      false,      false,   false,         "Device Model: this should not need to be configured: "};
  static inline constexpr ConfigParam PARAM_TIMEZONE     = {"tz",             true,       false,   true,          "Enter a standard TimeZone for your device to configure local time. A full list is available here https://en.wikipedia.org/wiki/List_of_tz_database_time_zones : "};
  static inline constexpr ConfigParam PARAM_MQTT_BROKER  = {"mqtt_broker_ip", true,       false,   true,          "Please enter a valid IP address for the MQTT broker: "};
  // Secret Items (really should NOT have defaults specificed here in the source code)
  static inline constexpr ConfigParam PARAM_WIFI_SSID    = {"s_wifi_ssid",    true,       true,    true,          "Enter your WiFi network SSID: "};
  static inline constexpr ConfigParam PARAM_WIFI_PWD     = {"s_wifi_pwd",     true,       true,    true,          "Enter your WiFi password: "};
  static inline constexpr ConfigParam PARAM_MQTT_USER    = {"s_mqtt_user",    true,       true,    true,          "Enter your MQTT broker user name: "};
  static inline constexpr ConfigParam PARAM_MQTT_PWD     = {"s_mqtt_pwd",     true,       true,    true,          "Enter your MQTT broker password: "};
  static inline constexpr ConfigParam PARAM_MQTT_CA      = {"s_mqtt_ca",      false,      true,    true,          "Enter your MQTT Broker Root CA (PEM format, or leave empty for plain MQTT): "};

  // General configuration items
  String deviceID               = "";                               // used for WiFi and Home Assistant device IDs
  String deviceSoftwareVersion  = "1.0.0";                          // used by Home Assistant
  String deviceManufacturer     = "Arduino";                        // used by Home Assistant
#ifdef ARDUINO_ARCH_SAMD
  String deviceModel            = "Nano 33 IoT";                    // used by Home Assistant
#endif
#ifdef ARDUINO_ARCH_ESP32
  String deviceModel            = "Nano ESP32 S3";                    // used by Home Assistant
#endif
  String timeZone               = "Europe/London";                  // used by NTP Time Libraries
  IPAddress mqttBrokerAddress   = IPAddress(0,0,0,0);               // used by Home Assistant for MQTT broker

  // Secret Items (really should NOT have defaults specificed here in the source code)
  String secretWiFiSSID         = "";                               // used by WiFi
  String secretWiFiPassword     = "";                               // used by WiFi
  String secretMqttUser         = "";                               // used by Home Assistant for MQTT broker
  String secretMqttPassword     = "";                               // used by Home Assistant for MQTT broker
  String secretMqttCA           = "";                               // Root CA for MQTT TLS (Empty for plain MQTT)
} config;


/**
 * @brief Loads a configuration value from persistent storage (Preferences).
 * Supports versioned plaintext and encrypted formats.
 * 
 * @param key The preference key to look up.
 * @param defaultValue Value to return if key is missing.
 * @param prompt Prompt string for interactive setup.
 * @param mandatory If true, keeps asking for input until a value is provided.
 * @param force If true, clears existing value and forces a new prompt.
 * @param isSecret If true, value is stored encrypted using sys_crypto.
 * @return String The loaded or newly entered configuration value.
 */
String loadConfig(String key, String defaultValue = "", String prompt = "", bool mandatory = false, bool force = false, bool isSecret = false) {
  // assumes a preferences namespace has been opened
  String rawValue = preferences.getString(key.c_str(), "");
  String value = "";
  bool migrationNeeded = false;

  // 1. Detect format and extract raw value
  if (rawValue == "") {
    // No value found in flash, use default
    value = defaultValue;
    if (value != "") migrationNeeded = true; 
  } else if (rawValue.startsWith(HEADER_SECRET_V1)) {
    // Current Secret format: extract payload and decrypt
    String payload = rawValue.substring(strlen(HEADER_SECRET_V1));
    value = decryptSecret(payload);
    if (!isSecret) migrationNeeded = true; // Should be plaintext now
  } else if (rawValue.startsWith(HEADER_PLAIN_V1)) {
    // Current Plaintext format: extract payload
    value = rawValue.substring(strlen(HEADER_PLAIN_V1));
    if (isSecret) migrationNeeded = true; // Should be secret now
  } else {
    // Legacy format (no prefix): treat as raw value
    value = rawValue;
    migrationNeeded = true; // Upgrade to versioned format
  }

  if (force) {
    // unset the value, and make the current value the default to force a prompt
    defaultValue = value;
    value = "";
  }

  // no value found
  while (value == "") {
    //logStatus("No value found for key: " + key);
    value = promptAndReadLine(prompt.c_str(), defaultValue.c_str());
    // if mandatory, keep asking
    if (mandatory && (value == "")) {
      logStatus("A value is required for this key to proceed.");
    } else {
      // user provided a value or it's not mandatory
      migrationNeeded = true; 
      break;
    }
  }

  // 2. Auto-migrate/save if needed
  if (migrationNeeded) {
    String valueToSave = "";
    if (isSecret && value != "") {
      valueToSave = String(HEADER_SECRET_V1) + encryptSecret(value);
    } else {
      valueToSave = String(HEADER_PLAIN_V1) + value;
    }
    // save the value (putString checks for nugatory writes)
    preferences.putString(key.c_str(), valueToSave);
  }

  return value;
}

/**
 * @brief Overload of loadConfig that uses ConfigParam metadata.
 */
String loadConfig(ConfigParam param, String defaultValue = "", bool force = false) {
  // combine the global reconfigure intent with the individual parameter's capability
  bool doForce = force && param.isConfigurable;
  return loadConfig(param.key, defaultValue, param.prompt, param.mandatory, doForce, param.isSecret);
}


/**
 * @brief Orchestrates the interactive device configuration.
 * Prompts user for WiFi, MQTT, and regional settings if needed or requested.
 */
void setupConfig() {
  bool doReconfigure;

  if (Serial && !NO_RECONFIGURE) {
    // if we have a serial port connection to the IDE terminal ask for forced reconfiguration
    // note: interactive reconfiguration will always occur if mandatory items are not set and there are no defaults
    doReconfigure = promptAndReadYesNo("Do you want to configure the device?", false);
  } else {
    doReconfigure = false;
  }

  // Load General Config
  preferences.begin("config");
    config.deviceID           = loadConfig(config.PARAM_DEVICE_ID,    config.deviceID,         doReconfigure);
    config.deviceManufacturer = loadConfig(config.PARAM_MANUFACTURER, config.deviceManufacturer, doReconfigure);
    config.deviceModel        = loadConfig(config.PARAM_MODEL,        config.deviceModel,        doReconfigure);
    config.timeZone           = loadConfig(config.PARAM_TIMEZONE,     config.timeZone,         doReconfigure);

    while (!config.mqttBrokerAddress.fromString( 
      loadConfig(config.PARAM_MQTT_BROKER, config.mqttBrokerAddress.toString(), doReconfigure).c_str() )) 
    {
      logStatus("Could not parse IP Address, or IP address is unconfigured value 0.0.0.0, please try again.");
    }
  preferences.end();

  // Ensure hardware key is derived before loading secrets
  deriveHardwareKey();

  // Load Secrets
  preferences.begin("secrets");
    config.secretWiFiSSID     = loadConfig(config.PARAM_WIFI_SSID,    config.secretWiFiSSID,     doReconfigure);
    config.secretWiFiPassword = loadConfig(config.PARAM_WIFI_PWD,     config.secretWiFiPassword, doReconfigure);
    config.secretMqttUser     = loadConfig(config.PARAM_MQTT_USER,    config.secretMqttUser,     doReconfigure);
    config.secretMqttPassword = loadConfig(config.PARAM_MQTT_PWD,     config.secretMqttPassword, doReconfigure);
    config.secretMqttCA       = loadConfig(config.PARAM_MQTT_CA,      config.secretMqttCA,       doReconfigure);
  preferences.end();
}

/**
 * @brief Retrieves a unique hardware ID for the chip.
 * Uses ESP32 MAC or SAMD's unique serial number.
 * 
 * @return String 8-character hex represention of the unique ID.
 */
String getUniqueChipID() {

#ifdef ARDUINO_ARCH_SAMD
  // Get the unique device ID (for SAMD-based boards)
  uint32_t uniqueID = *(uint32_t*)0x0080A00C;

  // Convert the ID to a string (hexadecimal representation)
  char uniqueIDStr[9]; // 8 hex digits + null terminator
  sprintf(uniqueIDStr, "%08X", uniqueID);

  return String(uniqueIDStr);
#endif

#ifdef ARDUINO_ARCH_ESP32
  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint16_t chip = (uint16_t)(chipid >> 32);

  // Convert the ID to a string (hexadecimal representation)
  char uniqueIDStr[9]; // 8 hex digits + null terminator
  sprintf(uniqueIDStr, "%08X", chip);

  return String(uniqueIDStr);
#endif

}