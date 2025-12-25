/**
 * @file sys_wifi.h
 * @author Matthew McNeill
 * @brief WiFi connectivity and resilient network client management.
 * @version 1.0.0
 * @date 2025-12-25
 * 
 * @section api Public API
 * - `setupWiFi()`: Initializes WiFi hardware and connects to network.
 * - `setupResilientClient()`: Configures `networkClient` (Secure/Plain) for MQTT.
 * - `networkClient`: Global instance of `ResilientClient` for networking.
 * - `loopWiFi()`: Handles background WiFi maintenance.
 * 
 * License: GPLv3 (see project LICENSE file for details)
 */
//
// based on the tutorials by Alex Astrum
// https://medium.com/@alexastrum/getting-started-with-arduino-and-firebase-347ec6917da5

#pragma once

#include <Arduino.h>

//Nano IOT 33
#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <esp_wifi.h>
  #include <WiFiClientSecure.h>
  #define WL_MAC_ADDR_LENGTH 6
#endif

#ifdef ARDUINO_ARCH_SAMD
  #include <WiFiNINA.h>
  #include <utility/wifi_drv.h>
  #include <ArduinoBearSSL.h>
#endif

#include "sys_config.h"
#include "sys_logStatus.h"
#include <ArduinoECCX08.h>
#include "sys_time.h"

/**
 * Re-initialize the WiFi driver.
 * This is currently necessary to switch from BLE to WiFi.
 */
void resetWiFi() {

  #ifdef ARDUINO_ARCH_SAMD
    wiFiDrv.wifiDriverDeinit();
    wiFiDrv.wifiDriverInit();
  #endif

  #ifdef ARDUINO_ARCH_ESP32
// seems to permanently kill the wifi
//    esp_wifi_disconnect();
//    esp_wifi_stop();
//    esp_wifi_deinit();
//    esp_wifi_init(NULL); // Reinitialize the Wi-Fi driver
//    delay(1000);
  #endif

}

void connectToWiFi()
{
  int status = WiFi.status();
  if (status == WL_CONNECTED)
  {
    return;
  }

  // connect to the wifi
  logStatus("Connecting to WiFi...");
  while(true) {
    WiFi.setHostname(config.deviceID.c_str());
    status = WiFi.begin(config.secretWiFiSSID.c_str(), config.secretWiFiPassword.c_str());
#ifdef ARDUINO_ARCH_ESP32
    status = WiFi.waitForConnectResult();  // needed for ESP32
#endif
    if (status == WL_CONNECTED) {
      break;
    } else {
      // evaluate failure mode   
      switch (status) {
      case WL_CONNECT_FAILED:
        logText("Connection failed. Check SSID and password.");
        break;
      case WL_NO_SSID_AVAIL:
        logText("SSID not found. Check if the network is available.");
        break;
      case WL_CONNECTION_LOST:
        logText("Connection lost. Check network stability.");
        break;
      case WL_DISCONNECTED:
        logText("Connection disconnected. Double-check that you've entered the correct SSID and password.");
        break; 
      default:
        logText("Unknown error [" + String(status) + "] occurred.");
      }
    }
    logError("Retrying in 5 seconds...");
    resetWiFi();
    delay(5000);
  }

  // log the mac address
  byte mac[6];
  WiFi.macAddress(mac);
  logStatus("MAC Address: ");
  logByteArrayAsHex(mac,6);
  // connection complete
  logStatus("Connected to WiFi.");
}

/**
 * @brief Initializes the WiFi hardware and establishes a network connection.
 * Handles hardware error checks and retries connection on failure.
 */
void setupWiFi()
{
  int status = WiFi.status();

  #ifdef ARDUINO_ARCH_SAMD
    if (status == WL_NO_SHIELD)
    {
      logSuspend("WiFi shield missing!");
    }
    if (status == WL_NO_MODULE)
    {
      logSuspend("Communication with WiFi module failed!");
    }
    if (WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION)
    {
      logStatus("Please upgrade WiFi firmware!");
    }
  #endif

  resetWiFi();
  connectToWiFi();
}

// helper functions
String getWiFiMACAddressAsString(bool includeColons = true) {
  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac); 

  String macAddress = "";
  for (int i = 0; i < 6; ++i) {
    macAddress += String(mac[i], HEX); // Convert each byte to hex and append
    if ((i < 5) && (includeColons)) {
      macAddress += ":"; // Add colon separator (except after the last byte)
    }
  }

  return macAddress;
}

// ================================[ Resilient Network Client ]==================================

WiFiClient baseClient;             // standard unencrypted client

#ifdef ARDUINO_ARCH_ESP32
WiFiClientSecure sslClient;        // ESP32 secure client
#endif

#ifdef ARDUINO_ARCH_SAMD
WiFiSSLClient sslClient;           // SAMD secure client (uses WiFi module firmware TLS)
#endif

// A proxy client that allows switching between plain and secure at runtime
class ResilientClient : public Client {
public:
    Client* activeClient = &baseClient;

    virtual int connect(IPAddress ip, uint16_t port) override { return activeClient->connect(ip, port); }
    virtual int connect(const char *host, uint16_t port) override { return activeClient->connect(host, port); }
    virtual size_t write(uint8_t b) override { return activeClient->write(b); }
    virtual size_t write(const uint8_t *buf, size_t size) override { return activeClient->write(buf, size); }
    virtual int available() override { return activeClient->available(); }
    virtual int read() override { return activeClient->read(); }
    virtual int read(uint8_t *buf, size_t size) override { return activeClient->read(buf, size); }
    virtual int peek() override { return activeClient->peek(); }
    virtual void flush() override { activeClient->flush(); }
    virtual void stop() override { activeClient->stop(); }
    virtual uint8_t connected() override { return activeClient->connected(); }
    virtual operator bool() override { return (bool)*activeClient; }
}; // Added semicolon to terminate class definition

ResilientClient networkClient; // Declared instance separately

/**
 * Configure the network client for secure or plain communication.
 * Returns true if TLS is enabled, false if plain.
 * This is autonomous and handles its own time sync and config retrieval.
 */
/**
 * @brief Configures the global `networkClient` for the current network environment.
 * Sets up TLS/SSL anchors and time sync if a Root CA is configured.
 * 
 * @return bool True if TLS/SSL is enabled, false if plain communication is used.
 */
bool setupResilientClient() {
    if (config.secretMqttCA.length() > 0) {
        logStatus("Network Security: TLS enabled.");
        
        // Ensure time is synced for certificate validation via sys_time module
        setupTime();

        // Initialize ECCX08 for hardware crypto support if present
        if (ECCX08.begin()) {
            logStatus("Network Security: Hardware Crypto (ECCX08) initialized.");
        } else {
            logStatus("Network Security: Hardware Crypto not detected, using software TLS.");
        }

#ifdef ARDUINO_ARCH_SAMD
        // WiFiSSLClient handles certificates via the NINA firmware. 
        // No runtime certificate injection is supported via this class.
#endif

#ifdef ARDUINO_ARCH_ESP32
        sslClient.setCACert(config.secretMqttCA.c_str());
#endif
        
        networkClient.activeClient = &sslClient;
        return true;
    } else {
        logStatus("Network Security: Plain communication (no Root CA).");
        networkClient.activeClient = &baseClient;
        return false;
    }
}

/**
 * Perform background network tasks (WiFi reconnection and Time sync).
 */
void loopWiFi() {
    connectToWiFi();
    loopTime();
}