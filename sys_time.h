/**
 * @file sys_time.h
 * @author Matthew McNeill
 * @brief NTP Time synchronization and management using ezTime library.
 * @version 1.0.0
 * @date 2025-12-25
 * 
 * @section api Public API
 * - `setupTime()`: Synchronizes local time via NTP.
 * - `loopTime()`: Maintains time synchronization in the background.
 * - `getTime()`: Returns current Unix timestamp for TLS/SSL.
 * 
 * License: GPLv3 (see project LICENSE file for details)
 */

#pragma once

#include "sys_logStatus.h"
#include "sys_config.h"

// Nano IoT 33 has NVS but no EEPROM so you need to edit ezTime.h header file with correct options. 
#include <ezTime.h>

Timezone tzLondon;

/**
 * @brief Helper for BearSSL to get current time.
 * 
 * @return unsigned long Current Unix epoch timestamp.
 */
unsigned long getTime() {
    return tzLondon.now(); // Use ezTime for accurate epoch
}

/**
 * @brief Initializes and synchronizes local time via NTP.
 * Waits for a successful sync before proceeding.
 */
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