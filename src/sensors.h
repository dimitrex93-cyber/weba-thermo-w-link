#ifndef SENSORS_H
#define SENSORS_H

#include <AHT20.h>
#include <Adafruit_BMP280.h>
#include "config.h"
#include "types.h"
#include "ina226_optimized.h"

// ============================================================
// SENSOR MANAGER CLASS
// ============================================================
class SensorManager {
private:
  AHT20 aht20;
  Adafruit_BMP280 bmp280;
  INA226Battery ina226;
  
  uint32_t lastAHT20Read = 0;
  uint32_t lastBMP280Read = 0;
  uint32_t lastINA226Read = 0;
  uint32_t lastBatteryCheck = 0;
  
  uint8_t failureCount = 0;
  static constexpr uint8_t MAX_FAILURES = 3;
  
public:
  SensorManager() : ina226(INA226_ADDR, INA226_SHUNT_RESISTOR) {}
  
  bool initialize() {
    bool allOk = true;
    
    // Initialize AHT20
    if (!aht20.begin()) {
      Serial.println("! AHT20 initialization failed");
      allOk = false;
    }
    
    // Initialize BMP280
    if (!bmp280.begin()) {
      Serial.println("! BMP280 initialization failed");
      allOk = false;
    } else {
      bmp280.setSampling(Adafruit_BMP280::MODE_FORCED,
                         Adafruit_BMP280::SAMPLING_X2,
                         Adafruit_BMP280::SAMPLING_X16,
                         Adafruit_BMP280::FILTER_OFF,
                         Adafruit_BMP280::STANDBY_MS_1);
    }
    
    // Initialize INA226
    if (!ina226.begin()) {
      Serial.println("! INA226 initialization failed");
      allOk = false;
    }
    
    return allOk;
  }
  
  // Read only when needed (interval-based)
  void readAll(RuntimeState& state, bool forceRead = false) {
    uint32_t now = millis() / 1000;
    
    // Determine read interval based on heater state
    uint16_t tempInterval = state.restzeit > 0 ? TEMP_READ_INTERVAL_ACTIVE_S : TEMP_READ_INTERVAL_S;
    
    // Read AHT20 (temperature & humidity)
    if (forceRead || (now - lastAHT20Read >= tempInterval)) {
      readAHT20(state);
      lastAHT20Read = now;
    }
    
    // Read BMP280 (pressure) - less frequent
    if (forceRead || (now - lastBMP280Read >= 30)) {
      readBMP280(state);
      lastBMP280Read = now;
    }
    
    // Always read INA226 (battery critical)
    if (now - lastINA226Read >= BATTERY_READ_INTERVAL_S) {
      readINA226(state);
      lastINA226Read = now;
    }
  }
  
  void readBatteryOnly(RuntimeState& state) {
    uint32_t now = millis() / 1000;
    if (now - lastINA226Read >= BATTERY_READ_INTERVAL_S) {
      readINA226(state);
      lastINA226Read = now;
    }
  }
  
private:
  void readAHT20(RuntimeState& state) {
    if (!aht20.available()) {
      failureCount++;
      if (failureCount > MAX_FAILURES) {
        state.aht20Status = SENSOR_FAILED;
      }
      return;
    }
    
    state.innenTemperatur = aht20.getTemperature();
    state.innenLuftfeuchtigkeit = aht20.getHumidity();
    state.aht20Status = SENSOR_OK;
    failureCount = 0;
    
    #if DEBUG_SENSORS
    Serial.printf("AHT20: T=%.1f°C H=%.0f%%\n", 
                  state.innenTemperatur, state.innenLuftfeuchtigkeit);
    #endif
  }
  
  void readBMP280(RuntimeState& state) {
    state.luftdruck = bmp280.readPressure() / 100.0f;
    state.bmp280Status = SENSOR_OK;
    
    #if DEBUG_SENSORS
    Serial.printf("BMP280: P=%.0f hPa\n", state.luftdruck);
    #endif
  }
  
  void readINA226(RuntimeState& state) {
    float voltage, current, power;
    ina226.readAll(voltage, current, power);
    
    state.batterieSpannung = voltage;
    state.batterieStrom = current;
    state.batterieLeistung = power;
    state.ina226Status = SENSOR_OK;
    
    #if DEBUG_INA226
    Serial.printf("Battery: %.2fV | %.1fmA | %.1fmW\n", 
                  voltage, current * 1000, power * 1000);
    #endif
  }
};

#endif
