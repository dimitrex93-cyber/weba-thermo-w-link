#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

// ============================================================
// ENUMERATIONS
// ============================================================
enum DisplayPage : uint8_t {
  PAGE_STATUS = 0,
  PAGE_ENVIRONMENT = 1,
  PAGE_BATTERY = 2,
  PAGE_COUNT = 3
};

enum SensorStatus : uint8_t {
  SENSOR_OK = 0,
  SENSOR_FAILED = 1,
  SENSOR_NOT_INITIALIZED = 2
};

// ============================================================
// RTC MEMORY STRUCTURE (Optimized for minimal size)
// ============================================================
#pragma pack(push, 1)
struct RTCData {
  uint32_t crc32;                    // 4 bytes
  uint32_t heizungStartZeit;         // 4 bytes
  uint32_t letzteDisplayAktualisierung; // 4 bytes
  uint32_t startTick;                // 4 bytes
  uint32_t gesamtEnergie_Wh_x100;    // 4 bytes (stored as x100 to use int)
  
  // Packed bit flags (1 byte instead of 4 bools)
  uint8_t heizungAktiv : 1;
  uint8_t aht20Aktiv : 1;
  uint8_t bmp280Aktiv : 1;
  uint8_t ina226Aktiv : 1;
  uint8_t displayAktiv : 1;
  uint8_t reserved : 3;
  
  uint8_t lastTemp_minus20;          // Temperature as int8_t (-20 to +80°C range)
  uint8_t lastHumidity;              // 0-100%
  uint8_t padding;                   // Alignment
};
#pragma pack(pop)

// ============================================================
// RUNTIME STATE (RAM - not persisted)
// ============================================================
struct RuntimeState {
  // Sensor readings
  float innenTemperatur;
  float innenLuftfeuchtigkeit;
  float luftdruck;
  
  // Battery readings (INA226)
  float batterieSpannung;
  float batterieStrom;
  float batterieLeistung;
  
  // Timing
  uint32_t aktuelleZeit;
  uint32_t restzeit;
  
  // Display state
  DisplayPage currentPage;
  uint32_t displayOnTime;
  
  // Sensor status
  uint8_t aht20Status : 2;
  uint8_t bmp280Status : 2;
  uint8_t ina226Status : 2;
  uint8_t reservedBits : 2;
};

// ============================================================
// ENERGY TRACKING STRUCTURE
// ============================================================
struct EnergyData {
  uint32_t lastUpdateTime;
  float totalEnergy_Wh;
  float peakPower_W;
  uint16_t heatingCycles;
};

#endif
