// ----[ Eastron SDM120 Modbus Sensor CONTROL MODULE]----- 

/*
  This module creates a Modbus RTU Client and uses that to read the status 
  off the power meter and write it to the home-assistant entites

  Circuit:
   - Iot Nano 33 board
   - RS485 peripheral board : DollaTek 5PCS 5V MAX485 / RS485 Module TTL to RS-485 MCU Development Board
*/

#include <cstring> // For memcpy
#include "sys_logStatus.h"

#include "home_assistant.h"

// ArduinoModbus depends on the ArduinoRS485 library - had to edit this library for the ESP32
// see https://github.com/arduino-libraries/ArduinoRS485/issues/54
#include <ArduinoRS485.h>  
#include <ArduinoModbus.h>        // HAD TO EDIT THIS INCLUDE TO WORK (see above)
#define MODBUS_DE_PIN       4     // connect DE pin of MAX485 to D4
#define MODBUS_RE_PIN       5     // connect RE pin of MAX485 to D5
#define MODBUS_RX_PIN       3     // connect RO pin of MAX485 to D3
#define MODBUS_TX_PIN       2     // connect DI pin of MAX485 to D2 
#define MODBUS_SERIAL_BAUD  9600  // Baud rate for esp32 and max485 communication

// setup the RS485 class on Serial1, we will need to begin Serial1 with the same params
RS485Class RS485(Serial1, MODBUS_TX_PIN, MODBUS_DE_PIN, MODBUS_RE_PIN); 

//floats are 32bit values.  Modbus registers are 16 bit, so 2 registers are used to 
// hold the value, therefore we have to do 2 reads and combine them.
// Alternatively, to read a single Holding Register value use: ModbusRTUClient.holdingRegisterRead(...)
float readFloatFromRegisters(int& registerCounter) {
    uint16_t highRegister = 0x0000; // Default value to 0
    uint16_t lowRegister = 0x0000;  // Default value to 0 

    // try and read two registers
//    Serial.print("R " + String(registerCounter) + " : h");

    // first register
    if (ModbusRTUClient.available()) {
      highRegister = ModbusRTUClient.read();
    } else {
      logError("Could not read data from register " + String(registerCounter) + " - modbus register (high) not available");
    }
    registerCounter += 1; // increment anyway to continue switching through the registers

    // second register
    if (ModbusRTUClient.available()) {
      lowRegister = ModbusRTUClient.read();
    } else {
      logError("Could not read data from register " + String(registerCounter) + " - modbus register (low) not available");
    }
    registerCounter += 1; // increment anyway to continue switching through the registers

//    Serial.print(highRegister, HEX);
//    Serial.print(" l");
//    Serial.print(lowRegister, HEX);

    // combine 2 x 16 bit ints into a 32 bit float
    uint32_t combinedBits = (static_cast<uint32_t>(highRegister) << 16) | lowRegister; // Combine bits
    float result;
    std::memcpy(&result, &combinedBits, sizeof(float)); // Interpret as float
//    Serial.println(" = f" + String(result));

    return result;
}

/*
void printRegistersToSerial(int id, int type, int address, int nb) {
  uint16_t value;           // value to sink unused values when read
  int status = 0;           // call status

  logText("requestFrom(" + String(id) + ", " + String(type) + ", " + String(address) + ", " + String(nb) + ")");
  status = ModbusRTUClient.requestFrom(id, type, address, nb); 

  if (status == 0) {
    logError("Sensor read over Modbus failed");
    logError(ModbusRTUClient.lastError());
  } else {
    for (int i = 0; i < status; i+=2) {
        Serial.print(String(30000 + i + 1) + " : ");
        value = ModbusRTUClient.read();
        Serial.printf("%04X", value);
        Serial.print(" ");
        value = ModbusRTUClient.read();
        Serial.printf("%04X", value);
        Serial.println();
    }
  }

  promptWaitForUser();

}
*/

void readRegisterBlockAndUpdateHA(HADataType::HAEntitiesType smartMeterHA, int startRegister, int endRegister) {
  uint16_t unusedRegister;  // value to sink unused values when read
  int currentRegister;      // tracks the address of the current register
  int baseRegister = 30000; // holding registers start at the base of 30000
  int status = 0;           // call status

  // register values come from the spec for the Eastron SDM120M
  //   https://www.eastroneurope.com/images/uploads/products/protocol/SDM120-MODBUS_Protocol.pdf

  startRegister += baseRegister;  
  endRegister += baseRegister;    
  logText("Reading Block " + String(startRegister) + "-" + String(endRegister) +" Register values for Modbus Client [" + String(smartMeterHA.modbusID) + "]");

//  printRegistersToSerial(smartMeterHA.modbusID, INPUT_REGISTERS, startRegister - 1 - baseRegister, endRegister - startRegister + 1);

  // read a block Input Register values from (client) id, address between the two register addresses 
  // (start register offset from base)
  // count of registers to load
  status = ModbusRTUClient.requestFrom(smartMeterHA.modbusID, INPUT_REGISTERS, startRegister - 1 - baseRegister, endRegister - startRegister + 1); 
  if (status == 0) {
    logError("Sensor read over Modbus failed");
    logError(ModbusRTUClient.lastError());
  } else {
    logStatus("Read " + String(status) + " registers successfully");
    // need to iterate through the registers 
    currentRegister = startRegister;
    while (currentRegister <= endRegister) {
      // read the float for each value, send to home assistant and move the current registers on
      switch (currentRegister) {
        case 30001:
          smartMeterHA.voltage.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30007:
          smartMeterHA.current.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30013:
          smartMeterHA.activePower.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30019:
          smartMeterHA.apparentPower.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30025:
          smartMeterHA.reactivePower.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30031:
          smartMeterHA.powerFactor.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30071:
          smartMeterHA.frequency.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30073:
          smartMeterHA.importActiveEnergy.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30075:
          smartMeterHA.exportActiveEnergy.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30077:
          smartMeterHA.importReactiveEnergy.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30079:
          smartMeterHA.exportReactiveEnergy.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30085:
          smartMeterHA.totalSystemPowerDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30087:
          smartMeterHA.maxTotalSystemPowerDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30089:
          smartMeterHA.importSystemPowerDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30091:
          smartMeterHA.maxImportSystemPowerDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30093:
          smartMeterHA.exportSystemPowerDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30095:
          smartMeterHA.maxExportSystemPowerDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30259:
          smartMeterHA.currentDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30265:
          smartMeterHA.maxCurrentDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30343:
          smartMeterHA.totalActiveEnergy.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30345:
          smartMeterHA.totalReactiveEnergy.setValue(readFloatFromRegisters(currentRegister));
          break;
        default:
          // no parameter to load, do a read and move to next register
          unusedRegister = ModbusRTUClient.read(); 
          currentRegister += 1;
      }
    }
  }

}

void readMeterAndUpdateHA(HADataType::HAEntitiesType smartMeterHA) {
  // break the meter reading into multiple blocks (the readings are spread out in 
  // the register spectrum) and do one read and then parse out each block
  // there are three blocks with large gaps between them
  // within each block there are a lot of unused registers
  // register values come from the spec for the Eastron SDM120M
  //   https://www.eastroneurope.com/images/uploads/products/protocol/SDM120-MODBUS_Protocol.pdf

  readRegisterBlockAndUpdateHA(smartMeterHA,1,32);
  readRegisterBlockAndUpdateHA(smartMeterHA,71,96);
  readRegisterBlockAndUpdateHA(smartMeterHA,259,266);
  readRegisterBlockAndUpdateHA(smartMeterHA,343,346);
}

void onSensorUpdateEvent() {
  readMeterAndUpdateHA(ha.meter1entities);
  readMeterAndUpdateHA(ha.meter2entities);
}


void setupSmartMeter() {
  logStatus("Setting up RS485 Serial Port");
  //create the RS485 serial port
  Serial1.begin(MODBUS_SERIAL_BAUD, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  RS485.begin(MODBUS_SERIAL_BAUD);

  logStatus("Setting up Modbus RTU Client to connect to Eastron");
  // start the Modbus RTU client (note that params must be the same as above)
  while (!ModbusRTUClient.begin(RS485, MODBUS_SERIAL_BAUD, SERIAL_8N1)) {
    logError("Failed to start Modbus RTU Client!  Retrying in 5 seconds...");
    delay(5000);
  }
  ModbusRTUClient.setTimeout(3000);
 
  // set up the timer thread
  ha.timers.readSmartMeters.onRun(onSensorUpdateEvent);
	ha.timers.readSmartMeters.setInterval(10 * 1000);   // poll every X seconds
  ha.timers.readSmartMeters.enabled = true;           // ensure that the thread is enabled

}


