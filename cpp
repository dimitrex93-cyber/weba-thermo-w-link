#include <SoftwareSerial.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <AHT20.h>
#include <Adafruit_BMP280.h>
#include <esp_sleep.h>
#include <cmath>

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define FUNK_PIN D5         // 433 MHz Signal from engine bay
#define W_BUS_TX D8         // SoftwareSerial TX to SI9241A
#define W_BUS_RX D7         // SoftwareSerial RX from SI9241A

// ============================================================
// INA226 BATTERY MONITOR CLASS (R010 Shunt)
// ============================================================
class INA226Battery {
private:
  uint8_t address;
  float shuntResistor;
  
  int16_t readRegister(uint8_t reg) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(address, (uint8_t)2);
    
    int16_t result = 0;
    if (Wire.available() >= 2) {
      result = (Wire.read() << 8) | Wire.read();
    }
    return result;
  }
  
public:
  static constexpr uint8_t REG_CONFIG = 0x00;
  static constexpr uint8_t REG_SHUNT_VOLTAGE = 0x01;
  static constexpr uint8_t REG_BUS_VOLTAGE = 0x02;
  static constexpr uint8_t REG_POWER = 0x03;
  static constexpr uint8_t REG_CURRENT = 0x04;
  static constexpr uint8_t REG_CALIBRATION = 0x05;
  
  INA226Battery(uint8_t addr, float shunt) : address(addr), shuntResistor(shunt) {}
  
  void begin() {
    Wire.beginTransmission(address);
    Wire.write(REG_CONFIG);
    Wire.write(0x45);  // continuous mode
    Wire.write(0x27);  // defaults
    Wire.endTransmission();
    delay(10);
  }
  
  float readBusVoltage() {
    int16_t raw = readRegister(REG_BUS_VOLTAGE);
    return (raw >> 3) * 0.00125f;
  }
  
  float readShuntVoltage() {
    int16_t raw = readRegister(REG_SHUNT_VOLTAGE);
    return raw * 0.0000025f;
  }
  
  float readCurrent() {
    return readShuntVoltage() / shuntResistor;
  }
  
  float readPower() {
    return readBusVoltage() * readCurrent();
  }
};

// ============================================================
// CONSTANTS & CONFIGURATION
// ============================================================
constexpr uint64_t US_TO_S_FACTOR = 1000000ULL;
constexpr uint16_t NORMAL_SLEEP_S = 60;
constexpr uint16_t ACTIVE_SLEEP_S = 10;
constexpr uint16_t HEATING_DURATION_S = 1800;
constexpr uint16_t WBUS_BAUD = 2400;
constexpr uint8_t I2C_CLOCK = 100000;
constexpr uint16_t DISPLAY_UPDATE_INTERVAL_S = 10;

// INA226 Configuration
constexpr uint8_t INA226_ADDR = 0x40;
constexpr float INA226_SHUNT_RESISTOR = 0.01f;

// Display pages
enum DisplayPage {
  PAGE_STATUS = 0,
  PAGE_ENVIRONMENT = 1,
  PAGE_BATTERY = 2
};

// W-BUS Commands
constexpr uint8_t WBUS_START_CMD[] = {0x44, 0x05, 0x21, 0x01, 0x01, 0x00, 0x6A};
constexpr uint8_t WBUS_STOP_CMD[] = {0x44, 0x05, 0x21, 0x00, 0x01, 0x00, 0x6B};
constexpr size_t WBUS_CMD_SIZE = 7;

// ============================================================
// RTC MEMORY STRUCTURE
// ============================================================
struct RTCData {
  uint32_t crc32;
  bool heizungAktiv;
  uint32_t heizungStartZeit;
  uint32_t letzteDisplayAktualisierung;
  float letzteTemp;
  float gesamtEnergie_Wh;
  uint32_t startTick;
};

// ============================================================
// DEVICE OBJECTS
// ============================================================
SoftwareSerial wbus(W_BUS_RX, W_BUS_TX);
U8G2_SSD1306_128X64_NONAME_1_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
AHT20 aht20;
Adafruit_BMP280 bmp;
INA226Battery ina226(INA226_ADDR, INA226_SHUNT_RESISTOR);

// ============================================================
// RUNTIME STATE
// ============================================================
struct RuntimeState {
  bool displayAktiv = false;
  bool heizungAktiv = false;
  bool ina226Aktiv = false;
  uint32_t aktuelleZeit = 0;
  float innenTemperatur = 0.0f;
  float innenLuftfeuchtigkeit = 0.0f;
  float luftdruck = 0.0f;
  uint32_t restzeit = 0;
  
  // INA226 Battery readings
  float batterieSpannung = 0.0f;
  float batterieStrom = 0.0f;
  float batterieLeistung = 0.0f;
  
  DisplayPage currentPage = PAGE_STATUS;
} state;

RTCData rtcData;

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n>>> Webasto Thermo W-Link gestartet");
  
  initPins();
  initI2C();
  initSensors();
  initINA226Battery();
  initDisplay();
  initWBUS();
  
  loadRTCData();
  
  rst_info *resetInfo = ESP.getResetInfoPtr();
  if (shouldProcessFunkSignal(resetInfo)) {
    handleFunkSignal();
  } else {
    handleTimerWake();
  }
  
  goToSleep();
}

void loop() {}

// ============================================================
// INITIALIZATION FUNCTIONS
// ============================================================

void initPins() {
  pinMode(FUNK_PIN, INPUT_PULLUP);
  pinMode(W_BUS_TX, OUTPUT);
  pinMode(W_BUS_RX, INPUT);
}

void initI2C() {
  Wire.begin();
  Wire.setClock(I2C_CLOCK);
}

void initSensors() {
  if (!aht20.begin()) {
    Serial.println("! AHT20 initialization failed");
  }
  
  if (!bmp.begin()) {
    Serial.println("! BMP280 initialization failed");
  } else {
    bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_OFF,
                    Adafruit_BMP280::STANDBY_MS_1);
  }
}

void initINA226Battery() {
  ina226.begin();
  state.ina226Aktiv = true;
  Serial.println(">> INA226 initialized (R010)");
}

void initDisplay() {
  if (!display.begin()) {
    Serial.println("! OLED initialization failed");
  } else {
    display.setPowerSave(1);
  }
}

void initWBUS() {
  wbus.begin(WBUS_BAUD);
}

// ============================================================
// SENSOR READING
// ============================================================

void readSensors() {
  if (aht20.available()) {
    state.innenTemperatur = aht20.getTemperature();
    state.innenLuftfeuchtigkeit = aht20.getHumidity();
    rtcData.letzteTemp = state.innenTemperatur;
  }
  
  state.luftdruck = bmp.readPressure() / 100.0f;
  readBatterySensors();
}

void readBatterySensors() {
  if (!state.ina226Aktiv) return;
  
  state.batterieSpannung = ina226.readBusVoltage();
  state.batterieStrom = ina226.readCurrent();
  state.batterieLeistung = ina226.readPower();
  
  Serial.printf("Batterie: %.2fV | %.1fmA | %.1fmW\n", 
                state.batterieSpannung, 
                state.batterieStrom * 1000, 
                state.batterieLeistung * 1000);
}

// ============================================================
// DATA MANAGEMENT
// ============================================================

void loadRTCData() {
  ESP.rtcUserMemoryRead(0, (uint32_t*)&rtcData, sizeof(rtcData));
  state.heizungAktiv = rtcData.heizungAktiv;
  state.aktuelleZeit = millis() / 1000;
}

void saveRTCData() {
  rtcData.crc32 = calculateCRC32((uint8_t*)&rtcData, sizeof(rtcData) - 4);
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));
}

uint32_t calculateCRC32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
    }
  }
  return ~crc;
}

// ============================================================
// WAKE REASON HANDLING
// ============================================================

bool shouldProcessFunkSignal(rst_info* resetInfo) {
  return (resetInfo->reason == REASON_EXT_SYS_RST && digitalRead(FUNK_PIN) == LOW);
}

void handleFunkSignal() {
  Serial.println(">> 433 MHz signal - starting heater");
  displayShowMessage("Funk-Signal", "Heizung 30min");
  heizungStarten();
  
  rtcData.heizungAktiv = true;
  rtcData.heizungStartZeit = millis() / 1000;
  rtcData.letzteDisplayAktualisierung = rtcData.heizungStartZeit;
  rtcData.startTick = millis() / 1000;
  rtcData.gesamtEnergie_Wh = 0.0f;
}

void handleTimerWake() {
  readSensors();
  if (state.heizungAktiv) processHeatingStatus();
}

// ============================================================
// HEATER CONTROL
// ============================================================

void processHeatingStatus() {
  state.aktuelleZeit = millis() / 1000;
  uint32_t elapsedTime = state.aktuelleZeit - rtcData.heizungStartZeit;
  
  if (elapsedTime >= HEATING_DURATION_S) {
    heizungStoppen();
    rtcData.heizungAktiv = false;
    state.heizungAktiv = false;
    displayShowMessage("Heizung aus", "automatisch");
  } else {
    state.restzeit = HEATING_DURATION_S - elapsedTime;
    updateDisplayIfNeeded();
  }
}

bool shouldUpdateDisplay() {
  return (state.aktuelleZeit - rtcData.letzteDisplayAktualisierung >= DISPLAY_UPDATE_INTERVAL_S);
}

void heizungStarten() {
  wbus.listen();
  delay(10);
  wbus.write(WBUS_START_CMD, WBUS_CMD_SIZE);
  wbus.flush();
  state.heizungAktiv = true;
  Serial.println(">> Heater started");
}

void heizungStoppen() {
  wbus.write(WBUS_STOP_CMD, WBUS_CMD_SIZE);
  wbus.flush();
  state.heizungAktiv = false;
  Serial.println(">> Heater stopped");
}

// ============================================================
// DISPLAY FUNCTIONS
// ============================================================

void displayShowMessage(const char* line1, const char* line2) {
  displayOn();
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 25, line1);
  display.drawStr(0, 45, line2);
  display.sendBuffer();
  delay(2000);
  displayOff();
}

void updateDisplayIfNeeded() {
  if (shouldUpdateDisplay()) {
    displayOn();
    updateDisplay();
    rtcData.letzteDisplayAktualisierung = state.aktuelleZeit;
  }
}

void updateDisplay() {
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  
  switch (state.currentPage) {
    case PAGE_STATUS:
      displayPageStatus();
      break;
    case PAGE_ENVIRONMENT:
      displayPageEnvironment();
      break;
    case PAGE_BATTERY:
      displayPageBattery();
      break;
  }
  
  state.currentPage = (DisplayPage)((state.currentPage + 1) % 3);
  display.sendBuffer();
}

void displayPageStatus() {
  display.drawStr(0, 12, "Weba Thermo W-Link");
  
  char buffer[24];
  if (state.heizungAktiv) {
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

void displayPageEnvironment() {
  display.drawStr(0, 12, "Umgebung");
  
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "Temp: %.1f C", state.innenTemperatur);
  display.drawStr(0, 28, buffer);
  
  snprintf(buffer, sizeof(buffer), "Luft: %.0f%%", state.innenLuftfeuchtigkeit);
  display.drawStr(0, 44, buffer);
  
  snprintf(buffer, sizeof(buffer), "Druck: %.0f hPa", state.luftdruck);
  display.drawStr(0, 60, buffer);
}

void displayPageBattery() {
  display.drawStr(0, 12, "Batterie");
  
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "U: %.2fV", state.batterieSpannung);
  display.drawStr(0, 28, buffer);
  
  if (fabsf(state.batterieStrom) > 0.1f) {
    snprintf(buffer, sizeof(buffer), "I: %.0fmA", state.batterieStrom * 1000);
    display.drawStr(0, 44, buffer);
    snprintf(buffer, sizeof(buffer), "P: %.1fW", state.batterieLeistung);
    display.drawStr(0, 60, buffer);
  } else {
    display.drawStr(0, 44, "I: <100mA");
    snprintf(buffer, sizeof(buffer), "P: %.1fW", state.batterieLeistung);
    display.drawStr(0, 60, buffer);
  }
}

void displayOn() {
  if (!state.displayAktiv) {
    display.setPowerSave(0);
    state.displayAktiv = true;
  }
}

void displayOff() {
  if (state.displayAktiv) {
    display.setPowerSave(1);
    state.displayAktiv = false;
  }
}

// ============================================================
// SLEEP MANAGEMENT
// ============================================================

void goToSleep() {
  Serial.flush();
  
  displayOff();
  cleanupBeforeSleep();
  saveRTCData();
  
  uint64_t sleepTimeUs = calculateSleepTime() * US_TO_S_FACTOR;
  configureSleepWakeups(sleepTimeUs);
  
  Serial.printf(">> Sleep for %llu seconds\n", sleepTimeUs / US_TO_S_FACTOR);
  delay(10);
  ESP.deepSleep(0);
}

uint16_t calculateSleepTime() {
  return state.heizungAktiv ? ACTIVE_SLEEP_S : NORMAL_SLEEP_S;
}

void cleanupBeforeSleep() {
  wbus.end();
  pinMode(W_BUS_TX, INPUT);
  pinMode(W_BUS_RX, INPUT);
}

void configureSleepWakeups(uint64_t sleepTimeUs) {
  esp_sleep_enable_timer_wakeup(sleepTimeUs);
  esp_sleep_enable_ext0_wakeup(GPIO_ID_PIN(14), LOW);
}
