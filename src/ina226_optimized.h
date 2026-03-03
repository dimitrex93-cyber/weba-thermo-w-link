#ifndef INA226_OPTIMIZED_H
#define INA226_OPTIMIZED_H

#include <Wire.h>
#include <cstdint>
#include "config.h"

// ============================================================
// INA226 BATTERY MONITOR CLASS (Optimized)
// ============================================================
class INA226Battery {
private:
  uint8_t address;
  float shuntResistor;
  float currentLSB;           // Current LSB for conversions
  float powerLSB;             // Power LSB for conversions
  
  static constexpr uint8_t REG_CONFIG = 0x00;
  static constexpr uint8_t REG_SHUNT_VOLTAGE = 0x01;
  static constexpr uint8_t REG_BUS_VOLTAGE = 0x02;
  static constexpr uint8_t REG_POWER = 0x03;
  static constexpr uint8_t REG_CURRENT = 0x04;
  static constexpr uint8_t REG_CALIBRATION = 0x05;
  
  static constexpr float SHUNT_VOLTAGE_LSB = 0.0000025f;  // 2.5 µV per LSB
  static constexpr float BUS_VOLTAGE_LSB = 0.00125f;      // 1.25 mV per LSB
  static constexpr uint16_t CONFIG_VALUE = 0x4527;        // Continuous mode
  
  int16_t readRegister(uint8_t reg) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    
    if (Wire.requestFrom(address, (uint8_t)2) < 2) return 0;
    
    int16_t result = ((int16_t)Wire.read() << 8) | Wire.read();
    return result;
  }
  
  void writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)value);
    Wire.endTransmission();
  }
  
public:
  INA226Battery(uint8_t addr, float shunt) 
    : address(addr), shuntResistor(shunt) {
    // Calculate LSBs for maximum accuracy
    currentLSB = INA226_MAX_CURRENT / 32768.0f;
    powerLSB = currentLSB * 25.0f;
  }
  
  bool begin() {
    // Initialize configuration register
    writeRegister(REG_CONFIG, CONFIG_VALUE);
    delay(I2C_INIT_DELAY_MS);
    
    // Set calibration register for max current range
    uint16_t calibration = (uint16_t)((0.04096f) / (INA226_MAX_CURRENT * shuntResistor));
    writeRegister(REG_CALIBRATION, calibration);
    
    #if DEBUG_INA226
    Serial.printf(">> INA226 calibration: 0x%04X\n", calibration);
    #endif
    
    return true;
  }
  
  // Optimized: Read all values in one burst
  void readAll(float& voltage, float& current, float& power) {
    int16_t busVoltageRaw = readRegister(REG_BUS_VOLTAGE);
    int16_t currentRaw = readRegister(REG_CURRENT);
    int16_t powerRaw = readRegister(REG_POWER);
    
    // Voltage: Bits 15-3 are significant (12-bit), multiply by 1.25mV
    voltage = (busVoltageRaw >> 3) * BUS_VOLTAGE_LSB;
    
    // Current: 16-bit signed value
    current = currentRaw * currentLSB;
    
    // Power: 16-bit value
    power = powerRaw * powerLSB;
  }
  
  float readBusVoltage() {
    int16_t raw = readRegister(REG_BUS_VOLTAGE);
    return (raw >> 3) * BUS_VOLTAGE_LSB;
  }
  
  float readCurrent() {
    int16_t raw = readRegister(REG_CURRENT);
    return raw * currentLSB;
  }
  
  float readPower() {
    int16_t raw = readRegister(REG_POWER);
    return raw * powerLSB;
  }
};

#endif
