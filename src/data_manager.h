#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "config.h"
#include "types.h"
#include <cstdint>

// ============================================================
// DATA MANAGER CLASS - RTC & CRC Handling
// ============================================================
class DataManager {
private:
  RTCData rtcData = {0};
  bool rtcValid = false;
  
public:
  bool initialize() {
    ESP.rtcUserMemoryRead(0, (uint32_t*)&rtcData, sizeof(rtcData));
    rtcValid = validateRTC();
    
    if (!rtcValid) {
      #if DEBUG_SERIAL
      Serial.println("! RTC data invalid, resetting");
      #endif
      resetRTC();
    }
    
    return rtcValid;
  }
  
  bool validateRTC() {
    // Check if CRC is valid
    uint32_t calculatedCRC = calculateCRC32((uint8_t*)&rtcData, sizeof(rtcData) - 4);
    
    if (calculatedCRC != rtcData.crc32) {
      return false;
    }
    
    // Check for reasonable values
    if (rtcData.startTick > 1704067200) {  // Sanity check: after 2024-01-01
      return false;
    }
    
    return true;
  }
  
  void resetRTC() {
    memset(&rtcData, 0, sizeof(rtcData));
    rtcData.heizungAktiv = false;
    rtcData.heizungStartZeit = 0;
    rtcData.letzteDisplayAktualisierung = 0;
    rtcData.startTick = millis() / 1000;
    rtcData.gesamtEnergie_Wh_x100 = 0;
    rtcValid = true;
  }
  
  void save() {
    rtcData.crc32 = calculateCRC32((uint8_t*)&rtcData, sizeof(rtcData) - 4);
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));
    
    #if DEBUG_SERIAL
    Serial.printf(">> RTC data saved (CRC: 0x%08X)\n", rtcData.crc32);
    #endif
  }
  
  // Accessors with safety checks
  bool isHeatingActive() const { return rtcData.heizungAktiv && rtcValid; }
  
  void setHeatingActive(bool active, uint32_t startTime) {
    rtcData.heizungAktiv = active;
    if (active) {
      rtcData.heizungStartZeit = startTime;
    }
  }
  
  uint32_t getHeatingStartTime() const { return rtcData.heizungStartZeit; }
  
  uint32_t getHeatingElapsedTime(uint32_t currentTime) const {
    if (!rtcData.heizungAktiv) return 0;
    return currentTime - rtcData.heizungStartZeit;
  }
  
  void updateLastTemp(float temp) {
    // Store temperature as int8_t (-20 to +80°C range)
    rtcData.lastTemp_minus20 = (uint8_t)(temp + 20);
  }
  
  void updateLastDisplayTime(uint32_t time) {
    rtcData.letzteDisplayAktualisierung = time;
  }
  
  uint32_t getLastDisplayTime() const {
    return rtcData.letzteDisplayAktualisierung;
  }
  
  void updateEnergy(float wh) {
    rtcData.gesamtEnergie_Wh_x100 = (uint32_t)(wh * 100.0f);
  }
  
  float getEnergy() const {
    return rtcData.gesamtEnergie_Wh_x100 / 100.0f;
  }
  
private:
  uint32_t calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = CRC32_INIT;
    
    while (length--) {
      crc ^= *data++;
      for (int i = 0; i < 8; i++) {
        crc = (crc & 1) ? (crc >> 1) ^ CRC32_POLY : (crc >> 1);
      }
    }
    
    return ~crc;
  }
};

#endif
