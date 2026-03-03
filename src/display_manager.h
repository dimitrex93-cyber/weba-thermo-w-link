#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include "config.h"
#include "types.h"

// ============================================================
// DISPLAY MANAGER CLASS
// ============================================================
class DisplayManager {
private:
  U8G2_SSD1306_128X64_NONAME_1_HW_I2C display;
  DisplayPage currentPage = PAGE_STATUS;
  uint32_t displayOnTime = 0;
  bool displayActive = false;
  uint32_t lastUpdateTime = 0;
  
  static constexpr uint8_t FONT_HEIGHT = 12;
  
public:
  DisplayManager() : display(U8G2_R0, U8X8_PIN_NONE) {}
  
  bool initialize() {
    if (!display.begin()) {
      Serial.println("! OLED initialization failed");
      return false;
    }
    display.setPowerSave(1);  // Off by default
    displayActive = false;
    return true;
  }
  
  void turnOn() {
    if (!displayActive) {
      display.setPowerSave(0);
      displayActive = true;
      displayOnTime = millis() / 1000;
    }
  }
  
  void turnOff() {
    if (displayActive) {
      display.setPowerSave(1);
      displayActive = false;
    }
  }
  
  bool isActive() const { return displayActive; }
  
  // Check if display should auto-off
  void updateAutoOff() {
    if (!displayActive) return;
    
    uint32_t now = millis() / 1000;
    if (now - displayOnTime > DISPLAY_TIMEOUT_S) {
      turnOff();
    }
  }
  
  // Show message and auto-off
  void showMessage(const char* line1, const char* line2) {
    turnOn();
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(0, 25, line1);
    display.drawStr(0, 45, line2);
    display.sendBuffer();
    delay(2000);
    turnOff();
  }
  
  // Update display with rotation
  void update(const RuntimeState& state) {
    uint32_t now = millis() / 1000;
    if (!displayActive || (now - lastUpdateTime < DISPLAY_UPDATE_INTERVAL_S)) {
      return;
    }
    
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB08_tr);
    
    switch (currentPage) {
      case PAGE_STATUS:
        drawPageStatus(state);
        break;
      case PAGE_ENVIRONMENT:
        drawPageEnvironment(state);
        break;
      case PAGE_BATTERY:
        drawPageBattery(state);
        break;
    }
    
    // Draw page indicator at bottom
    drawPageIndicator();
    display.sendBuffer();
    
    // Rotate to next page
    currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
    lastUpdateTime = now;
  }
  
private:
  void drawPageStatus(const RuntimeState& state) {
    display.drawStr(0, 12, "Weba Thermo W-Link");
    
    char buffer[DISPLAY_BUFFER_SIZE];
    if (state.restzeit > 0) {
      display.drawStr(0, 28, "Heizung AN");
      uint16_t minutes = state.restzeit / 60;
      uint16_t seconds = state.restzeit % 60;
      snprintf(buffer, sizeof(buffer), "%02u:%02u", minutes, seconds);
      display.drawStr(85, 28, buffer);
    } else {
      display.drawStr(0, 28, "Heizung AUS");
    }
    
    snprintf(buffer, sizeof(buffer), "Batt: %.1fV", state.batterieSpannung);
    display.drawStr(0, 55, buffer);
  }
  
  void drawPageEnvironment(const RuntimeState& state) {
    display.drawStr(0, 12, "Umgebung");
    
    char buffer[DISPLAY_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "T: %.1fC", state.innenTemperatur);
    display.drawStr(0, 28, buffer);
    
    snprintf(buffer, sizeof(buffer), "H: %.0f%%", state.innenLuftfeuchtigkeit);
    display.drawStr(0, 44, buffer);
    
    snprintf(buffer, sizeof(buffer), "P: %.0f hPa", state.luftdruck);
    display.drawStr(0, 60, buffer);
  }
  
  void drawPageBattery(const RuntimeState& state) {
    display.drawStr(0, 12, "Batterie");
    
    char buffer[DISPLAY_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "U: %.2fV", state.batterieSpannung);
    display.drawStr(0, 28, buffer);
    
    float currentMa = state.batterieStrom * 1000.0f;
    snprintf(buffer, sizeof(buffer), "I: %.0fmA", currentMa);
    display.drawStr(0, 44, buffer);
    
    snprintf(buffer, sizeof(buffer), "P: %.1fW", state.batterieLeistung);
    display.drawStr(0, 60, buffer);
  }
  
  void drawPageIndicator() {
    // Draw page dots (1-3)
    uint8_t dotX = 60;
    for (uint8_t i = 0; i < PAGE_COUNT; i++) {
      if (i == currentPage) {
        display.drawBox(dotX, 62, 3, 2);  // Filled dot for current page
      } else {
        display.drawFrame(dotX, 62, 3, 2);  // Empty dot for other pages
      }
      dotX += 6;
    }
  }
};

#endif
