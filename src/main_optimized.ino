#include <Wire.h>
#include <esp_sleep.h>

#include "config.h"
#include "types.h"
#include "ina226_optimized.h"
#include "sensors.h"
#include "display_manager.h"
#include "wbus_controller.h"
#include "power_manager.h"
#include "data_manager.h"

// ============================================================
// DEVICE OBJECTS
// ============================================================
SensorManager sensors;
DisplayManager display;
WBUSController wbus;
PowerManager power;
DataManager data;

RuntimeState state = {0};

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println("\n\n>>> Webasto Thermo W-Link (Optimized) started");
  
  // Initialize subsystems
  Wire.begin();
  Wire.setClock(I2C_CLOCK);
  
  if (!sensors.initialize()) {
    Serial.println("! Sensor initialization failed");
  }
  
  if (!display.initialize()) {
    Serial.println("! Display initialization failed");
  }
  
  wbus.initialize();
  power.initialize();
  
  if (!data.initialize()) {
    Serial.println("! RTC data initialization failed");
  }
  
  // Handle wake reason
  rst_info *resetInfo = ESP.getResetInfoPtr();
  handleWakeReason(resetInfo);
  
  // Go to sleep
  goToSleep();
}

void loop() {
  // Never reached due to deep sleep
}

// ============================================================
// WAKE REASON HANDLING
// ============================================================
void handleWakeReason(rst_info* resetInfo) {
  bool funkSignalActive = (resetInfo->reason == REASON_EXT_SYS_RST) && 
                          (digitalRead(FUNK_PIN) == LOW);
  
  if (funkSignalActive) {
    handleFunkSignal();
  } else {
    handleTimerWake();
  }
}

void handleFunkSignal() {
  #if DEBUG_SERIAL
  Serial.println(">> 433 MHz signal received");
  #endif
  
  display.showMessage("Funk-Signal", "Heizung 30min");
  
  wbus.begin();
  if (wbus.startHeater()) {
    data.setHeatingActive(true, millis() / 1000);
    power.startHeating();
    state.restzeit = HEATING_DURATION_S;
  } else {
    display.showMessage("Fehler", "Heater offline");
  }
  wbus.end();
}

void handleTimerWake() {
  state.aktuelleZeit = millis() / 1000;
  
  // Read all sensors
  sensors.readAll(state);
  
  // Check battery health
  bool shouldStopHeating = false;
  power.checkBatteryHealth(state.batterieSpannung, shouldStopHeating);
  
  if (data.isHeatingActive()) {
    processHeatingStatus(shouldStopHeating);
  }
}

void processHeatingStatus(bool forcedStop) {
  uint32_t elapsedTime = data.getHeatingElapsedTime(state.aktuelleZeit);
  
  if (forcedStop || elapsedTime >= HEATING_DURATION_S) {
    // Stop heater
    wbus.begin();
    if (wbus.stopHeater()) {
      display.showMessage("Heizung aus", "OK");
    }
    wbus.end();
    
    data.setHeatingActive(false, 0);
    power.stopHeating();
    state.restzeit = 0;
  } else {
    // Still heating
    state.restzeit = HEATING_DURATION_S - elapsedTime;
    
    // Update energy tracking
    power.updateEnergy(state.batterieLeistung, state.aktuelleZeit);
    
    // Update display if needed
    uint32_t timeSinceLastUpdate = state.aktuelleZeit - data.getLastDisplayTime();
    if (timeSinceLastUpdate >= DISPLAY_UPDATE_INTERVAL_S) {
      display.turnOn();
      display.update(state);
      data.updateLastDisplayTime(state.aktuelleZeit);
    }
  }
}

// ============================================================
// SLEEP MANAGEMENT
// ============================================================
void goToSleep() {
  // Update data before sleep
  data.updateEnergy(power.getEnergyConsumption());
  data.updateLastTemp(state.innenTemperatur);
  data.save();
  
  // Turn off all peripherals
  display.turnOff();
  wbus.end();
  
  Serial.flush();
  
  // Calculate sleep time
  uint16_t sleepSeconds = power.calculateSleepTime(data.isHeatingActive());
  uint64_t sleepTimeUs = sleepSeconds * 1000000ULL;
  
  #if DEBUG_SERIAL
  Serial.printf(">> Deep sleep for %u seconds\n", sleepSeconds);
  #endif
  
  delay(10);
  
  // Configure wake sources
  esp_sleep_enable_timer_wakeup(sleepTimeUs);
  esp_sleep_enable_ext0_wakeup(GPIO_ID_PIN(14), LOW);  // 433 MHz signal on GPIO14 (D5)
  
  // Enter deep sleep
  ESP.deepSleep(0);
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================
void printDebugInfo() {
  #if DEBUG_SERIAL
  Serial.println("\n=== DEBUG INFO ===");
  Serial.printf("Temperature: %.1f°C\n", state.innenTemperatur);
  Serial.printf("Humidity: %.0f%%\n", state.innenLuftfeuchtigkeit);
  Serial.printf("Pressure: %.0f hPa\n", state.luftdruck);
  Serial.printf("Battery: %.2fV | %.1fmA | %.1fmW\n", 
                state.batterieSpannung, state.batterieStrom * 1000, state.batterieLeistung * 1000);
  Serial.printf("Heating: %s (%u s remaining)\n", 
                data.isHeatingActive() ? "YES" : "NO", state.restzeit);
  Serial.printf("Energy: %.1f Wh\n", power.getEnergyConsumption());
  Serial.println("==================\n");
  #endif
}
