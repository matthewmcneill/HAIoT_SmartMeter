/**
 * @file sys_logStatus.h
 * @author Matthew McNeill
 * @brief Logging and debug status utilities with Serial and LED feedback.
 * @version 1.0.0
 * @date 2025-12-25
 * 
 * @section api Public API
 * - `setupLog()`: Configures serial port and debug pins.
 * - `logStatus()`: Logs a status message and blinks status LED.
 * - `logError()`: Logs an error message and blinks error pattern.
 * - `logText()`: Low-level formatted output to serial.
 * - `blinkLED()`: Utility for visual feedback.
 * 
 * License: GPLv3 (see project LICENSE file for details)
 */

// Variadic macros used for debugging to print information in de-bugging mode from LarryD, Arduino forum

#pragma once

// un-comment this line to print the debugging statements
#define DEBUG
// uncomment this line to use the internal LED for debugging
#define DEBUG_LED

#ifdef DEBUG
  #define DPRINT(...)    Serial.print(__VA_ARGS__)
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)
#else
  // define blank line
  #define DPRINT(...)
  #define DPRINTLN(...)
#endif

/**
 * @brief Utility to blink the built-in LED.
 * 
 * @param duration Period of each blink in milliseconds.
 * @param numberOfTimes Total number of blinks to perform.
 */
void blinkLED(int duration, int numberOfTimes = 1) {
#ifdef DEBUG_LED
  bool state = (digitalRead(LED_BUILTIN) == HIGH);
  for (int i = 0; i < numberOfTimes; ++i) {
    digitalWrite(LED_BUILTIN, (!state ? HIGH : LOW));
    delay(duration);
    digitalWrite(LED_BUILTIN, (state ? HIGH : LOW));
    delay(duration);
  }
#endif
}

/**
 * @brief Initializes the serial port and LED pins for logging.
 * Waits briefly for a serial connection before proceeding.
 */
void setupLog() {
#ifdef DEBUG_LED
  pinMode(LED_BUILTIN, OUTPUT);
#endif
  Serial.begin(115200); 
  // Add a timeout to prevent infinite loop when no serial monitor is connected
  unsigned long startTime = millis();
  const unsigned long timeout = 5000; // 5 seconds timeout
  while (!Serial && (millis() - startTime < timeout)) {
    ; // Wait for serial port to connect, but with a timeout
  }
  if (Serial) {
    DPRINTLN("\nSerial port connected.");
  } else {
    DPRINTLN("\nNo serial port connected.");  // a bit pointless really.
  }
}

/**
 * @brief Logs a raw string to the serial port.
 * 
 * @param msg The message to log.
 */
void logText(String msg) { 
  DPRINTLN(msg); 
}

/**
 * @brief Logs a status message and provides visual LED feedback.
 * 
 * @param msg The status message to log.
 */
void logStatus(String msg) {
  logText(msg);
  blinkLED(50);
}

/**
 * @brief Logs an error message and provides urgent LED blink feedback.
 * 
 * @param msg The error message to log.
 */
void logError(String msg) {
  logText("Error: " + msg);
  blinkLED(100, 3);
}

/**
 * @brief Logs a critical failure and suspends program execution.
 * Enters an infinite loop with blinking LED feedback.
 * 
 * @param msg The reason for suspension.
 */
void logSuspend(String msg) {
  logText("Execution suspended: " + msg);
  while (true) {
    // Stop execution.
#ifdef DEBUG_LED
    blinkLED(1000);
#endif
  }
}

void logByteArrayAsHex(const byte* byteArray, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    DPRINT(byteArray[i] >> 4, HEX); // Print high nibble
    DPRINT(byteArray[i] & 0xF, HEX); // Print low nibble
    if (i < len - 1) {
      DPRINT(" "); // Add space between bytes
    }
  }
  DPRINTLN();
}
