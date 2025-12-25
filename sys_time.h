// ----[TIME SYNCHRONIZATION MODULE]----- 
// Handles NTP time synchronization using the ezTime library.

#pragma once

#include "sys_logStatus.h"
#include "sys_config.h"

// Nano IoT 33 has NVS but no EEPROM so you need to edit ezTime.h header file with correct options. 
#include <ezTime.h>

Timezone tzLondon;

// Helper for BearSSL to get current time
unsigned long getTime() {
    return tzLondon.now(); // Use ezTime for accurate epoch
}

void setupTime() {
  // set up the timer
  ezt::setInterval(60);  // set interval for NTP time sync polling
  logStatus("Syncing NTP...");
  ezt::waitForSync(5);    // ensure that the time is synced (5 sec timeout)

  // Provide official timezone names
  tzLondon.setLocation(F(config.timeZone.c_str()));
  logStatus("IS8601 for Time Zone '" + config.timeZone + "' :" + tzLondon.dateTime(ISO8601));
}

void loopTime() { 
    // process the timer events
  ezt::events();
}