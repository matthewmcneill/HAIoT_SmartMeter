// ----[HOME ASSISTANT CONTROL MODULE]----- 
// built for Arduino IoT 33 wifi board

#pragma once

// include system modules
#include "sys_config.h"
#include "sys_logStatus.h"
#include "sys_wifi.h"

// ====================================[ HA device + entity definition ]=======================================

// Turns on debug information of the ArduinoHA core (from <ArduinoHADefines.h>)
// Please note that you need to initialize serial interface 
// by calling Serial.begin([baudRate]) before initializing ArduinoHA.
// #define ARDUINOHA_DEBUG

#include <ArduinoHADefines.h>   // HA libraries
#include <ArduinoHA.h>          //  |
#include <HADevice.h>           //  |
#include <HAMqtt.h>             //  |
#include <Thread.h>             // simple threading library for time event driven updates
#include <ThreadController.h>   //  |

WiFiClient networkClient;               // declare a wifi client object for the HA MQTT connection 

// a nice herlper class to organise all the HA objects into a neat collection 
// that makes them easier to navigate in the logic and collects all the configuration 
// together into a neat package.  Note that the order of construction is very precise
//
// find your full list fo mdi:icons here: https://pictogrammers.com/library/mdi/
// and your full list of Units here: https://github.com/home-assistant/core/blob/d7ac4bd65379e11461c7ce0893d3533d8d8b8cbf/homeassistant/const.py#L384
//
class HADataType {
public:
    HADevice device; // HADevice entity object
    HAMqtt mqtt;     // mqtt communication object
    // Constructor for HADataType
    HADataType(arduino::Client &netClient) :
        device(),                       
        mqtt(netClient, this->device),  // needs to be constructed here with the newly created device in this order
        meter1entities(1),              // volt meter on MODBUS device ID 1
        meter2entities(2),              // volt meter on MODBUS device ID 2
        timers()
        {}; 

    // Nested class for defining the Entities on the device
    class HAEntitiesType {
    public:            
        int modbusID;     // store this, it will link to the modbus item ID when we use it in the modbus module
        HASensorNumber voltage;
        HASensorNumber current;
        HASensorNumber activePower;
        HASensorNumber apparentPower;
        HASensorNumber reactivePower;
        HASensorNumber powerFactor;
        HASensorNumber frequency;
        HASensorNumber importActiveEnergy;
        HASensorNumber exportActiveEnergy;
        HASensorNumber importReactiveEnergy;
        HASensorNumber exportReactiveEnergy;
        HASensorNumber totalSystemPowerDemand;
        HASensorNumber maxTotalSystemPowerDemand;
        HASensorNumber importSystemPowerDemand;
        HASensorNumber maxImportSystemPowerDemand;
        HASensorNumber exportSystemPowerDemand;
        HASensorNumber maxExportSystemPowerDemand;
        HASensorNumber currentDemand;
        HASensorNumber maxCurrentDemand;
        HASensorNumber totalActiveEnergy;
        HASensorNumber totalReactiveEnergy;


        // Set up all the entities (note that event handlers are added later)
        HAEntitiesType(int clientID) : 
            modbusID(clientID), 
            voltage(String("ups_" + String(clientID) + ".voltage").c_str(), HASensorNumber::PrecisionP2), 
            current(String("ups_" + String(clientID) + ".current").c_str(), HASensorNumber::PrecisionP2), 
            activePower(String("ups_" + String(clientID) + ".activePower").c_str(), HASensorNumber::PrecisionP2), 
            apparentPower(String("ups_" + String(clientID) + ".apparentPower").c_str(), HASensorNumber::PrecisionP2), 
            reactivePower(String("ups_" + String(clientID) + ".reactivePower").c_str(), HASensorNumber::PrecisionP2), 
            powerFactor(String("ups_" + String(clientID) + ".powerFactor").c_str(), HASensorNumber::PrecisionP2), 
            frequency(String("ups_" + String(clientID) + ".frequency").c_str(), HASensorNumber::PrecisionP2), 
            importActiveEnergy(String("ups_" + String(clientID) + ".importActiveEnergy").c_str(), HASensorNumber::PrecisionP2), 
            exportActiveEnergy(String("ups_" + String(clientID) + ".exportActiveEnergy").c_str(), HASensorNumber::PrecisionP2), 
            importReactiveEnergy(String("ups_" + String(clientID) + ".importReactiveEnergy").c_str(), HASensorNumber::PrecisionP2), 
            exportReactiveEnergy(String("ups_" + String(clientID) + ".exportReactiveEnergy").c_str(), HASensorNumber::PrecisionP2), 
            totalSystemPowerDemand(String("ups_" + String(clientID) + ".totalSystemPowerDemand").c_str(), HASensorNumber::PrecisionP2), 
            maxTotalSystemPowerDemand(String("ups_" + String(clientID) + ".maxTotalSystemPowerDemand").c_str(), HASensorNumber::PrecisionP2), 
            importSystemPowerDemand(String("ups_" + String(clientID) + ".importSystemPowerDemand").c_str(), HASensorNumber::PrecisionP2), 
            maxImportSystemPowerDemand(String("ups_" + String(clientID) + ".maxImportSystemPowerDemand").c_str(), HASensorNumber::PrecisionP2), 
            exportSystemPowerDemand(String("ups_" + String(clientID) + ".exportSystemPowerDemand").c_str(), HASensorNumber::PrecisionP2), 
            maxExportSystemPowerDemand(String("ups_" + String(clientID) + ".maxExportSystemPowerDemand").c_str(), HASensorNumber::PrecisionP2), 
            currentDemand(String("ups_" + String(clientID) + ".currentDemand").c_str(), HASensorNumber::PrecisionP2), 
            maxCurrentDemand(String("ups_" + String(clientID) + ".maxCurrentDemand").c_str(), HASensorNumber::PrecisionP2), 
            totalActiveEnergy(String("ups_" + String(clientID) + ".totalActiveEnergy").c_str(), HASensorNumber::PrecisionP2), 
            totalReactiveEnergy(String("ups_" + String(clientID) + ".totalReactiveEnergy").c_str(), HASensorNumber::PrecisionP2) 
            {

              voltage.setIcon("mdi:meter-electric-outline");
              voltage.setName("Voltage");
              voltage.setUnitOfMeasurement("V");

              current.setIcon("mdi:mdi:current-ac");
              current.setName("Current");
              current.setUnitOfMeasurement("A");

              activePower.setIcon("mdi:transmission-tower");
              activePower.setName("Active Power");
              activePower.setUnitOfMeasurement("W");

              apparentPower.setIcon("mdi:transmission-tower");
              apparentPower.setName("Apparent Power");
              apparentPower.setUnitOfMeasurement("W");
              
              reactivePower.setIcon("mdi:transmission-tower");
              reactivePower.setName("Reactive Power");
              reactivePower.setUnitOfMeasurement("W");
              
              powerFactor.setIcon("mdi:ab-testing");
              powerFactor.setName("Power Factor");

              frequency.setIcon("mdi:sine-wave");
              frequency.setName("Frequency");
              frequency.setUnitOfMeasurement("Hz");             

              importActiveEnergy.setIcon("mdi:transmission-tower-import");
              importActiveEnergy.setName("Active Energy Import");
              importActiveEnergy.setUnitOfMeasurement("kWh");    

              exportActiveEnergy.setIcon("mdi:transmission-tower-export");
              exportActiveEnergy.setName("Reactive Energy Export");
              exportActiveEnergy.setUnitOfMeasurement("kWh");                  

              importReactiveEnergy.setIcon("mdi:transmission-tower-import");
              importReactiveEnergy.setName("Reactive Energy Import");
              importReactiveEnergy.setUnitOfMeasurement("kvarh");    

              exportReactiveEnergy.setIcon("mdi:transmission-tower-export");
              exportReactiveEnergy.setName("Reactive Energy Export");
              exportReactiveEnergy.setUnitOfMeasurement("kvarh");     

              totalSystemPowerDemand.setIcon("mdi:transmission-tower");
              totalSystemPowerDemand.setName("Total System Power Demand");
              totalSystemPowerDemand.setUnitOfMeasurement("W");

              maxTotalSystemPowerDemand.setIcon("mdi:transmission-tower");
              maxTotalSystemPowerDemand.setName("Max Total System Power Demand");
              maxTotalSystemPowerDemand.setUnitOfMeasurement("W");

              importSystemPowerDemand.setIcon("mdi:transmission-tower-import");
              importSystemPowerDemand.setName("Import System Power Demand");
              importSystemPowerDemand.setUnitOfMeasurement("W");

              maxImportSystemPowerDemand.setIcon("mdi:transmission-tower-import");
              maxImportSystemPowerDemand.setName("Max Import System Power Demand");
              maxImportSystemPowerDemand.setUnitOfMeasurement("W");

              exportSystemPowerDemand.setIcon("mdi:transmission-tower-export");
              exportSystemPowerDemand.setName("Export System Power Demand");
              exportSystemPowerDemand.setUnitOfMeasurement("W");

              maxExportSystemPowerDemand.setIcon("mdi:transmission-tower-export");
              maxExportSystemPowerDemand.setName("Max Export System Power Demand");
              maxExportSystemPowerDemand.setUnitOfMeasurement("W");

              currentDemand.setIcon("mdi:mdi:current-ac");
              currentDemand.setName("Current Demand");
              currentDemand.setUnitOfMeasurement("A");

              maxCurrentDemand.setIcon("mdi:mdi:current-ac");
              maxCurrentDemand.setName("Max Current Demand");
              maxCurrentDemand.setUnitOfMeasurement("A");

              totalActiveEnergy.setIcon("mdi:transmission-tower");
              totalActiveEnergy.setName("Total Active Energy");
              totalActiveEnergy.setUnitOfMeasurement("kWh");

              totalReactiveEnergy.setIcon("mdi:transmission-tower");
              totalReactiveEnergy.setName("Total Reactive Energy");
              totalReactiveEnergy.setUnitOfMeasurement("kvarh");

            }
    }; 
    // declare the instances
    HAEntitiesType meter1entities; // container object for entities
    HAEntitiesType meter2entities; // container object for entities

    // Nested class for defining the events we want to run on a timer
    class ThreadTimersType {
    public: 
      ThreadController controller;  // ThreadController that will controll all threads
      Thread readSmartMeters;     // timer thread for polling the temperature sensor
      ThreadTimersType() :
          controller(),
          readSmartMeters() 
          {
              // add all the threads to the controller
              this->controller.add(&this->readSmartMeters);
          }
    } timers;

    // Method to drive all the polling on the ha object and raise the events
    void loop() {
      this->timers.controller.run();    // poll all the timer events
      this->mqtt.loop();                // then propagate any status to MQTT and poll MQTT events
    }
};
// Create instance of the HA controller object, and connect it to the network
HADataType ha(networkClient); 

// ====================[ Include all the sensor and indicator control modules here ]=====================

// have to put them after the ha class definition and instantiation, so that they can reference it
// seems a bit hacky but that's the consequence of combining .h and .cpp and/or the use of a 
// global ha variable.  However the overall code is simpler

// include sensor sub-modules
#include "sensor_eastron_smart_meter.h" // control module for the smart meters

// ====================================[ HA setup and connection ]=======================================
 
void setupHA() {

  // [1] -- set up the HA device --

  logStatus("Configuring the HA Device.");
  // Retrieve MAC address from the wifi card and initialise the HA device with that as the unique ID

  byte macAddress[WL_MAC_ADDR_LENGTH]; // a byte string to containg the mac address we need to configure the HA device
  WiFi.macAddress(macAddress);

  // Initialize the HADevice object
  ha.device.setUniqueId(macAddress, sizeof(macAddress));
  ha.device.setName(config.deviceID.c_str());
  ha.device.setSoftwareVersion(config.deviceSoftwareVersion.c_str());
  ha.device.setManufacturer(config.deviceManufacturer.c_str());
  ha.device.setModel(config.deviceModel.c_str());
  // ha_device.setConfigurationUrl("http://192.168.1.55:1234");
  
  // set up the availability options and LWT
  // This method enables availability for all device types registered on the device.
  // For example, if you have 5 sensors on the same device, you can enable
  // shared availability and change availability state of all sensors using
  // single method call "device.setAvailability(false|true)"
  ha.device.enableSharedAvailability();

  // Optionally, you can enable MQTT LWT feature. If device will lose connection
  // to the broker, all device types related to it will be marked as offline in
  // the Home Assistant Panel.
  ha.device.enableLastWill();

  // [2] -- set up the HA control plane --

  logStatus("Setting up subsystems and connecting HA control plane...");
  setupSmartMeter();     
    
  // [3] -- connect to the WiFiClient and MQTT --

  logStatus("Connecting to MQTT Broker...");
  // Initialize the HAMqtt object

  while(!ha.mqtt.begin(config.mqttBrokerAddress, config.secretMqttUser.c_str(), config.secretMqttPassword.c_str())) {
    logError("Retrying in 5 seconds...");
    delay(5000);
  }
  
  logStatus("Connected to MQTT Broker");
  ha.device.publishAvailability();

}