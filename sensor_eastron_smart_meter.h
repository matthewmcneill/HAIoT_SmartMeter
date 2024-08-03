// ----[ Eastron SDM120 Modbus Sensor CONTROL MODULE]----- 

/*
  This module creates a Modbus RTU Client and uses that to read the status 
  off the power meter and write it to the home-assistant entites

  Circuit:
   - Iot Nano 33 board
   - RS485 peripheral board : DollaTek 5PCS 5V MAX485 / RS485 Module TTL to RS-485 MCU Development Board

     - ISO GND connected to GND of the Modbus RTU server
     - Y connected to A/Y of the Modbus RTU server
     - Z connected to B/Z of the Modbus RTU server
     - Jumper positions
       - FULL set to OFF
       - Z \/\/ Y set to ON
*/

#include <ArduinoRS485.h> // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>
#include "sys_logStatus.h"
#include "home_assistant.h"

#include <cstring> // For memcpy

//floats are 32bit values.  Modbus registers are 16 bit, so 2 registers are used to 
// hold the value, therefore we have to do 2 reads and combine them.
// Alternatively, to read a single Holding Register value use: ModbusRTUClient.holdingRegisterRead(...)
float readFloatFromRegisters(int& registerCounter) {
    uint16_t highRegister = 0x0000; // Default value to 0
    uint16_t lowRegister = 0x0000;  // Default value to 0 

    // try and read two registers
    if (ModbusRTUClient.available()) {
      highRegister = ModbusRTUClient.read();
    } else {
      logError("Could not read data from register " + String(registerCounter) + " - modbus register (high) not available");
    }
    registerCounter += 1; // increment anyway to continue switching through the registers

    if (ModbusRTUClient.available()) {
      lowRegister = ModbusRTUClient.read();
    } else {
      logError("Could not read data from register " + String(registerCounter) + " - modbus register (low) not available");
    }
    registerCounter += 1; // increment anyway to continue switching through the registers

    uint32_t combinedBits = (static_cast<uint32_t>(highRegister) << 16) | lowRegister; // Combine bits
    float result;
    std::memcpy(&result, &combinedBits, sizeof(float)); // Interpret as float

    return result;
}

void readMeterAndUpdateHA(HADataType::HAEntitiesType smartMeterHA) {
  uint16_t unusedRegister;  // value to sink unused values when read
  int currentRegister;      // tracks the address of the current register
  int baseRegister = 30000; // holding registers start at the base of 30000
  int startRegister;        // address of the first register of the block being loaded 
  int endRegister;          // address of the last register of the block being loaded

  // break the meter reading into multiple blocks and 
  // do one read and then parse out each block
  // there are three blocks with large gaps between them
  // within each block there are a lot of unused registers
  // register values come from the spec for the Eastron SDM120M
  //   https://www.eastroneurope.com/images/uploads/products/protocol/SDM120-MODBUS_Protocol.pdf

  logStatus("Reading Block 30001-30096 Register values ... ");
  startRegister = baseRegister + 1;   // 30001 (inclusive)
  endRegister = baseRegister + 96;    // 30096 (inclusive)
  // read a block Input Register values from (client) id, addressbetween the two register addresses 
  // (start register offset form base)
  // count of registers to load
  if (!ModbusRTUClient.requestFrom(smartMeterHA.modbusID, HOLDING_REGISTERS, startRegister - 1 - baseRegister, endRegister - startRegister + 1)) {
    logStatus("Sensor read over Modbus failed for block at " + String(startRegister));
    logError(ModbusRTUClient.lastError());
  } else {
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
        default:
          // no parameter to load, do a read and move to next register
          unusedRegister = ModbusRTUClient.read(); 
          currentRegister += 1;
      }
    }
  }

  logStatus("Reading Block 30259-30265 Register values ... ");

  startRegister = baseRegister + 259;  // 30259 (inclusive)
  endRegister = baseRegister + 266;    // 30266 (inclusive)
  // read a block Input Register values from (client) id, addressbetween the two register addresses 
  // (start register offset form base)
  // count of registers to load
  if (!ModbusRTUClient.requestFrom(smartMeterHA.modbusID, HOLDING_REGISTERS, startRegister - 1 - baseRegister, endRegister - startRegister + 1)) {
    logStatus("Sensor read over Modbus failed for block at " + String(startRegister));
    logError(ModbusRTUClient.lastError());
  } else {
    // need to iterate through the registers 
    currentRegister = startRegister;
    while (currentRegister <= endRegister) {
      // read the float for each value, send to home assistant and move the current registers on
      switch (currentRegister) {
        case 30259:
          smartMeterHA.currentDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        case 30265:
          smartMeterHA.maxCurrentDemand.setValue(readFloatFromRegisters(currentRegister));
          break;
        default:
          // no parameter to load, do a read and move to next register
          unusedRegister = ModbusRTUClient.read(); 
          currentRegister += 1;
      }
    }
  }

  logStatus("Reading Block 30343-30345 Register values ... ");

  startRegister = baseRegister + 343;  // 30343 (inclusive)
  endRegister = baseRegister + 346;    // 30346 (inclusive)
  // read a block Input Register values from (client) id, addressbetween the two register addresses 
  // (start register offset form base)
  // count of registers to load
  if (!ModbusRTUClient.requestFrom(smartMeterHA.modbusID, HOLDING_REGISTERS, startRegister - 1 - baseRegister, endRegister - startRegister + 1)) {
    logStatus("Sensor read over Modbus failed for block at " + String(startRegister));
    logError(ModbusRTUClient.lastError());
  } else {
    // need to iterate through the registers 
    currentRegister = startRegister;
    while (currentRegister <= endRegister) {
      // read the float for each value, send to home assistant and move the current registers on
      switch (currentRegister) {
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

void onSensorUpdateEvent() {
  readMeterAndUpdateHA(ha.meter1entities);
  readMeterAndUpdateHA(ha.meter2entities);
}


void setupSmartMeter() {

  logStatus("Setting up Modbus RTU Client to connect to Eastron");

  // start the Modbus RTU client
  while (!ModbusRTUClient.begin(9600)) {
    Serial.println("Failed to start Modbus RTU Client!  Retrying in 5 seconds...");
    delay(5000);
  }
 
  // set up the timer thread
  ha.timers.readSmartMeters.onRun(onSensorUpdateEvent);
	ha.timers.readSmartMeters.setInterval(5000);  // poll every 5 seconds
  ha.timers.readSmartMeters.enabled = true;     // ensure that the thread is enabled

}


