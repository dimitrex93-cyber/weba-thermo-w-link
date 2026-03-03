#ifndef WBUS_CONTROLLER_H
#define WBUS_CONTROLLER_H

#include <SoftwareSerial.h>
#include "config.h"

// ============================================================
// W-BUS COMMAND DEFINITIONS
// ============================================================
static constexpr uint8_t WBUS_START_CMD[] = {0x44, 0x05, 0x21, 0x01, 0x01, 0x00, 0x6A};
static constexpr uint8_t WBUS_STOP_CMD[] = {0x44, 0x05, 0x21, 0x00, 0x01, 0x00, 0x6B};
static constexpr size_t WBUS_CMD_SIZE = 7;

// ============================================================
// W-BUS CONTROLLER CLASS
// ============================================================
class WBUSController {
private:
  SoftwareSerial serial;
  bool initialized = false;
  uint32_t lastCommandTime = 0;
  uint8_t commandRetries = 0;
  static constexpr uint8_t MAX_RETRIES = 2;
  
public:
  WBUSController() : serial(W_BUS_RX, W_BUS_TX) {}
  
  bool initialize() {
    // Don't start serial yet - only configure pins
    pinMode(W_BUS_TX, OUTPUT);
    pinMode(W_BUS_RX, INPUT);
    initialized = true;
    return true;
  }
  
  void begin() {
    if (initialized) {
      serial.begin(WBUS_BAUD);
      delay(WBUS_INIT_DELAY_MS);
    }
  }
  
  void end() {
    serial.end();
    pinMode(W_BUS_TX, INPUT);
    pinMode(W_BUS_RX, INPUT);
  }
  
  bool startHeater() {
    #if DEBUG_SERIAL
    Serial.println(">> Sending W-BUS START command");
    #endif
    return sendCommand(WBUS_START_CMD, WBUS_CMD_SIZE);
  }
  
  bool stopHeater() {
    #if DEBUG_SERIAL
    Serial.println(">> Sending W-BUS STOP command");
    #endif
    return sendCommand(WBUS_STOP_CMD, WBUS_CMD_SIZE);
  }
  
private:
  bool sendCommand(const uint8_t* cmd, size_t cmdSize) {
    // Initialize on first send
    if (!serial) {
      begin();
    }
    
    // Simple debounce: don't send too frequently
    uint32_t now = millis();
    if (now - lastCommandTime < 100) {
      delay(100 - (now - lastCommandTime));
    }
    
    serial.listen();
    delay(WBUS_INIT_DELAY_MS);
    
    // Send command
    for (size_t i = 0; i < cmdSize; i++) {
      serial.write(cmd[i]);
    }
    serial.flush();
    
    // Wait for response
    bool hasResponse = waitForResponse();
    
    if (hasResponse) {
      commandRetries = 0;
      lastCommandTime = millis();
      return true;
    } else {
      commandRetries++;
      if (commandRetries < MAX_RETRIES) {
        #if DEBUG_SERIAL
        Serial.printf(">> Retry command (%d/%d)\n", commandRetries, MAX_RETRIES);
        #endif
        return sendCommand(cmd, cmdSize);
      }
      
      #if DEBUG_SERIAL
      Serial.println("! W-BUS command failed after retries");
      #endif
      return false;
    }
  }
  
  bool waitForResponse() {
    uint32_t timeout = millis() + WBUS_RESPONSE_TIMEOUT_MS;
    
    while (millis() < timeout) {
      if (serial.available()) {
        uint8_t response = serial.read();
        #if DEBUG_SERIAL
        Serial.printf(">> W-BUS response: 0x%02X\n", response);
        #endif
        return true;
      }
      delay(10);
    }
    
    #if DEBUG_SERIAL
    Serial.println("! W-BUS timeout");
    #endif
    return false;
  }
};

#endif
