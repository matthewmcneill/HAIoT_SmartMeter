// -----[MAIN MODULE]----- 
// main Arduino initialisation and control loops

#include "sys_config.h"         // installation configuration data
#include "sys_logStatus.h"      // library for logging and debugging
#include "sys_wifi.h"           // library to control the wifi connection
#include "sys_crypto.h"         // crypto control and configuration
#include "home_assistant.h"    // home assistant module

// the setup function runs once when you press reset or power the board
void setup() {
  // light up for startup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // [1] set up system modules
  setupLog(); 
  setupCrypto();  // Crypto must come before Config for secure secret derivation
  setupConfig();     
  setupWiFi();

  // [2] set up the home asistant config for device & entities and timers
  setupHA();   // setupHA will call setups for all subsystems

  // finish startup
  logStatus("Startup complete.");
}

// the loop function runs over and over again forever
void loop() {
  loopWiFi();         // check we still have the network up and do ezTime updates
  ha.loop();          // home assistant polling and event updates
}
