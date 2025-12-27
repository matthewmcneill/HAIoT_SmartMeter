/**
 * @file sys_logStatus.h
 * @author Matthew McNeill
 * @brief Logging and debug status utilities with Serial and LED feedback.
 * @version 1.1.0
 * @date 2025-12-26
 * 
 * @section api Public API
 * - `setupLog()`: Configures serial port and debug pins, initializes ArduinoLog.
 * - `logStatus()`: Logs a status message (variadic) and blinks status LED.
 * - `logError()`: Logs an error message (variadic) and blinks error pattern.
 * - `logText()`: Low-level formatted output to serial (variadic).
 * - `logSuspend()`: Critical failure log and halt (variadic).
 * - `blinkLED()`: Utility for visual feedback.
 * 
 * License: GPLv3 (see project LICENSE file for details)
 */

#pragma once
#include <ArduinoLog.h>
#include <stdarg.h>

// define the global log level - can be overridden before including this file
#ifndef SYSTEM_LOG_LEVEL
  #ifdef DEBUG
    #define SYSTEM_LOG_LEVEL LOG_LEVEL_VERBOSE
  #else
    #define SYSTEM_LOG_LEVEL LOG_LEVEL_NOTICE
  #endif
#endif

// uncomment this line to use the internal LED for debugging
#define DEBUG_LED

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

  // Initialize ArduinoLog
  Log.begin(SYSTEM_LOG_LEVEL, &Serial);
  Log.setPrefix([](Print* _logOutput, int level) {
      // Print timestamp in milliseconds
      _logOutput->print("[");
      _logOutput->print(millis());
      _logOutput->print("] [HAIOT] ");

      // Print log level indicator
      switch (level) {
        case LOG_LEVEL_FATAL:   _logOutput->print("[FATAL]   "); break;
        case LOG_LEVEL_ERROR:   _logOutput->print("[ERROR]   "); break;
        case LOG_LEVEL_WARNING: _logOutput->print("[WARNING] "); break;
        case LOG_LEVEL_NOTICE:  _logOutput->print("[NOTICE]  "); break;
        case LOG_LEVEL_TRACE:   _logOutput->print("[TRACE]   "); break;
        case LOG_LEVEL_VERBOSE: _logOutput->print("[VERBOSE] "); break;
        default:                _logOutput->print("[LOG]     "); break;
      }
  });

  if (Serial) {
    Log.notice(CR "Serial port connected." CR);
  } else {
    // If we're here, Serial failed to connect within timeout
  }
}

/**
 * @brief Logs a formatted string to the serial port.
 * 
 * @param format The printf-style format string.
 * @param ... Variadic arguments for the format string.
 */
void logText(const char* format, ...) { 
  va_list args;
  va_start(args, format);
  Log.verbose(format, args);
  Log.verbose(CR);
  va_end(args);
}

// Overload for backward compatibility with String
void logText(String msg) {
  logText(msg.c_str());
}

/**
 * @brief Logs a status message and provides visual LED feedback.
 */
void logStatus(const char* format, ...) {
  va_list args;
  va_start(args, format);
  Log.notice(format, args);
  Log.notice(CR);
  va_end(args);
  blinkLED(50);
}

// Overload for backward compatibility with String
void logStatus(String msg) {
  logStatus(msg.c_str());
}

/**
 * @brief Logs an error message and provides urgent LED blink feedback.
 */
void logError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  Log.error("Error: ");
  Log.error(format, args);
  Log.error(CR);
  va_end(args);
  blinkLED(100, 3);
}

// Overload for backward compatibility with String
void logError(String msg) {
  logError(msg.c_str());
}

/**
 * @brief Logs a critical failure and suspends program execution.
 * Enters an infinite loop with blinking LED feedback.
 */
void logSuspend(const char* format, ...) {
  va_list args;
  va_start(args, format);
  Log.fatal("Execution suspended: ");
  Log.fatal(format, args);
  Log.fatal(CR);
  va_end(args);

  while (true) {
    // Stop execution.
    blinkLED(1000);
  }
}

// Overload for backward compatibility with String
void logSuspend(String msg) {
  logSuspend(msg.c_str());
}


void logByteArrayAsHex(const byte* byteArray, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (byteArray[i] < 0x10) Log.verbose("0");
    Log.verbose("%x", byteArray[i]);
    if (i < len - 1) {
      Log.verbose(" "); // Add space between bytes
    }
  }
  Log.verbose(CR);
}
