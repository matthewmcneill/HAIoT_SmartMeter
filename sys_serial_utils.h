/**
 * @file sys_serial_utils.h
 * @author Matthew McNeill
 * @brief Interactive serial port utilities for user prompts and input.
 * @version 1.0.0
 * @date 2025-12-25
 * 
 * @section api Public API
 * - `promptAndReadLine()`: Displays a prompt and reads user input with a default value.
 * - `promptAndReadYesNo()`: Prompts user for a Y/N choice.
 * - `readLine()`: Reads a line from the serial buffer.
 * 
 * License: GPLv3 (see project LICENSE file for details)
 *
 * serial port interaction utilities from 
 * https://github.com/arduino-libraries/ArduinoECCX08/blob/master/examples/Tools/ECCX08JWSPublicKey/ECCX08JWSPublicKey.ino
 *
 */


#pragma once

/**
 * @brief Reads a single line from the serial port.
 * Blocks until a newline character is received.
 * 
 * @return String The characters read before the newline.
 */
String readLine() {
  String line;

  while (1) {
    if (Serial.available()) {
      char c = Serial.read();

      if (c == '\r') {
        // ignore
        continue;
      } else if (c == '\n') {
        break;
      }

      line += c;
    }
  }

  return line;
}

/**
 * @brief Prompts the user and waits for a line of input.
 * 
 * @param prompt The prompt message to display.
 * @param defaultValue The value to return if the user just presses enter.
 * @return String The user's input or the default value.
 */
String promptAndReadLine(const char* prompt, const char* defaultValue) {
  Serial.print(prompt);
  Serial.print(" [");
  Serial.print(defaultValue);
  Serial.print("]: ");

  String s = readLine();

  if (s.length() == 0) {
    s = defaultValue;
  }

  Serial.println(s);

  return s;
}

/**
 * @brief Prompts the user for a Yes/No choice.
 * 
 * @param prompt The question to ask.
 * @param defaultValue The default boolean choice.
 * @return bool True for 'yes/y', false for 'no/n'.
 */
bool promptAndReadYesNo(String prompt, bool defaultValue) {
  prompt = prompt + (defaultValue ? " (Y/n)" : " (y/N)" );
  String yesno = promptAndReadLine(prompt.c_str(), defaultValue ? "Y" : "N");
  yesno.toLowerCase();
  return (yesno.startsWith("y"));
}

void promptWaitForUser() {
  if (Serial) {
    //only do this if serial port is up
    promptAndReadLine("System waiting... ", "press enter to continue");
  }
}