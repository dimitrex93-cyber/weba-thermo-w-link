#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "config.h"
#include "types.h"
#include <cmath>

// ============================================================
// POWER MANAGER CLASS
// ============================================================
class PowerManager {
private:
  EnergyData energyData = {0};
  uint32_t lastHeatingStartTime = 0;
  bool heatingActive = false;
  
public:
  void initialize() {
    energyData.lastUpdateTime = millis() / 1000;
    energyData.totalEnergy_Wh = 0.0f;
    energyData.peakPower_W = 0.0f;
    energyData.heatingCycles = 0;
  }
  
  void startHeating() {
    heatingActive = true;
    lastHeatingStartTime = millis() / 1000;
    energyData.heatingCycles++;
    
    #if DEBUG_SERIAL
    Serial.printf(">> Heating started (cycle %u)\n", energyData.heatingCycles);
    #endif
  }
  
  void stopHeating() {
    heatingActive = false;
    
    #if DEBUG_SERIAL
    Serial.printf(">> Heating stopped (total: %.1f Wh)\n", energyData.totalEnergy_Wh);
    #endif
  }
  
  void updateEnergy(float powerW, uint32_t currentTime) {
    if (!heatingActive) return;
    
    uint32_t deltaTime = currentTime - energyData.lastUpdateTime;
    if (deltaTime == 0) return;
    
    // Energy = Power * Time (W * s / 3600 = Wh)
    float deltaEnergy_Wh = (powerW * deltaTime) / 3600.0f;
    energyData.totalEnergy_Wh += deltaEnergy_Wh;
    
    // Track peak power
    if (powerW > energyData.peakPower_W) {
      energyData.peakPower_W = powerW;
    }
    
    energyData.lastUpdateTime = currentTime;
    
    #if DEBUG_SERIAL
    static uint32_t lastLog = 0;
    if (currentTime - lastLog > ENERGY_LOG_INTERVAL_S) {
      Serial.printf("Energy: %.1f Wh | Peak: %.1f W\n", 
                    energyData.totalEnergy_Wh, energyData.peakPower_W);
      lastLog = currentTime;
    }
    #endif
  }
  
  void checkBatteryHealth(float voltage, bool& shouldStopHeating) {
    shouldStopHeating = false;
    
    if (voltage < BATTERY_CRITICAL_VOLTAGE) {
      #if DEBUG_SERIAL
      Serial.println("! CRITICAL: Battery voltage too low!");
      #endif
      shouldStopHeating = true;
    } else if (voltage < BATTERY_LOW_VOLTAGE) {
      #if DEBUG_SERIAL
      Serial.println("! WARNING: Battery low voltage");
      #endif
    }
  }
  
  uint16_t calculateSleepTime(bool heatingActive) const {
    return heatingActive ? ACTIVE_SLEEP_S : NORMAL_SLEEP_S;
  }
  
  float getEnergyConsumption() const {
    return energyData.totalEnergy_Wh;
  }
  
  float getPeakPower() const {
    return energyData.peakPower_W;
  }
  
  uint16_t getHeatingCycles() const {
    return energyData.heatingCycles;
  }
};

#endif
