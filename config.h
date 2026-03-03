#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define FUNK_PIN D5                  // 433 MHz Signal from engine bay
#define W_BUS_TX D8                  // SoftwareSerial TX to SI9241A
#define W_BUS_RX D7                  // SoftwareSerial RX from SI9241A

// ============================================================
// TIMING CONFIGURATION (seconds)
// ============================================================
#define NORMAL_SLEEP_S 60            // Deep sleep when inactive
#define ACTIVE_SLEEP_S 10            // Deep sleep when heating active
#define HEATING_DURATION_S 1800      // 30 minutes total heating time
#define DISPLAY_UPDATE_INTERVAL_S 10 // Update display every 10s when active
#define DISPLAY_TIMEOUT_S 30         // Auto-off display after 30s
#define WBUS_RESPONSE_TIMEOUT_MS 1000
#define I2C_INIT_DELAY_MS 10
#define WBUS_INIT_DELAY_MS 50

// ============================================================
// SERIAL & I2C CONFIGURATION
// ============================================================
#define WBUS_BAUD 2400
#define I2C_CLOCK 100000
#define SERIAL_BAUD 115200

// ============================================================
// SENSOR CALIBRATION
// ============================================================
#define INA226_ADDR 0x40
#define INA226_SHUNT_RESISTOR 0.01f  // 0.01 Ohm shunt
#define INA226_MAX_CURRENT 20.0f      // Max 20A for calibration

// ============================================================
// BATTERY THRESHOLDS
// ============================================================
#define BATTERY_LOW_VOLTAGE 10.5f
#define BATTERY_CRITICAL_VOLTAGE 9.5f

// ============================================================
// SENSOR UPDATE INTERVALS
// ============================================================
#define TEMP_READ_INTERVAL_S 10      // Read temp every 10s when inactive
#define TEMP_READ_INTERVAL_ACTIVE_S 5 // Read every 5s when heating
#define BATTERY_READ_INTERVAL_S 5    // Always read battery every 5s

// ============================================================
// DISPLAY BUFFER SIZE
// ============================================================
#define DISPLAY_BUFFER_SIZE 24

// ============================================================
// CRC & DATA VALIDATION
// ============================================================
#define CRC32_POLY 0xEDB88320
#define CRC32_INIT 0xffffffff

// ============================================================
// ENERGY TRACKING
// ============================================================
#define ENERGY_TRACKING_ENABLED 1
#define ENERGY_LOG_INTERVAL_S 60

// ============================================================
// DEBUG FLAGS
// ============================================================
#define DEBUG_SERIAL 1
#define DEBUG_INA226 1
#define DEBUG_SENSORS 0

#endif
