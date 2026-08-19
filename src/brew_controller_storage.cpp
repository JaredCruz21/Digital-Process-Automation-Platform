#include "brew_controller.h"

namespace {
constexpr float DEFAULT_T1 = 0.0f;
constexpr float DEFAULT_R1 = 100.00f;
constexpr float DEFAULT_T2 = 100.0f;
constexpr float DEFAULT_R2 = 138.51f;
constexpr float LEGACY_DEFAULT_R1 = 99.35f;
constexpr float LEGACY_DEFAULT_R2 = 136.30f;

constexpr int EEPROM_MAGIC_ADDR = 0;
constexpr uint8_t EEPROM_MAGIC = 0xA5;
constexpr int EEPROM_R1_ADDR = 1;
constexpr int EEPROM_T1_ADDR = EEPROM_R1_ADDR + sizeof(float);
constexpr int EEPROM_R2_ADDR = EEPROM_T1_ADDR + sizeof(float);
constexpr int EEPROM_T2_ADDR = EEPROM_R2_ADDR + sizeof(float);
constexpr int EEPROM_LEVEL_MAGIC_ADDR = EEPROM_T2_ADDR + sizeof(float);
constexpr uint8_t EEPROM_LEVEL_MAGIC = 0x5A;
constexpr int EEPROM_LEVEL_VOL1_ADDR = EEPROM_LEVEL_MAGIC_ADDR + 1;
constexpr int EEPROM_LEVEL_VOL2_ADDR = EEPROM_LEVEL_VOL1_ADDR + sizeof(float);
constexpr int EEPROM_LEVEL_DIS1_ADDR = EEPROM_LEVEL_VOL2_ADDR + sizeof(float);
constexpr int EEPROM_LEVEL_DIS2_ADDR = EEPROM_LEVEL_DIS1_ADDR + sizeof(float);
constexpr int EEPROM_FERM_TEMP_MAGIC_ADDR = EEPROM_LEVEL_DIS2_ADDR + sizeof(float);
constexpr uint8_t EEPROM_FERM_TEMP_MAGIC = 0xC3;
constexpr int EEPROM_FERM_R1_ADDR = EEPROM_FERM_TEMP_MAGIC_ADDR + 1;
constexpr int EEPROM_FERM_T1_ADDR = EEPROM_FERM_R1_ADDR + sizeof(float);
constexpr int EEPROM_FERM_R2_ADDR = EEPROM_FERM_T1_ADDR + sizeof(float);
constexpr int EEPROM_FERM_T2_ADDR = EEPROM_FERM_R2_ADDR + sizeof(float);

bool isLeapYear(int year) {
  return ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0));
}

uint8_t daysInMonth(int year, uint8_t month) {
  static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month == 2 && isLeapYear(year)) return 29;
  return days[month - 1];
}

void breakRtcEpoch(time_t epoch, int &year, uint8_t &month, uint8_t &day,
                   uint8_t &hour, uint8_t &minute, uint8_t &second) {
  uint32_t days = epoch / 86400UL;
  uint32_t rem = epoch % 86400UL;

  hour = rem / 3600UL;
  rem %= 3600UL;
  minute = rem / 60UL;
  second = rem % 60UL;

  year = 1970;
  while (true) {
    uint16_t diy = isLeapYear(year) ? 366 : 365;
    if (days < diy) break;
    days -= diy;
    year++;
  }

  month = 1;
  while (true) {
    uint8_t dim = daysInMonth(year, month);
    if (days < dim) break;
    days -= dim;
    month++;
  }

  day = days + 1;
}

String formatPacificTimestamp(time_t rtcEpoch) {
  if (rtcEpoch < 1704067200UL) return "RTC_NOT_SET";

  int year;
  uint8_t month, day, hour, minute, second;
  breakRtcEpoch(rtcEpoch, year, month, day, hour, minute, second);

  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02u:%02u:%02u PT",
           year, month, day, hour, minute, second);
  return String(buf);
}

String formatMax31865Fault(uint8_t fault) {
  if (fault == 0) return "OK";

  String text = "";
  if (fault & MAX31865_FAULT_HIGHTHRESH) text += "HIGH ";
  if (fault & MAX31865_FAULT_LOWTHRESH) text += "LOW ";
  if (fault & MAX31865_FAULT_REFINLOW) text += "REFINLOW ";
  if (fault & MAX31865_FAULT_REFINHIGH) text += "REFINHIGH ";
  if (fault & MAX31865_FAULT_RTDINLOW) text += "RTDINLOW ";
  if (fault & MAX31865_FAULT_OVUV) text += "OVUV ";
  text.trim();
  return text;
}

String formatTempCalMax31865Status(uint16_t rtd, float resistance, uint8_t fault) {
  (void)rtd;
  (void)resistance;
  return fault ? formatMax31865Fault(fault) : "";
}
}

bool readTextValue(NexText &obj, String &out) {
  char buffer[64] = {0};
  uint16_t len = obj.getText(buffer, sizeof(buffer));
  if (len == 0) {
    out = "";
    return false;
  }
  out = String(buffer);
  out.trim();
  return true;
}

bool readNumberWithRetry(NexNumber &obj, uint32_t &out, uint8_t attempts) {
  for (uint8_t i = 0; i < attempts; i++) {
    if (obj.getValue(&out)) return true;
    delay(20);
  }
  return false;
}

bool readComponentNumberValue(const char *objName, uint32_t &out, uint8_t attempts) {
  for (uint8_t attempt = 0; attempt < attempts; attempt++) {
    while (nexSerial.available()) {
      nexSerial.read();
    }

    nexSerial.print("get ");
    nexSerial.print(objName);
    nexSerial.print(".val");
    sendTerminator();

    unsigned long startMs = millis();
    while ((millis() - startMs) < 120UL) {
      if (nexSerial.available() < 8) {
        delay(2);
        continue;
      }

      if (nexSerial.read() != 0x71) {
        continue;
      }

      uint32_t value = 0;
      value |= (uint32_t)nexSerial.read();
      value |= (uint32_t)nexSerial.read() << 8;
      value |= (uint32_t)nexSerial.read() << 16;
      value |= (uint32_t)nexSerial.read() << 24;

      uint8_t end1 = nexSerial.read();
      uint8_t end2 = nexSerial.read();
      uint8_t end3 = nexSerial.read();

      if (end1 == 0xFF && end2 == 0xFF && end3 == 0xFF) {
        out = value;
        return true;
      }
      break;
    }

    delay(20);
  }

  out = 0;
  return false;
}

const char *temperatureProbeName(TemperatureProbe probe) {
  return probe == TEMP_PROBE_FERMENTATION ? "FermentationProbe1" : "ProcessProbe1";
}

void applyActiveTempProbeCalibration() {
  if (activeTempCalProbe == TEMP_PROBE_FERMENTATION) {
    R1_val = fermentationR1_val;
    T1_val = fermentationT1_val;
    R2_val = fermentationR2_val;
    T2_val = fermentationT2_val;
  } else {
    R1_val = processR1_val;
    T1_val = processT1_val;
    R2_val = processR2_val;
    T2_val = processT2_val;
  }
}

void storeActiveTempProbeCalibration() {
  if (activeTempCalProbe == TEMP_PROBE_FERMENTATION) {
    fermentationR1_val = R1_val;
    fermentationT1_val = T1_val;
    fermentationR2_val = R2_val;
    fermentationT2_val = T2_val;
  } else {
    processR1_val = R1_val;
    processT1_val = T1_val;
    processR2_val = R2_val;
    processT2_val = T2_val;
  }
}

bool readVolumeInputGallons(const char *objName, float &out, uint8_t attempts) {
  uint32_t raw = 0;
  if (!readComponentNumberValue(objName, raw, attempts)) {
    out = 0.0f;
    return false;
  }

  out = raw / 100.0f;
  return true;
}

void saveCalibrationToEEPROM() {
  storeActiveTempProbeCalibration();

  EEPROM.put(EEPROM_R1_ADDR, processR1_val);
  EEPROM.put(EEPROM_T1_ADDR, processT1_val);
  EEPROM.put(EEPROM_R2_ADDR, processR2_val);
  EEPROM.put(EEPROM_T2_ADDR, processT2_val);
  EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);

  EEPROM.put(EEPROM_FERM_R1_ADDR, fermentationR1_val);
  EEPROM.put(EEPROM_FERM_T1_ADDR, fermentationT1_val);
  EEPROM.put(EEPROM_FERM_R2_ADDR, fermentationR2_val);
  EEPROM.put(EEPROM_FERM_T2_ADDR, fermentationT2_val);
  EEPROM.update(EEPROM_FERM_TEMP_MAGIC_ADDR, EEPROM_FERM_TEMP_MAGIC);
}

void loadCalibrationFromEEPROM() {
  uint8_t magic = EEPROM.read(EEPROM_MAGIC_ADDR);
  bool shouldSave = false;

  if (magic == EEPROM_MAGIC) {
    EEPROM.get(EEPROM_R1_ADDR, processR1_val);
    EEPROM.get(EEPROM_T1_ADDR, processT1_val);
    EEPROM.get(EEPROM_R2_ADDR, processR2_val);
    EEPROM.get(EEPROM_T2_ADDR, processT2_val);

    bool usingLegacyDefaults =
      fabsf(processR1_val - LEGACY_DEFAULT_R1) < 0.01f &&
      fabsf(processT1_val - DEFAULT_T1) < 0.01f &&
      fabsf(processR2_val - LEGACY_DEFAULT_R2) < 0.01f &&
      fabsf(processT2_val - DEFAULT_T2) < 0.01f;

    // Migrate only the previous built-in defaults; preserve any real user calibration.
    if (usingLegacyDefaults) {
      processR1_val = DEFAULT_R1;
      processT1_val = DEFAULT_T1;
      processR2_val = DEFAULT_R2;
      processT2_val = DEFAULT_T2;
      shouldSave = true;
    }
  } else {
    processR1_val = DEFAULT_R1;
    processT1_val = DEFAULT_T1;
    processR2_val = DEFAULT_R2;
    processT2_val = DEFAULT_T2;
    shouldSave = true;
  }

  if (EEPROM.read(EEPROM_FERM_TEMP_MAGIC_ADDR) == EEPROM_FERM_TEMP_MAGIC) {
    EEPROM.get(EEPROM_FERM_R1_ADDR, fermentationR1_val);
    EEPROM.get(EEPROM_FERM_T1_ADDR, fermentationT1_val);
    EEPROM.get(EEPROM_FERM_R2_ADDR, fermentationR2_val);
    EEPROM.get(EEPROM_FERM_T2_ADDR, fermentationT2_val);
  } else {
    fermentationR1_val = DEFAULT_R1;
    fermentationT1_val = DEFAULT_T1;
    fermentationR2_val = DEFAULT_R2;
    fermentationT2_val = DEFAULT_T2;
    shouldSave = true;
  }

  activeTempCalProbe = TEMP_PROBE_PROCESS;
  applyActiveTempProbeCalibration();
  if (shouldSave) saveCalibrationToEEPROM();
}

void saveLevelCalibrationToEEPROM() {
  EEPROM.put(EEPROM_LEVEL_VOL1_ADDR, levelVol1);
  EEPROM.put(EEPROM_LEVEL_VOL2_ADDR, levelVol2);
  EEPROM.put(EEPROM_LEVEL_DIS1_ADDR, levelDis1);
  EEPROM.put(EEPROM_LEVEL_DIS2_ADDR, levelDis2);
  EEPROM.update(EEPROM_LEVEL_MAGIC_ADDR, EEPROM_LEVEL_MAGIC);
}

void loadLevelCalibrationFromEEPROM() {
  uint8_t magic = EEPROM.read(EEPROM_LEVEL_MAGIC_ADDR);

  if (magic == EEPROM_LEVEL_MAGIC) {
    EEPROM.get(EEPROM_LEVEL_VOL1_ADDR, levelVol1);
    EEPROM.get(EEPROM_LEVEL_VOL2_ADDR, levelVol2);
    EEPROM.get(EEPROM_LEVEL_DIS1_ADDR, levelDis1);
    EEPROM.get(EEPROM_LEVEL_DIS2_ADDR, levelDis2);
    computeLevelCalibration();
    Serial.println("Loaded level calibration from EEPROM");
  } else {
    levelCalValid = false;
    Serial.println("No saved level calibration found");
  }
}

float readRTDResistanceOhms() {
  uint16_t rtd = thermo.readRTD();
  return (rtd / 32768.0f) * RREF;
}

float calculateCalibratedTempC(float measuredResistance) {
  float denominator = (T2_val - T1_val);
  if (denominator == 0.0f || R1_val == 0.0f) return 0.0f;

  float alpha_cal = (R2_val - R1_val) / (R1_val * denominator);
  if (alpha_cal == 0.0f) return 0.0f;

  float R0_cal = R1_val / (1.0f + alpha_cal * T1_val);
  return (measuredResistance / R0_cal - 1.0f) / alpha_cal;
}

float calculateCalibratedTempCForProbe(float measuredResistance, TemperatureProbe probe) {
  float r1 = (probe == TEMP_PROBE_FERMENTATION) ? fermentationR1_val : processR1_val;
  float t1 = (probe == TEMP_PROBE_FERMENTATION) ? fermentationT1_val : processT1_val;
  float r2 = (probe == TEMP_PROBE_FERMENTATION) ? fermentationR2_val : processR2_val;
  float t2 = (probe == TEMP_PROBE_FERMENTATION) ? fermentationT2_val : processT2_val;

  float denominator = t2 - t1;
  if (denominator == 0.0f || r1 == 0.0f) return 0.0f;

  float alpha_cal = (r2 - r1) / (r1 * denominator);
  if (alpha_cal == 0.0f) return 0.0f;

  float r0_cal = r1 / (1.0f + alpha_cal * t1);
  return (measuredResistance / r0_cal - 1.0f) / alpha_cal;
}

float getLiveTempC() {
  float measuredResistance = readRTDResistanceOhms();
  bool fermentationRunning =
    (fermentationState == FERMENTATION_LOGGING || fermentationState == FERMENTATION_PITCHED);

  TemperatureProbe probe = TEMP_PROBE_PROCESS;
  if (currentPage == PAGE_TEMP_CAL) {
    probe = activeTempCalProbe;
  } else if (currentPage == PAGE_FERMENTATION || fermentationRunning) {
    probe = TEMP_PROBE_FERMENTATION;
  }

  return calculateCalibratedTempCForProbe(measuredResistance, probe);
}

float getLiveResistanceOhms() {
  return readRTDResistanceOhms();
}

bool initSDCard() {
  sdReady = SD.begin(SD_CHIP_SELECT);
  return sdReady;
}

void updateSDCardStatus(const String &status) {
  sendText("SDcard", status);
}

String sanitizeFileBaseName(String input) {
  input.trim();
  if (input.length() == 0) return "RUN_UNNAMED";

  String out = "";
  for (uint16_t i = 0; i < input.length(); i++) {
    char c = input[i];
    bool isAlphaNum =
      (c >= 'A' && c <= 'Z') ||
      (c >= 'a' && c <= 'z') ||
      (c >= '0' && c <= '9');

    if (isAlphaNum) out += c;
    else if (c == ' ' || c == '-' || c == '_') out += '_';
  }

  if (out.length() == 0) out = "RUN_UNNAMED";
  return out;
}

String makeUniqueCsvFileName(const String &requestedBase, bool &hadToRename) {
  hadToRename = false;

  String base = sanitizeFileBaseName(requestedBase);
  String candidate = base + ".csv";

  if (!SD.exists(candidate.c_str())) return candidate;

  hadToRename = true;
  for (int i = 1; i < 1000; i++) {
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "_%02d", i);
    candidate = base + String(suffix) + ".csv";
    if (!SD.exists(candidate.c_str())) return candidate;
  }
  return "";
}

bool createRunFileWithSetup() {
  if (!sdReady) return false;
  if (currentLogFileName.length() == 0) return false;

  File logFile = SD.open(currentLogFileName.c_str(), FILE_WRITE);
  if (!logFile) return false;

  if (logFile.size() == 0) {
    runLogActive = true;

    logFile.println("[SETUP]");
    logFile.print("run_name,"); logFile.println(runName);
    logFile.print("file_name,"); logFile.println(currentLogFileName);
    logFile.print("log_started_local_pacific,"); logFile.println(formatPacificTimestamp(Teensy3Clock.get()));
    logFile.print("log_started_rtc_epoch,"); logFile.println((uint32_t)Teensy3Clock.get());
    logFile.print("mash_duration_min,"); logFile.println(mashDurationMin);
    logFile.print("mash_temp_c,"); logFile.println(mashTempC, 2);
    logFile.print("boil_duration_min,"); logFile.println(boilDurationMin);
    logFile.print("process_temp_cal_R1,"); logFile.println(processR1_val, 2);
    logFile.print("process_temp_cal_T1,"); logFile.println(processT1_val, 2);
    logFile.print("process_temp_cal_R2,"); logFile.println(processR2_val, 2);
    logFile.print("process_temp_cal_T2,"); logFile.println(processT2_val, 2);
    logFile.print("fermentation_temp_cal_R1,"); logFile.println(fermentationR1_val, 2);
    logFile.print("fermentation_temp_cal_T1,"); logFile.println(fermentationT1_val, 2);
    logFile.print("fermentation_temp_cal_R2,"); logFile.println(fermentationR2_val, 2);
    logFile.print("fermentation_temp_cal_T2,"); logFile.println(fermentationT2_val, 2);
    logFile.print("level_vol1,"); logFile.println(levelVol1, 2);
    logFile.print("level_vol2,"); logFile.println(levelVol2, 2);
    logFile.print("level_dis1,"); logFile.println(levelDis1, 2);
    logFile.print("level_dis2,"); logFile.println(levelDis2, 2);
    logFile.println();
    logFile.println("[DATA]");
    logFile.println("timestamp_s,page,state,temp_c,setpoint_c,display_vol_gal,live_vol_gal,live_voltage_v,heater_on,pump_on,time_remaining_s,sg,resistance_ohms,boil_aggression_pct");
  }

  logFile.close();
  runLogActive = true;
  return true;
}

bool finishRunLog(const String &reason) {
  if (!sdReady || currentLogFileName.length() == 0 || !runLogActive) return false;

  File f = SD.open(currentLogFileName.c_str(), FILE_WRITE);
  if (!f) return false;

  unsigned long ts = millis() / 1000UL;
  f.print(ts);
  f.print(",EVENT,LOGGING_STOPPED,");
  f.println(reason);
  f.close();

  runLogActive = false;
  return true;
}

bool appendTempCalibrationLog() {
  if (!sdReady) return false;
  File f = SD.open("temp_cal_log.csv", FILE_WRITE);
  if (!f) return false;

  if (f.size() == 0) f.println("probe,R1,T1,R2,T2");
  f.print(temperatureProbeName(activeTempCalProbe)); f.print(",");
  f.print(R1_val, 2); f.print(",");
  f.print(T1_val, 2); f.print(",");
  f.print(R2_val, 2); f.print(",");
  f.println(T2_val, 2);

  f.close();
  return true;
}

bool appendRunLogLine(const String &page, const String &state, float tempC, float setpointC,
                      float displayVol, float liveVol, float liveVoltage,
                      bool heater, bool pump,
                      long timeRemainingSec, float sgValue, float resistanceOhms,
                      float controlAggressionPct) {
  if (!sdReady || currentLogFileName.length() == 0 || !runLogActive) return false;

  File f = SD.open(currentLogFileName.c_str(), FILE_WRITE);
  if (!f) return false;

  unsigned long ts = millis() / 1000UL;
  f.print(ts); f.print(",");
  f.print(page); f.print(",");
  f.print(state); f.print(",");
  f.print(tempC, 2); f.print(",");
  f.print(setpointC, 2); f.print(",");
  f.print(displayVol, 2); f.print(",");
  f.print(liveVol, 2); f.print(",");
  f.print(liveVoltage, 3); f.print(",");
  f.print(heater ? 1 : 0); f.print(",");
  f.print(pump ? 1 : 0); f.print(",");
  f.print(timeRemainingSec); f.print(",");
  f.print(sgValue, 3); f.print(",");
  f.print(resistanceOhms, 3); f.print(",");
  f.println(controlAggressionPct, 1);

  f.close();
  return true;
}

bool appendEventLog(const String &eventName, const String &detail) {
  if (!sdReady || currentLogFileName.length() == 0 || !runLogActive) return false;

  File f = SD.open(currentLogFileName.c_str(), FILE_WRITE);
  if (!f) return false;

  unsigned long ts = millis() / 1000UL;
  f.print(ts);
  f.print(",EVENT,");
  f.print(eventName);
  f.print(",");
  f.println(detail);

  f.close();
  return true;
}

bool createFermentationLogFile(const String &requestedName, bool &hadToRename) {
  hadToRename = false;

  if (!initSDCard()) {
    fermentationLogActive = false;
    return false;
  }

  fermentationLogFileName = makeUniqueCsvFileName(requestedName, hadToRename);
  if (fermentationLogFileName.length() == 0) {
    fermentationLogActive = false;
    return false;
  }

  File f = SD.open(fermentationLogFileName.c_str(), FILE_WRITE);
  if (!f) {
    fermentationLogActive = false;
    return false;
  }

  if (f.size() == 0) {
    f.println("[FERMENTATION]");
    f.print("run_name,"); f.println(requestedName);
    f.print("file_name,"); f.println(fermentationLogFileName);
    f.print("log_created_local_pacific,"); f.println(formatPacificTimestamp(Teensy3Clock.get()));
    f.print("log_created_rtc_epoch,"); f.println((uint32_t)Teensy3Clock.get());
    f.print("process_temp_cal_R1,"); f.println(processR1_val, 2);
    f.print("process_temp_cal_T1,"); f.println(processT1_val, 2);
    f.print("process_temp_cal_R2,"); f.println(processR2_val, 2);
    f.print("process_temp_cal_T2,"); f.println(processT2_val, 2);
    f.print("fermentation_temp_cal_R1,"); f.println(fermentationR1_val, 2);
    f.print("fermentation_temp_cal_T1,"); f.println(fermentationT1_val, 2);
    f.print("fermentation_temp_cal_R2,"); f.println(fermentationR2_val, 2);
    f.print("fermentation_temp_cal_T2,"); f.println(fermentationT2_val, 2);
    f.println();
    f.println("[DATA]");
    f.println("timestamp_s,state,temp_c,min_temp_c,max_temp_c,sixhr_avg_temp_c,avg_temp_c,elapsed_s,sg,resistance_ohms");
  }

  f.close();
  fermentationLogActive = true;
  return true;
}

bool appendFermentationLogLine(const String &state, float tempC, float minTempC,
                               float maxTempC, float sixHrAvgTempC, float avgTempC,
                               unsigned long elapsedSec, float sgValue,
                               float resistanceOhms) {
  if (!sdReady || fermentationLogFileName.length() == 0 || !fermentationLogActive) return false;

  File f = SD.open(fermentationLogFileName.c_str(), FILE_WRITE);
  if (!f) return false;

  unsigned long ts = millis() / 1000UL;
  f.print(ts); f.print(",");
  f.print(state); f.print(",");
  f.print(tempC, 2); f.print(",");
  f.print(minTempC, 2); f.print(",");
  f.print(maxTempC, 2); f.print(",");
  f.print(sixHrAvgTempC, 2); f.print(",");
  f.print(avgTempC, 2); f.print(",");
  f.print(elapsedSec); f.print(",");
  f.print(sgValue, 3); f.print(",");
  f.println(resistanceOhms, 3);

  f.close();
  return true;
}

bool appendFermentationEventLog(const String &eventName, const String &detail) {
  if (!sdReady || fermentationLogFileName.length() == 0 || !fermentationLogActive) return false;

  File f = SD.open(fermentationLogFileName.c_str(), FILE_WRITE);
  if (!f) return false;

  unsigned long ts = millis() / 1000UL;
  f.print(ts);
  f.print(",EVENT,");
  f.print(eventName);
  f.print(",");
  f.println(detail);

  f.close();
  return true;
}

void updateLogsFileList() {
  if (!initSDCard()) {
    sendText("t0", "NO CARD");
    return;
  }

  File root = SD.open("/");
  if (!root) {
    sendText("t0", "ROOT OPEN FAIL");
    return;
  }

  String listing = "";
  uint8_t entryCount = 0;
  bool truncated = false;
  constexpr uint16_t MAX_LOG_TEXT_CHARS = 900;
  constexpr uint8_t MAX_LOG_ENTRIES = 24;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    String line = String(entry.name());
    if (entry.isDirectory()) {
      line += " <DIR>";
    } else {
      line += " ";
      line += String((uint32_t)entry.size());
      line += " B";
    }
    line += "\r";

    if (entryCount >= MAX_LOG_ENTRIES ||
        listing.length() + line.length() + 16 > MAX_LOG_TEXT_CHARS) {
      truncated = true;
      entry.close();
      break;
    }

    listing += line;
    entryCount++;
    entry.close();
  }

  root.close();

  if (entryCount == 0) {
    listing = "NO FILES";
  } else if (truncated) {
    listing += "...";
  }

  sendText("t0", listing);
}

void updateProcessSetupLoadedObjects() {
  sendNumber("loadmashdur", mashDurationMin);
  sendNumber("loadmashTemp", mashTempRaw);
  sendNumber("loadBoilDur", boilDurationMin);
}

void updateTempCalProbeText() {
  sendText("probe", temperatureProbeName(activeTempCalProbe));
}

void setTempCalStatusText(const String &status) {
  tempCalStatusMessage = status;
  sendText("t3", tempCalStatusMessage);
  Serial.print("TempCal Status -> ");
  Serial.println(tempCalStatusMessage);
}

void updateTempCalStoredValuesOnHMI() {
  sendNumber("R1", (int32_t)round(R1_val * DECIMAL_SCALE));
  sendNumber("T1", (int32_t)round(T1_val * DECIMAL_SCALE));
  sendNumber("R2", (int32_t)round(R2_val * DECIMAL_SCALE));
  sendNumber("T2", (int32_t)round(T2_val * DECIMAL_SCALE));
}

void updateTempCalStatusText() {
  uint16_t rtd = thermo.readRTD();
  float measuredResistance = (rtd / 32768.0f) * RREF;
  float tempC = calculateCalibratedTempCForProbe(measuredResistance, activeTempCalProbe);
  uint8_t fault = thermo.readFault();
  String maxStatus = formatTempCalMax31865Status(rtd, measuredResistance, fault);
  String status = "R:" + String(measuredResistance, 2) + " T:" + String(tempC, 2) + " " + maxStatus;
  sendText("loadcal", "LOAD CAL");
  sendText("tomenu", "MAIN MENU");
  sendText("firstCal", "0C SNAP");
  sendText("secondCal", "100C SNAP");
  updateTempCalProbeText();
  sendText("cR", String(measuredResistance, 2));
  sendText("temp", String(tempC, 2));
  if (maxStatus.length() > 0) {
    setTempCalStatusText(maxStatus);
  } else if (tempCalStatusMessage.length() > 0) {
    sendText("t3", tempCalStatusMessage);
  }
  Serial.println(status);
  if (fault) thermo.clearFault();
}

void updateTempCalLiveDisplay() {
  float measuredResistance = readRTDResistanceOhms();
  sendNumber("cR", (int32_t)round(measuredResistance * DECIMAL_SCALE));
  sendNumber("temp", (int32_t)round(getLiveTempC() * DECIMAL_SCALE));
}

void updateTempCalPageOnEnter() {
  applyActiveTempProbeCalibration();
  updateTempCalStoredValuesOnHMI();
  updateTempCalProbeText();
  updateTempCalStatusText();

  if (tempCalStatusMessage.length() == 0) {
    setTempCalStatusText(String(temperatureProbeName(activeTempCalProbe)) + " READY");
  } else {
    sendText("t3", tempCalStatusMessage);
  }
}

void updateLevelStatusText(const String &status) {
  sendText("status", status);
  Serial.print("LevelCal Status -> ");
  Serial.println(status);
}

void updateLevelCalibrationStatus() {
  if (levelCalLowerPointCaptured) {
    updateLevelStatusText("LOW SAVED V=" + String(levelDis1, 3) +
                          " gal=" + String(levelVol1, 3) +
                          " | Add water, enter vol2");
    return;
  }

  if (!levelCalValid) {
    updateLevelStatusText("NO CAL | Enter vol1, press LOAD LOW");
    return;
  }

  float spanV = levelDis2 - levelDis1;
  String status = "Vspan " + String(levelDis1, 3) + "-" + String(levelDis2, 3) +
                  " dV=" + String(spanV, 3) +
                  " | minV=" + String(min(levelDis1, levelDis2), 3) +
                  " | gal=" + String(levelSlope, 3) + "*V";
  status += (levelIntercept >= 0.0f ? "+" : "");
  status += String(levelIntercept, 3);

  updateLevelStatusText(status);
}

void updateLevelCalStoredValuesOnHMI() {
  sendNumber("vol1", levelVolumeToXFloat(levelVol1));
  sendNumber("vol2", levelVolumeToXFloat(levelVol2));
  sendNumber("forceVoltage1", (int32_t)round(levelDis1 * LEVEL_VOLUME_XFLOAT_SCALE));
  sendNumber("forceVoltage2", (int32_t)round(levelDis2 * LEVEL_VOLUME_XFLOAT_SCALE));
}

bool computeLevelCalibration() {
  float deltaDis = levelDis2 - levelDis1;
  float deltaVol = levelVol2 - levelVol1;

  if (deltaDis == 0.0f || deltaVol == 0.0f) {
    levelCalValid = false;
    return false;
  }

  levelSlope = deltaVol / deltaDis;
  levelIntercept = levelVol1 - (levelSlope * levelDis1);
  levelCalValid = true;
  return true;
}

float calculateLiveVolumeFromDistance(float distanceMM) {
  if (!levelCalValid) return 0.0f;
  if (!isLevelVoltageInReliableRange(distanceMM)) return 0.0f;
  return (levelSlope * distanceMM) + levelIntercept;
}

bool isLevelVoltageInReliableRange(float voltageV) {
  if (!levelCalValid) return false;
  float minReliableVoltage = min(levelDis1, levelDis2);
  return voltageV >= minReliableVoltage;
}

bool isLiveLevelVolumeReliable() {
  return isLevelVoltageInReliableRange(filteredDistanceMM);
}

float readHydrostaticPressureSensorVoltage() {
  int rawCount = analogRead(LEVEL_PRESSURE_SENSOR_PIN);
  return ((float)rawCount * LEVEL_ADC_REFERENCE_V) / (float)LEVEL_ADC_MAX_COUNT;
}

int32_t levelVolumeToXFloat(float volumeGal) {
  return (int32_t)round(volumeGal * LEVEL_VOLUME_XFLOAT_SCALE);
}

void updateLevelLoadCalButtonText() {
  sendText("loadcal", levelCalLowerPointCaptured ? "LOAD HIGH" : "LOAD LOW");
}

float filterDistance(float newReadingMM) {
  if (newReadingMM < MIN_VALID_LEVEL_SENSOR_V || newReadingMM > MAX_VALID_LEVEL_SENSOR_V) {
    return filteredDistanceMM;
  }

  if (!distanceInitialized) {
    filteredDistanceMM = newReadingMM;
    distanceInitialized = true;
    return filteredDistanceMM;
  }

  if (fabs(newReadingMM - filteredDistanceMM) > MAX_VALID_LEVEL_SENSOR_JUMP_V) {
    return filteredDistanceMM;
  }

  filteredDistanceMM = newReadingMM;
  return filteredDistanceMM;
}

void resetLevelLiveState() {
  rawDistanceMM = 0.0f;
  filtDistanceMM = 0.0f;
  liveVolume = 0.0f;
  filteredDistanceMM = 0.0f;
  distanceInitialized = false;

  sendNumber("rawVoltage", 0);
  sendNumber("x0", 0);
  updateLevelCalibrationStatus();
}

void updateLevelLiveDisplay() {
  rawDistanceMM = readHydrostaticPressureSensorVoltage();
  float acceptedDistance = filterDistance(rawDistanceMM);

  filtDistanceMM = acceptedDistance;
  liveVolume = calculateLiveVolumeFromDistance(acceptedDistance);

  if (currentPage == PAGE_LEVEL_CAL) {
    sendNumber("rawVoltage", (int32_t)round(rawDistanceMM * LEVEL_VOLUME_XFLOAT_SCALE));
    sendNumber("x0", levelVolumeToXFloat(liveVolume));
    if (levelCalValid && !isLevelVoltageInReliableRange(acceptedDistance)) {
      updateLevelStatusText("LOW REGION | V=" + String(acceptedDistance, 3) +
                            " < " + String(min(levelDis1, levelDis2), 3));
    } else {
      updateLevelCalibrationStatus();
    }

    Serial.print("Raw level sensor(V): ");
    Serial.print(rawDistanceMM, 3);
    Serial.print(" | Filtered(V): ");
    Serial.print(acceptedDistance, 3);
    Serial.print(" | Vol: ");
    Serial.println(liveVolume, 2);
  }
}

float getLiveVolumeFromTOF() {
  return calculateLiveVolumeFromDistance(filteredDistanceMM);
}
