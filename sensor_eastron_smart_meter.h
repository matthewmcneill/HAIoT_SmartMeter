// ----[ Eastron SDM120 Modbus Sensor CONTROL MODULE]----- 

/*
  This module creates a Modbus RTU Client and uses that to read the status 
  off the power meter and write it to the home-assistant entites

  Circuit:
   - Iot Nano 33 board / ESP32 Nano board
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
#ifdef ARDUINO_ARCH_ESP32
// ESP32 does not have a default RS485 object defined in the library, so we define one here.
// Note: Rename back to RS485 because ArduinoModbus has a hard-coded dependency on this global name.
RS485Class RS485(Serial1, MODBUS_TX_PIN, MODBUS_DE_PIN, MODBUS_RE_PIN); 
#else
// For SAMD (Nano 33 IoT), the RS485Class RS485 object is already defined by the ArduinoRS485 library.
// We will use that global object directly below.
#endif

// mapping structure to link Modbus register addresses to HA entities
struct ModbusMapping {
    int address;
    HASensorNumber HADataType::HAEntitiesType::* member;
};

// Define all the mappings for the Eastron SDM120M
// register values come from the spec for the Eastron SDM120M
// https://www.eastroneurope.com/images/uploads/products/protocol/SDM120-MODBUS_Protocol.pdf
//
const ModbusMapping smartMeterMappings[] = {
    {30001, &HADataType::HAEntitiesType::voltage},
    {30007, &HADataType::HAEntitiesType::current},
    {30013, &HADataType::HAEntitiesType::activePower},
    {30019, &HADataType::HAEntitiesType::apparentPower},
    {30025, &HADataType::HAEntitiesType::reactivePower},
    {30031, &HADataType::HAEntitiesType::powerFactor},
    {30071, &HADataType::HAEntitiesType::frequency},
    {30073, &HADataType::HAEntitiesType::importActiveEnergy},
    {30075, &HADataType::HAEntitiesType::exportActiveEnergy},
    {30077, &HADataType::HAEntitiesType::importReactiveEnergy},
    {30079, &HADataType::HAEntitiesType::exportReactiveEnergy},
    {30085, &HADataType::HAEntitiesType::totalSystemPowerDemand},
    {30087, &HADataType::HAEntitiesType::maxTotalSystemPowerDemand},
    {30089, &HADataType::HAEntitiesType::importSystemPowerDemand},
    {30091, &HADataType::HAEntitiesType::maxImportSystemPowerDemand},
    {30093, &HADataType::HAEntitiesType::exportSystemPowerDemand},
    {30095, &HADataType::HAEntitiesType::maxExportSystemPowerDemand},
    {30259, &HADataType::HAEntitiesType::currentDemand},
    {30265, &HADataType::HAEntitiesType::maxCurrentDemand},
    {30343, &HADataType::HAEntitiesType::totalActiveEnergy},
    {30345, &HADataType::HAEntitiesType::totalReactiveEnergy}
};

// floats are 32bit values.  Modbus registers are 16 bit, so 2 registers are used to 
// hold the value, therefore we have to do 2 reads and combine them.
// Alternatively, to read a single Holding Register value use: ModbusRTUClient.holdingRegisterRead(...)
float readFloatFromRegisters(int& registerCounter) {
    uint16_t highRegister = 0x0000; // Default value to 0
    uint16_t lowRegister = 0x0000;  // Default value to 0 

    // try and read two registers
//    logText("R %d : h", registerCounter);

    // first register
    if (ModbusRTUClient.available()) {
      highRegister = ModbusRTUClient.read();
    } else {
      logError("Could not read data from register %d - modbus register (high) not available", registerCounter);
    }
    registerCounter += 1; // increment anyway to continue switching through the registers

    // second register
    if (ModbusRTUClient.available()) {
      lowRegister = ModbusRTUClient.read();
    } else {
      logError("Could not read data from register %d - modbus register (low) not available", registerCounter);
    }
    registerCounter += 1; // increment anyway to continue switching through the registers

//    logText("%x l%x", highRegister, lowRegister);

    // combine 2 x 16 bit ints into a 32 bit float
    uint32_t combinedBits = (static_cast<uint32_t>(highRegister) << 16) | lowRegister; // Combine bits
    float result;
    std::memcpy(&result, &combinedBits, sizeof(float)); // Interpret as float
//    logText(" = f%f", result);

    return result;
}

void readRegisterBlockAndUpdateHA(HADataType::HAEntitiesType& smartMeterHA, int startRegister, int endRegister) {
  uint16_t unusedRegister;  // value to sink unused values when read
  int currentRegister;      // tracks the address of the current register
  int baseRegister = 30000; // holding registers start at the base of 30000
  int status = 0;           // call status

  startRegister += baseRegister;  
  endRegister += baseRegister;    
  logText("Reading Block %d-%d Register values for Modbus Client [%d]", startRegister, endRegister, smartMeterHA.modbusID);

  // read a block Input Register values from (client) id, address between the two register addresses 
  status = ModbusRTUClient.requestFrom(smartMeterHA.modbusID, INPUT_REGISTERS, startRegister - 1 - baseRegister, endRegister - startRegister + 1); 
  if (status == 0) {
    logError("Sensor read over Modbus failed");
    logError(ModbusRTUClient.lastError());
  } else {
    logStatus("Read %d registers successfully", status);
    
    currentRegister = startRegister;
    while (currentRegister <= endRegister) {
      bool found = false;

      // check if the current register has a mapping
      for (const auto& mapping : smartMeterMappings) {
        if (currentRegister == mapping.address) {
          // call setValue on the HA entity and update currentRegister (reads 2 registers)
          (smartMeterHA.*(mapping.member)).setValue(readFloatFromRegisters(currentRegister));
          found = true;
          break;
        }
      }

      if (!found) {
        // no parameter to load, do a read and move to next register
        unusedRegister = ModbusRTUClient.read(); 
        currentRegister += 1;
      }
    }
  }
}

void readMeterAndUpdateHA(HADataType::HAEntitiesType& smartMeterHA) {
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
  // callback, called by the timer thread every XX seconds
  readMeterAndUpdateHA(ha.meter1entities);
  readMeterAndUpdateHA(ha.meter2entities);
}


void setupSmartMeter() {
  logStatus("Setting up RS485 Serial Port");
  //create the RS485 serial port
#if defined(ARDUINO_ARCH_ESP32)
  // ESP32 supports pin remapping for Serial1
  Serial1.begin(MODBUS_SERIAL_BAUD, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
#else
  // SAMD (Nano 33 IoT) uses fixed pins 0 (RX) and 1 (TX) for Serial1
  Serial1.begin(MODBUS_SERIAL_BAUD, SERIAL_8N1);
#endif

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
	ha.timers.readSmartMeters.setInterval(10 * 1000);   // poll every 10 seconds
  ha.timers.readSmartMeters.enabled = true;           // ensure that the thread is enabled

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
