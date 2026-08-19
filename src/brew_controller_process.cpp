#include "brew_controller.h"

namespace {
constexpr float MANUAL_VOLUME_EPSILON_GAL = 0.0005f;
constexpr float ESTIMATE_BLEND_RECENT_WEIGHT = 0.35f;
constexpr float ESTIMATE_BLEND_CUMULATIVE_WEIGHT = 0.65f;
constexpr float ESTIMATE_MIN_ELAPSED_SEC = 15.0f;
constexpr float ESTIMATE_MIN_RATE = 0.01f;

bool hasManualVolumeValue(float volumeGal) {
  return volumeGal > MANUAL_VOLUME_EPSILON_GAL;
}

float blendEstimate(float recentValue, float cumulativeValue) {
  bool recentValid = recentValue > 0.0f && isfinite(recentValue);
  bool cumulativeValid = cumulativeValue > 0.0f && isfinite(cumulativeValue);

  if (recentValid && cumulativeValid) {
    return recentValue * ESTIMATE_BLEND_RECENT_WEIGHT +
           cumulativeValue * ESTIMATE_BLEND_CUMULATIVE_WEIGHT;
  }
  if (cumulativeValid) return cumulativeValue;
  if (recentValid) return recentValue;
  return 0.0f;
}
}

void updateFillResultText(const String &txt) {
  sendText("fillresult", txt);
}

void updateFillStatusObjects() {
  sendText("hxstatus", heaterOn ? "ON" : "OFF");
  sendText("pumpstatus", pumpOn ? "ON" : "OFF");
  sendText("alarm", "NONE");
}

void updateFillLiveDisplay() {
  liveTempC = getLiveTempC();

  bool liveVolumeReliable = isLiveLevelVolumeReliable();
  float displayVolume = fillVolumeLocked ? lockedFillVolumeGal : getLiveVolumeFromTOF();

  sendNumber("temp", (int32_t)round(liveTempC * DECIMAL_SCALE));
  sendNumber("vol", processVolToNextion(displayVolume));

  if (!fillVolumeLocked && !fillStabilizing && levelCalValid) {
    updateFillResultText(liveVolumeReliable ? "FILLING" : "LOW LEVEL");
  }

  updateFillStatusObjects();
}

void startFillStabilization() {
  fillStabilizing = true;
  fillCaptureSource = VOLUME_CAPTURE_NONE;
  fillStabilizeStartMs = millis();
  fillStabilizeSum = 0.0f;
  fillStabilizeCount = 0;
  updateFillResultText("STABILIZING 10s");
}

void processFillStabilization() {
  if (!fillStabilizing) return;

  bool liveVolumeReliable = isLiveLevelVolumeReliable();
  float currentVolume = getLiveVolumeFromTOF();
  if (liveVolumeReliable) {
    fillStabilizeSum += currentVolume;
    fillStabilizeCount++;
  }

  unsigned long elapsed = millis() - fillStabilizeStartMs;
  unsigned long remainingMs = (elapsed >= FILL_STABILIZE_TIME_MS) ? 0 : (FILL_STABILIZE_TIME_MS - elapsed);
  uint8_t remainingSec = (remainingMs + 999) / 1000;

  updateFillResultText(liveVolumeReliable ? ("STABILIZING " + String(remainingSec) + "s") : "LOW LEVEL");

  if (elapsed >= FILL_STABILIZE_TIME_MS) {
    fillStabilizing = false;

    if (fillStabilizeCount > 0) {
      lockedFillVolumeGal = fillStabilizeSum / (float)fillStabilizeCount;
      fillVolumeLocked = true;
      fillCaptureSource = VOLUME_CAPTURE_TOF;
      sendNumber("vol", processVolToNextion(lockedFillVolumeGal));
      updateFillResultText("LOCKED " + String(lockedFillVolumeGal, 2) + " gal");
      appendEventLog("FILL_LOCKED", String(lockedFillVolumeGal, 2));
    } else {
      fillVolumeLocked = false;
      updateFillResultText("LOCK FAIL");
    }
  }
}

String formatSignedTime(long sec) {
  bool neg = (sec < 0);
  long s = labs(sec);
  long minutes = s / 60;
  long seconds = s % 60;

  char buf[16];
  snprintf(buf, sizeof(buf), "%s%02ld:%02ld", neg ? "-" : "", minutes, seconds);
  return String(buf);
}

float getMashDisplayVolume() {
  if (boilStartVolumeLocked) return lockedBoilStartVolumeGal;
  if (fillVolumeLocked) return lockedFillVolumeGal;
  return getLiveVolumeFromTOF();
}

float getMashLiveCaptureVolume() {
  return getLiveVolumeFromTOF();
}

float getCurrentMashSetpointC() {
  if (mashState == MASH_HEATING_STRIKE || mashState == MASH_WAIT_FOR_BAG) {
    return mashTempC + STRIKE_OFFSET_C;
  }
  return mashTempC;
}

long getMashTimeRemainingSec() {
  if (!mashStarted) return (long)mashDurationMin * 60L;

  long targetSec = (long)mashDurationMin * 60L;
  long elapsedSec = (long)((millis() - mashStartMs) / 1000UL);
  return targetSec - elapsedSec;
}

String mashStateText() {
  switch (mashState) {
    case MASH_IDLE:            return "READY";
    case MASH_HEATING_STRIKE:  return "HEATING TO STRIKE";
    case MASH_WAIT_FOR_BAG:    return "ADD BAG";
    case MASH_HOLD:            return "MASH HOLD";
    case MASH_WAIT_REMOVE_BAG: return "REMOVE BAG";
    case MASH_CAPTURE_VOLUME:  return "STABILIZING VOLUME";
    case MASH_COMPLETE:
      if (mashCaptureSource == VOLUME_CAPTURE_MANUAL) return "READY FOR BOIL MAN";
      if (mashCaptureSource == VOLUME_CAPTURE_TOF) return "READY FOR BOIL TOF";
      return "READY FOR BOIL";
    default:                   return "UNKNOWN";
  }
}

String mashActionButtonText() {
  switch (mashState) {
    case MASH_IDLE:            return "START";
    case MASH_HEATING_STRIKE:  return "HEATING";
    case MASH_WAIT_FOR_BAG:    return "MASH IN";
    case MASH_HOLD:            return (getMashTimeRemainingSec() <= 0) ? "MASH OUT" : "MASHING";
    case MASH_WAIT_REMOVE_BAG: return "CAPTURE VOL";
    case MASH_CAPTURE_VOLUME:  return "LOCKING";
    case MASH_COMPLETE:        return "READY";
    default:                   return "WAIT";
  }
}

void setMashActionButtonText(const String &txt) {
  sendText("actbtn", txt);
}

void setHeater(bool on) {
  heaterOn = on;
  digitalWrite(HEATER_RELAY_PIN, on ? HIGH : LOW);
}

void controlMashTemperature() {
  static bool mashHoldWindowActive = false;
  static unsigned long mashHoldWindowStartMs = 0;

  if (!(mashState == MASH_HEATING_STRIKE ||
        mashState == MASH_WAIT_FOR_BAG ||
        mashState == MASH_HOLD)) {
    mashHoldWindowActive = false;
    setHeater(false);
    return;
  }

  liveTempC = getLiveTempC();
  float setpoint = getCurrentMashSetpointC();

  if (mashState == MASH_HOLD) {
    unsigned long now = millis();
    if (!mashHoldWindowActive) {
      mashHoldWindowActive = true;
      mashHoldWindowStartMs = now;
    } else if (now - mashHoldWindowStartMs >= MASH_HOLD_CONTROL_WINDOW_MS) {
      mashHoldWindowStartMs = now;
    }

    float errorC = setpoint - liveTempC;
    float dutyPct = MASH_HOLD_BASE_DUTY_PCT + (errorC * MASH_HOLD_GAIN_PCT_PER_C);

    if (errorC >= MASH_HOLD_FORCE_ON_BELOW_C) {
      dutyPct = 100.0f;
    } else if (errorC <= -MASH_HOLD_FORCE_OFF_ABOVE_C) {
      dutyPct = 0.0f;
    } else {
      if (dutyPct < 0.0f) dutyPct = 0.0f;
      if (dutyPct > MASH_HOLD_MAX_DUTY_PCT) dutyPct = MASH_HOLD_MAX_DUTY_PCT;
    }

    unsigned long onDurationMs =
      (unsigned long)round((dutyPct / 100.0f) * (float)MASH_HOLD_CONTROL_WINDOW_MS);
    bool heaterShouldBeOn = false;

    if (dutyPct >= 100.0f) {
      heaterShouldBeOn = true;
    } else if (onDurationMs >= MASH_HOLD_MIN_SWITCH_MS) {
      heaterShouldBeOn = (now - mashHoldWindowStartMs) < onDurationMs;
    }

    setHeater(heaterShouldBeOn);
    return;
  }

  mashHoldWindowActive = false;

  if (liveTempC < (setpoint - TEMP_HYSTERESIS_C)) {
    setHeater(true);
  } else if (liveTempC > (setpoint + TEMP_HYSTERESIS_C)) {
    setHeater(false);
  }
}

void updateMashStatusObjects() {
  sendText("hxstatus", heaterOn ? "ON" : "OFF");
  sendText("pumpstatus", pumpOn ? "ON" : "OFF");
  sendText("alarm", "NONE");
}

float getMashWaveHalfSpanC() {
  return (mashState == MASH_HOLD) ? MASH_WAVE_HOLD_HALF_SPAN_C : MASH_WAVE_HEAT_HALF_SPAN_C;
}

void updateMashYAxisLabels(float setpointC) {
  float halfSpan = getMashWaveHalfSpanC();
  float bottom = setpointC - halfSpan;
  float top = setpointC + halfSpan;
  float span = top - bottom;
  float tickSpan = (float)(TEMP_WAVE_TOP_TICK - TEMP_WAVE_BOTTOM_TICK);
  float lowerMid = bottom + (span * (float)TEMP_WAVE_LOWER_MID_TICK / tickSpan);
  float midVal = bottom + (span * (float)TEMP_WAVE_MID_TICK / tickSpan);
  float upperMid = bottom + (span * (float)TEMP_WAVE_UPPER_MID_TICK / tickSpan);

  sendText("bottom", String(bottom, 1));
  sendText("lowermid", String(lowerMid, 1));
  sendText("mid", String(midVal, 1));
  sendText("uppermid", String(upperMid, 1));
  sendText("top", String(top, 1));
}

uint8_t scaleMashTempToWave(float tempC, float setpointC) {
  float halfSpan = getMashWaveHalfSpanC();
  float yMin = setpointC - halfSpan;
  float yMax = setpointC + halfSpan;

  if (tempC < yMin) tempC = yMin;
  if (tempC > yMax) tempC = yMax;

  float normalized = (tempC - yMin) / (yMax - yMin);
  int value = (int)round(normalized * MASH_WAVE_PLOT_MAX);

  if (value < 0) value = 0;
  if (value > MASH_WAVE_PLOT_MAX) value = MASH_WAVE_PLOT_MAX;

  return (uint8_t)value;
}

void resetMashWaveform() {
  clearMashWaveformChannels();
  updateMashYAxisLabels(getCurrentMashSetpointC());
  Serial.println("Mash waveform reset");
}

void updateMashWaveformFixedRange() {
  float liveT = getLiveTempC();
  float setT = getCurrentMashSetpointC();

  updateMashYAxisLabels(setT);

  uint8_t liveScaled = scaleMashTempToWave(liveT, setT);
  uint8_t spScaled = scaleMashTempToWave(setT, setT);

  waveMashTemp.addValue(MASH_WAVE_CHANNEL_LIVE, liveScaled);
  waveMashTemp.addValue(MASH_WAVE_CHANNEL_SP, spScaled);

  Serial.print("Mash waveform -> Live scaled: ");
  Serial.print(liveScaled);
  Serial.print(" | SP scaled: ");
  Serial.println(spScaled);
}

void updateMashDisplay() {
  liveTempC = getLiveTempC();
  float setpoint = getCurrentMashSetpointC();
  float displayVol = getMashDisplayVolume();
  float liveVolCapture = getMashLiveCaptureVolume();

  sendNumber("temp", (int32_t)round(liveTempC * DECIMAL_SCALE));
  sendNumber("tempSP", (int32_t)round(setpoint * DECIMAL_SCALE));
  sendNumber("vol", processVolToNextion(displayVol));
  sendNumber("volLive", processVolToNextion(liveVolCapture));

  updateMashStatusObjects();
  sendText("mashstate", mashStateText());
  setMashActionButtonText(mashActionButtonText());

  long remain = getMashTimeRemainingSec();
  sendText("timeRemain", formatSignedTime(remain));

  if (!hasLoggedSG) {
    sendText("sgstatus", "NO SG YET");
  }
}

void startMashProcess() {
  mashState = MASH_HEATING_STRIKE;
  mashStarted = false;
  boilStartVolumeLocked = false;
  boilEndVolumeLocked = false;
  mashCaptureStabilizing = false;
  mashCaptureSource = VOLUME_CAPTURE_NONE;
  boilCaptureSource = VOLUME_CAPTURE_NONE;
  processAmbientTempC = getLiveTempC();
  setHeater(true);
  resetMashWaveform();
  appendEventLog("MASH_START", "heating_to_strike");
}

void markMashIn() {
  mashState = MASH_HOLD;
  mashStartMs = millis();
  mashStarted = true;
  resetMashWaveform();
  appendEventLog("MASH_IN", "timer_started");
}

void markMashOut() {
  mashState = MASH_WAIT_REMOVE_BAG;
  setHeater(false);
  resetMashWaveform();
  appendEventLog("MASH_OUT", "remove_bag");
}

void startMashVolumeCapture() {
  float manualVolumeGal = 0.0f;
  if (readVolumeInputGallons("fillvol", manualVolumeGal, 3) && hasManualVolumeValue(manualVolumeGal)) {
    mashCaptureStabilizing = false;
    lockedBoilStartVolumeGal = manualVolumeGal;
    boilStartVolumeLocked = true;
    mashCaptureSource = VOLUME_CAPTURE_MANUAL;
    mashState = MASH_COMPLETE;

    resetMashWaveform();
    sendNumber("vol", processVolToNextion(lockedBoilStartVolumeGal));
    sendNumber("volLive", processVolToNextion(lockedBoilStartVolumeGal));
    sendText("mashstate", mashStateText());
    appendEventLog("BOIL_VOL_LOCKED_MANUAL", String(lockedBoilStartVolumeGal, 2));
    return;
  }

  if (!levelCalValid) {
    sendText("mashstate", "NO LEVEL CAL");
    return;
  }

  mashState = MASH_CAPTURE_VOLUME;
  mashCaptureStabilizing = true;
  mashCaptureStartMs = millis();
  mashCaptureSum = 0.0f;
  mashCaptureCount = 0;
  resetMashWaveform();
  sendText("mashstate", "STABILIZING 10s");
  appendEventLog("BOIL_VOL_CAPTURE_START", "");
}

void processMashVolumeCapture() {
  if (!mashCaptureStabilizing) return;

  bool liveVolumeReliable = isLiveLevelVolumeReliable();
  float currentVolume = getMashLiveCaptureVolume();
  if (liveVolumeReliable) {
    mashCaptureSum += currentVolume;
    mashCaptureCount++;
  }

  unsigned long elapsed = millis() - mashCaptureStartMs;
  unsigned long remainingMs = (elapsed >= MASH_CAPTURE_STABILIZE_TIME_MS) ? 0 : (MASH_CAPTURE_STABILIZE_TIME_MS - elapsed);
  uint8_t remainingSec = (remainingMs + 999) / 1000;

  sendText("mashstate", liveVolumeReliable ? ("STABILIZING " + String(remainingSec) + "s") : "LOW LEVEL");
  sendNumber("volLive", processVolToNextion(currentVolume));

  if (elapsed >= MASH_CAPTURE_STABILIZE_TIME_MS) {
    mashCaptureStabilizing = false;

    if (mashCaptureCount > 0) {
      lockedBoilStartVolumeGal = mashCaptureSum / (float)mashCaptureCount;
      boilStartVolumeLocked = true;
      mashCaptureSource = VOLUME_CAPTURE_TOF;
      mashState = MASH_COMPLETE;

      sendNumber("vol", processVolToNextion(lockedBoilStartVolumeGal));
      sendText("mashstate", mashStateText());
      appendEventLog("BOIL_VOL_LOCKED", String(lockedBoilStartVolumeGal, 2));
    } else {
      sendText("mashstate", "CAPTURE FAIL");
      mashState = MASH_WAIT_REMOVE_BAG;
    }
  }
}

void updateMashStateMachine() {
  if (mashState == MASH_HEATING_STRIKE) {
    float strikeSetpoint = mashTempC + STRIKE_OFFSET_C;
    if (liveTempC >= strikeSetpoint) {
      mashState = MASH_WAIT_FOR_BAG;
      resetMashWaveform();
      appendEventLog("STRIKE_REACHED", String(strikeSetpoint, 2));
    }
  }
}

void logMashSGEntry(float sgValue) {
  long remain = getMashTimeRemainingSec();
  hasLoggedSG = true;
  lastLoggedSG = sgValue;

  String status = "SG " + String(sgValue, 3) + " @ " + formatSignedTime(remain);
  sendText("sgstatus", status);

  appendRunLogLine("MASH", "SG_ENTRY", liveTempC, getCurrentMashSetpointC(),
                   getMashDisplayVolume(), getMashLiveCaptureVolume(),
                   filtDistanceMM, heaterOn, pumpOn, remain, sgValue,
                   getLiveResistanceOhms(), 0.0f);

  appendEventLog("SG_ENTRY", String(sgValue, 3) + " @ " + formatSignedTime(remain));
}

void updateMashPageOnEnter() {
  if (!(mashState == MASH_HEATING_STRIKE ||
        mashState == MASH_WAIT_FOR_BAG ||
        mashState == MASH_HOLD)) {
    setHeater(false);
  }

  resetMashWaveform();
  updateMashDisplay();
}

bool isBoilAllowedToStart() {
  return mashState == MASH_COMPLETE && boilStartVolumeLocked;
}

void stopActiveProcessControls() {
  setHeater(false);
  pumpOn = false;

  if (mashState == MASH_HEATING_STRIKE ||
      mashState == MASH_WAIT_FOR_BAG ||
      mashState == MASH_HOLD) {
    mashState = MASH_IDLE;
    mashStarted = false;
    appendEventLog("MASH_ABORTED", "controls_stopped");
  }

  if (mashState == MASH_CAPTURE_VOLUME) {
    mashCaptureStabilizing = false;
    mashState = MASH_WAIT_REMOVE_BAG;
    appendEventLog("MASH_CAPTURE_ABORTED", "controls_stopped");
  }

  if (boilState == BOIL_HEATING ||
      boilState == BOIL_READY_CONFIRM ||
      boilState == BOIL_ACTIVE ||
      boilState == BOIL_COOLING_ACTIVE) {
    boilState = BOIL_STOPPED;
    boilStarted = false;
    boilActiveStartMs = 0;
    appendEventLog("BOIL_ABORTED", "controls_stopped");
  }
}

String formatElapsedTime(unsigned long elapsedMs) {
  unsigned long totalSec = elapsedMs / 1000UL;
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}

long getBoilTimeRemainingSec() {
  long targetSec = (long)boilDurationMin * 60L;
  if (boilActiveStartMs == 0) return targetSec;

  long elapsedSec = (long)((millis() - boilActiveStartMs) / 1000UL);
  return targetSec - elapsedSec;
}

String boilStateText() {
  switch (boilState) {
    case BOIL_IDLE: return "READY";
    case BOIL_HEATING: return "Heating to Boil";
    case BOIL_READY_CONFIRM: return "Confirm Boil";
    case BOIL_ACTIVE: return "Boiling";
    case BOIL_STOPPED: return "Boil Stopped";
    case BOIL_COOLING_ACTIVE: return "Cooling";
    case BOIL_COOLING_READY_TO_LOAD: return "Cooling Stopped";
    case BOIL_COMPLETE:
      if (boilCaptureSource == VOLUME_CAPTURE_MANUAL) return "Stopped - Manual Vol";
      if (boilCaptureSource == VOLUME_CAPTURE_TOF) return "Stopped - TOF Vol";
      return "Stopped";
    default: return "Unknown";
  }
}

String boilButtonText() {
  switch (boilState) {
    case BOIL_IDLE: return "Start";
    case BOIL_HEATING: return "Heating";
    case BOIL_READY_CONFIRM: return "Confirm Boil";
    case BOIL_ACTIVE: return "Stop";
    case BOIL_STOPPED: return "Start Cooling";
    case BOIL_COOLING_ACTIVE: return "End Cooling";
    case BOIL_COOLING_READY_TO_LOAD: return "Load Volume";
    case BOIL_COMPLETE: return "Loaded";
    default: return "Start";
  }
}

void updateBoilStatusObjects() {
  sendText("hxstatus", heaterOn ? "ON" : "OFF");
  sendText("pumpstatus", pumpOn ? "ON" : "OFF");
  sendText("alarm", "NONE");
}

uint8_t scaleBoilTempToWave(float tempC, float yMin, float yMax) {
  if (tempC < yMin) tempC = yMin;
  if (tempC > yMax) tempC = yMax;

  float normalized = (tempC - yMin) / (yMax - yMin);
  int value = (int)round(normalized * BOIL_WAVE_PLOT_MAX);

  if (value < 0) value = 0;
  if (value > BOIL_WAVE_PLOT_MAX) value = BOIL_WAVE_PLOT_MAX;

  return (uint8_t)value;
}

void updateBoilYAxisLabels(float yMin, float yMax) {
  float span = yMax - yMin;
  float tickSpan = (float)(TEMP_WAVE_TOP_TICK - TEMP_WAVE_BOTTOM_TICK);
  float lowerMid = yMin + (span * 6.0f / tickSpan);
  float midVal = yMin + (span * (float)TEMP_WAVE_MID_TICK / tickSpan);
  float upperMid = yMin + (span * (float)TEMP_WAVE_UPPER_MID_TICK / tickSpan);

  sendText("bottom", String(yMin, 1));
  sendText("lowermid", String(lowerMid, 1));
  sendText("mid", String(midVal, 1));
  sendText("uppermid", String(upperMid, 1));
  sendText("top", String(yMax, 1));
}

void resetBoilWaveform() {
  clearBoilWaveformChannels();

  if (boilState == BOIL_ACTIVE) {
    updateBoilYAxisLabels(BOIL_START_TEMP_C - 2.0f, BOIL_START_TEMP_C + 2.0f);
  } else if (boilState == BOIL_COOLING_ACTIVE || boilState == BOIL_COOLING_READY_TO_LOAD || boilState == BOIL_COMPLETE) {
    updateBoilYAxisLabels(COOLING_MODEL_AMBIENT_TEMP_C, COOLING_START_TEMP_C);
  } else {
    float yMin = boilWaveStartTempC;
    float yMax = BOIL_START_TEMP_C;

    if (fabs(yMax - yMin) < 0.5f) {
      yMin = yMax - 1.0f;
    }

    updateBoilYAxisLabels(yMin, yMax);
  }

  Serial.println("Boil waveform reset");
}

void updateBoilWaveform() {
  float liveT = getLiveTempC();

  if (boilState == BOIL_ACTIVE) {
    float yMin = BOIL_START_TEMP_C - 2.0f;
    float yMax = BOIL_START_TEMP_C + 2.0f;

    updateBoilYAxisLabels(yMin, yMax);

    uint8_t liveScaled = scaleBoilTempToWave(liveT, yMin, yMax);
    uint8_t refScaled = scaleBoilTempToWave(BOIL_START_TEMP_C, yMin, yMax);

    waveBoilTemp.addValue(BOIL_WAVE_CHANNEL_LIVE, liveScaled);
    waveBoilTemp.addValue(BOIL_WAVE_CHANNEL_REF, refScaled);

    Serial.print("Boil waveform ACTIVE -> Live: ");
    Serial.print(liveScaled);
    Serial.print(" Ref: ");
    Serial.println(refScaled);
  } else if (boilState == BOIL_COOLING_ACTIVE || boilState == BOIL_COOLING_READY_TO_LOAD || boilState == BOIL_COMPLETE) {
    float yMin = COOLING_MODEL_AMBIENT_TEMP_C;
    float yMax = COOLING_START_TEMP_C;

    updateBoilYAxisLabels(yMin, yMax);

    uint8_t liveScaled = scaleBoilTempToWave(liveT, yMin, yMax);
    uint8_t refScaled = scaleBoilTempToWave(COOLING_TARGET_TEMP_C, yMin, yMax);

    waveBoilTemp.addValue(BOIL_WAVE_CHANNEL_LIVE, liveScaled);
    waveBoilTemp.addValue(BOIL_WAVE_CHANNEL_REF, refScaled);

    Serial.print("Boil waveform COOLING -> Live: ");
    Serial.print(liveScaled);
    Serial.print(" Ref: ");
    Serial.println(refScaled);
  } else {
    float yMin = boilWaveStartTempC;
    float yMax = BOIL_START_TEMP_C;

    if (fabs(yMax - yMin) < 0.5f) {
      yMin = yMax - 1.0f;
    }

    updateBoilYAxisLabels(yMin, yMax);

    uint8_t liveScaled = scaleBoilTempToWave(liveT, yMin, yMax);
    waveBoilTemp.addValue(BOIL_WAVE_CHANNEL_LIVE, liveScaled);

    Serial.print("Boil waveform HEATING -> Live: ");
    Serial.println(liveScaled);
  }
}

String formatBoilControlPct() {
  return String(boilControlPct, 1) + "%";
}

void updateBoilControlText() {
  sendText("contrlprct", formatBoilControlPct());
}

void updateBoilEstimateText() {
  float currentTemp = getLiveTempC();
  unsigned long now = millis();

  if (boilState == BOIL_HEATING) {
    if (lastBoilEstimateMs == 0) {
      lastBoilEstimateMs = now;
      lastBoilEstimateTempC = currentTemp;
      sendText("updates", "Estimating...");
      return;
    }

    float dtMin = (now - lastBoilEstimateMs) / 60000.0f;
    float dTemp = currentTemp - lastBoilEstimateTempC;

    if (dtMin < 0.02f) {
      sendText("updates", "Estimating...");
      return;
    }

    float instantRate = dTemp / dtMin;

    if (instantRate > 0.01f) {
      if (boilRateFilteredCPerMin <= 0.0f) {
        boilRateFilteredCPerMin = instantRate;
      } else {
        boilRateFilteredCPerMin =
          boilRateFilteredCPerMin + BOIL_RATE_ALPHA * (instantRate - boilRateFilteredCPerMin);
      }
    }

    float cumulativeRate = 0.0f;
    float elapsedSec = (now - boilStartMs) / 1000.0f;
    if (elapsedSec >= ESTIMATE_MIN_ELAPSED_SEC && currentTemp > boilEstimateStartTempC) {
      cumulativeRate = (currentTemp - boilEstimateStartTempC) / (elapsedSec / 60.0f);
    }

    float effectiveRate = blendEstimate(boilRateFilteredCPerMin, cumulativeRate);
    float remainingC = BOIL_HEAT_CYCLE_START_TEMP_C - currentTemp;

    if (remainingC <= 0.0f) {
      sendText("updates", "At boil threshold");
    } else if (effectiveRate <= ESTIMATE_MIN_RATE) {
      sendText("updates", "Estimating...");
    } else {
      float remainingMin = remainingC / effectiveRate;

      if (remainingMin < 0.0f || remainingMin > 300.0f) {
        sendText("updates", "Estimating...");
      } else {
        unsigned long remainingSec = (unsigned long)round(remainingMin * 60.0f);
        String eta = formatElapsedTime(remainingSec * 1000UL);
        sendText("updates", "ETA to boil: " + eta);
      }
    }

    lastBoilEstimateMs = now;
    lastBoilEstimateTempC = currentTemp;
    return;
  }

  if (boilState == BOIL_READY_CONFIRM) {
    sendText("updates", "Confirm visible boil");
    return;
  }

  if (boilState == BOIL_COOLING_ACTIVE) {
    if (currentTemp <= COOLING_TARGET_TEMP_C) {
      sendText("updates", "At Pitch Temp");
      return;
    }

    if (lastBoilEstimateMs == 0) {
      lastBoilEstimateMs = now;
      lastBoilEstimateTempC = currentTemp;
      sendText("updates", "Estimating...");
      return;
    }

    float dtSec = (now - lastBoilEstimateMs) / 1000.0f;
    float ambientC = processAmbientTempC;
    float prevDelta = lastBoilEstimateTempC - ambientC;
    float currDelta = currentTemp - ambientC;

    if (dtSec < 2.0f || prevDelta <= COOLING_MIN_MODEL_DELTA_C || currDelta <= COOLING_MIN_MODEL_DELTA_C) {
      sendText("updates", "Estimating...");
      return;
    }

    float ratio = currDelta / prevDelta;
    if (ratio > 0.0f && ratio < 1.0f) {
      float instantK = -logf(ratio) / dtSec;
      if (instantK > 0.0f && isfinite(instantK)) {
        if (boilRateFilteredCPerMin <= 0.0f) {
          boilRateFilteredCPerMin = instantK;
        } else {
          boilRateFilteredCPerMin =
            boilRateFilteredCPerMin + BOIL_RATE_ALPHA * (instantK - boilRateFilteredCPerMin);
        }
      }
    }

    float cumulativeK = 0.0f;
    float totalCoolingSec = (now - boilStartMs) / 1000.0f;
    float startDelta = coolingEstimateStartTempC - ambientC;
    if (totalCoolingSec >= ESTIMATE_MIN_ELAPSED_SEC &&
        startDelta > COOLING_MIN_MODEL_DELTA_C &&
        currDelta > COOLING_MIN_MODEL_DELTA_C &&
        currDelta < startDelta) {
      cumulativeK = logf(startDelta / currDelta) / totalCoolingSec;
    }

    float effectiveK = blendEstimate(boilRateFilteredCPerMin, cumulativeK);
    float targetDelta = COOLING_TARGET_TEMP_C - ambientC;
    if (effectiveK <= 0.0f || targetDelta <= COOLING_MIN_MODEL_DELTA_C || currDelta <= targetDelta) {
      sendText("updates", currentTemp <= COOLING_TARGET_TEMP_C ? "At Pitch Temp" : "Estimating...");
    } else {
      float remainingSec = logf(currDelta / targetDelta) / effectiveK;
      if (!isfinite(remainingSec) || remainingSec < 0.0f || remainingSec > 6.0f * 3600.0f) {
        sendText("updates", "Estimating...");
      } else {
        sendText("updates", "ETA to 25C: " + formatElapsedTime((unsigned long)round(remainingSec * 1000.0f)));
      }
    }

    lastBoilEstimateMs = now;
    lastBoilEstimateTempC = currentTemp;
    return;
  }

  if (boilState == BOIL_COOLING_READY_TO_LOAD) {
    sendText("updates", "Cooling Ended - Load Vol");
    return;
  }

  if (boilState == BOIL_COMPLETE) {
    if (boilCaptureSource == VOLUME_CAPTURE_MANUAL) {
      sendText("updates", "Manual Vol Loaded");
    } else if (boilCaptureSource == VOLUME_CAPTURE_TOF) {
      sendText("updates", "TOF Vol Loaded");
    } else {
      sendText("updates", "");
    }
    return;
  }

  sendText("updates", "");
}

float getBoilDisplayVolume() {
  if (boilEndVolumeLocked) return lockedBoilEndVolumeGal;
  if (boilStartVolumeLocked) return lockedBoilStartVolumeGal;
  return getMashDisplayVolume();
}

void updateBoilDisplay() {
  float liveT = getLiveTempC();
  float displayVol = getBoilDisplayVolume();

  sendNumber("temp", (int32_t)round(liveT * DECIMAL_SCALE));
  sendNumber("vol", processVolToNextion(displayVol));

  updateBoilStatusObjects();
  sendText("phase", "Boil");
  sendText("status", boilStateText());
  sendText("boilbtn", boilButtonText());
  updateBoilControlText();

  sendText("time", formatSignedTime(getBoilTimeRemainingSec()));

  updateBoilEstimateText();
}

bool captureBoilEndVolume(float volumeGal, VolumeCaptureSource source) {
  if (volumeGal < 0.0f) return false;

  lockedBoilEndVolumeGal = volumeGal;
  boilEndVolumeLocked = true;
  boilCaptureSource = source;
  boilState = BOIL_COMPLETE;

  sendNumber("vol", processVolToNextion(lockedBoilEndVolumeGal));

  if (source == VOLUME_CAPTURE_MANUAL) {
    appendEventLog("BOIL_END_VOL_MANUAL", String(lockedBoilEndVolumeGal, 2));
  } else {
    appendEventLog("BOIL_END_VOL_TOF", String(lockedBoilEndVolumeGal, 2));
  }

  return true;
}

void startBoilProcess() {
  boilState = BOIL_HEATING;
  boilStarted = true;
  boilStartMs = millis();
  boilActiveStartMs = 0;
  boilControlPct = 100.0f;
  boilEndVolumeLocked = false;
  boilCaptureSource = VOLUME_CAPTURE_NONE;

  lastBoilEstimateMs = millis();
  lastBoilEstimateTempC = getLiveTempC();
  boilRateFilteredCPerMin = 0.0f;
  boilEstimateStartTempC = lastBoilEstimateTempC;
  coolingEstimateStartTempC = lastBoilEstimateTempC;
  if (!isfinite(processAmbientTempC) || processAmbientTempC >= boilEstimateStartTempC) {
    processAmbientTempC = boilEstimateStartTempC - 1.0f;
  }

  boilWaveStartTempC = getLiveTempC();
  if (boilWaveStartTempC > BOIL_START_TEMP_C) {
    boilWaveStartTempC = BOIL_START_TEMP_C - 1.0f;
  }

  setHeater(true);
  resetBoilWaveform();
  appendEventLog("BOIL_START", "heating_to_boil");
}

void enterBoilingState() {
  boilState = BOIL_ACTIVE;
  boilActiveStartMs = millis();
  boilControlPct = 100.0f;

  resetBoilWaveform();
  appendEventLog("BOIL_REACHED", ">=" + String(BOIL_HEAT_CYCLE_START_TEMP_C, 1) + "C");
}

void readyToConfirmBoil() {
  boilState = BOIL_READY_CONFIRM;
  appendEventLog("BOIL_READY_CONFIRM", ">=" + String(BOIL_HEAT_CYCLE_START_TEMP_C, 1) + "C");
}

void stopBoilProcess() {
  boilState = BOIL_STOPPED;
  setHeater(false);
  appendEventLog("BOIL_STOPPED", "");
}

void startBoilCoolingProcess() {
  boilState = BOIL_COOLING_ACTIVE;
  boilStartMs = millis();
  lastBoilEstimateMs = millis();
  lastBoilEstimateTempC = getLiveTempC();
  boilRateFilteredCPerMin = 0.0f;
  coolingEstimateStartTempC = lastBoilEstimateTempC;
  setHeater(false);
  resetBoilWaveform();
  appendEventLog("COOLING_START", "");
}

void stopBoilCoolingProcess() {
  boilState = BOIL_COOLING_READY_TO_LOAD;
  setHeater(false);
  appendEventLog("COOLING_END", "");
}

void controlBoilHeater() {
  if (boilState == BOIL_HEATING) {
    setHeater(true);
    return;
  }

  if (boilState == BOIL_READY_CONFIRM) {
    setHeater(true);
    return;
  }

  if (boilState == BOIL_ACTIVE) {
    unsigned long cyclePos = (millis() - boilActiveStartMs) % BOIL_DUTY_CYCLE_MS;
    unsigned long onTime = (unsigned long)round((boilControlPct / 100.0f) * BOIL_DUTY_CYCLE_MS);

    setHeater(cyclePos < onTime);
    return;
  }

  setHeater(false);
}

void updateBoilStateMachine() {
  float liveT = getLiveTempC();

  if (boilState == BOIL_HEATING && liveT >= BOIL_HEAT_CYCLE_START_TEMP_C) {
    readyToConfirmBoil();
  }
}

void incrementBoilControlPct() {
  boilControlPct += BOIL_CTRL_STEP_PCT;
  if (boilControlPct > 100.0f) boilControlPct = 100.0f;
}

void decrementBoilControlPct() {
  boilControlPct -= BOIL_CTRL_STEP_PCT;
  if (boilControlPct < 0.0f) boilControlPct = 0.0f;
}

void updateBoilPageOnEnter() {
  if (!(boilState == BOIL_HEATING || boilState == BOIL_READY_CONFIRM || boilState == BOIL_ACTIVE)) {
    setHeater(false);
  }

  resetBoilWaveform();
  updateBoilDisplay();
}

namespace {
constexpr unsigned long FERMENTATION_SIX_HOUR_MS = 6UL * 60UL * 60UL * 1000UL;

String formatFermentationElapsedTime(unsigned long elapsedMs) {
  unsigned long totalSec = elapsedMs / 1000UL;
  unsigned long days = totalSec / 86400UL;
  totalSec %= 86400UL;
  unsigned long hours = totalSec / 3600UL;
  totalSec %= 3600UL;
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[24];
  if (days > 0) {
    snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
  } else {
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  }
  return String(buf);
}
}

String fermentationStateText() {
  switch (fermentationState) {
    case FERMENTATION_NEEDS_FILE: return "NEEDS FILE";
    case FERMENTATION_FILE_READY: return "FILE READY";
    case FERMENTATION_LOGGING: return "LOGGING TEMP";
    case FERMENTATION_PITCHED: return "FERMENTING";
    case FERMENTATION_STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

String fermentationButtonText() {
  switch (fermentationState) {
    case FERMENTATION_NEEDS_FILE: return "CREATE FILE";
    case FERMENTATION_FILE_READY: return "START";
    case FERMENTATION_LOGGING: return "PITCH YEAST";
    case FERMENTATION_PITCHED: return "STOP";
    case FERMENTATION_STOPPED: return "CREATE FILE";
    default: return "CREATE FILE";
  }
}

unsigned long getFermentationElapsedMs() {
  if (!fermentationStarted) return 0;
  if (fermentationState == FERMENTATION_LOGGING || fermentationState == FERMENTATION_PITCHED) {
    return millis() - fermentationStartMs;
  }
  return fermentationStopMs - fermentationStartMs;
}

void resetFermentationStats(float initialTempC) {
  fermentationMinTempC = initialTempC;
  fermentationMaxTempC = initialTempC;
  fermentationTempSumC = 0.0f;
  fermentationTempCount = 0;
  fermentationSixHourStartMs = millis();
  fermentationSixHourTempSumC = 0.0f;
  fermentationSixHourTempCount = 0;
  lastFermentationStatsUpdateMs = 0;
}

void createFermentationFileFromHMI() {
  String requestedName = "";
  if (!readTextValue(txtFermentationFilename, requestedName) || requestedName.length() == 0) {
    requestedName = "FERMENTATION";
  }

  bool renamed = false;
  if (!createFermentationLogFile(requestedName, renamed)) {
    fermentationState = FERMENTATION_NEEDS_FILE;
    fermentationStatusMessage = sdReady ? "WRITE FAIL" : "NO CARD";
    return;
  }

  fermentationState = FERMENTATION_FILE_READY;
  fermentationStarted = false;
  fermentationYeastPitched = false;
  fermentationStartMs = 0;
  fermentationPitchMs = 0;
  fermentationStopMs = 0;
  hasFermentationSG = false;
  lastFermentationSG = 0.0f;
  fermentationSGStatusMessage = "NO SG YET";
  liveTempC = getLiveTempC();
  resetFermentationStats(liveTempC);
  resetFermentationWaveform(liveTempC);

  fermentationStatusMessage = renamed ? "EXISTS->" + fermentationLogFileName : "CREATED " + fermentationLogFileName;
  appendFermentationEventLog("FILE_CREATED", fermentationLogFileName);
}

void startFermentationLogging() {
  if (!fermentationLogActive || fermentationLogFileName.length() == 0) {
    fermentationState = FERMENTATION_NEEDS_FILE;
    fermentationStatusMessage = "CREATE FILE FIRST";
    return;
  }

  liveTempC = getLiveTempC();
  fermentationState = FERMENTATION_LOGGING;
  fermentationStarted = true;
  fermentationYeastPitched = false;
  fermentationStartMs = millis();
  fermentationPitchMs = 0;
  fermentationStopMs = 0;
  resetFermentationStats(liveTempC);
  resetFermentationWaveform(liveTempC);
  setHeater(false);
  fermentationStatusMessage = "TEMP LOGGING";
  appendFermentationEventLog("TEMP_LOGGING_START", String(liveTempC, 2));
}

void pitchFermentationYeast() {
  if (fermentationState != FERMENTATION_LOGGING) return;

  liveTempC = getLiveTempC();
  fermentationState = FERMENTATION_PITCHED;
  fermentationYeastPitched = true;
  fermentationPitchMs = millis();
  fermentationStartMs = fermentationPitchMs;
  resetFermentationStats(liveTempC);
  resetFermentationWaveform(liveTempC);
  fermentationStatusMessage = "YEAST PITCHED";
  appendFermentationEventLog("YEAST_PITCHED", String(liveTempC, 2));
}

void stopFermentationProcess() {
  if (fermentationState != FERMENTATION_LOGGING && fermentationState != FERMENTATION_PITCHED) return;

  fermentationStopMs = millis();
  fermentationState = FERMENTATION_STOPPED;
  setHeater(false);
  appendFermentationEventLog("FERMENTATION_STOP", formatFermentationElapsedTime(getFermentationElapsedMs()));
  fermentationStatusMessage = "STOPPED";
}

void updateFermentationStats(float tempC) {
  if (fermentationState != FERMENTATION_PITCHED) return;

  unsigned long now = millis();
  if (lastFermentationStatsUpdateMs != 0 && now - lastFermentationStatsUpdateMs < 1000UL) return;
  lastFermentationStatsUpdateMs = now;

  if (fermentationTempCount == 0) {
    fermentationMinTempC = tempC;
    fermentationMaxTempC = tempC;
  } else {
    if (tempC < fermentationMinTempC) fermentationMinTempC = tempC;
    if (tempC > fermentationMaxTempC) fermentationMaxTempC = tempC;
  }

  fermentationTempSumC += tempC;
  fermentationTempCount++;

  if (millis() - fermentationSixHourStartMs >= FERMENTATION_SIX_HOUR_MS) {
    fermentationSixHourStartMs = millis();
    fermentationSixHourTempSumC = 0.0f;
    fermentationSixHourTempCount = 0;
  }

  fermentationSixHourTempSumC += tempC;
  fermentationSixHourTempCount++;
}

void updateFermentationYAxisLabels() {
  float span = fermentationWaveMaxC - fermentationWaveMinC;
  if (span < FERMENTATION_WAVE_MIN_SPAN_C) {
    float center = (fermentationWaveMinC + fermentationWaveMaxC) * 0.5f;
    fermentationWaveMinC = center - (FERMENTATION_WAVE_MIN_SPAN_C * 0.5f);
    fermentationWaveMaxC = center + (FERMENTATION_WAVE_MIN_SPAN_C * 0.5f);
    span = fermentationWaveMaxC - fermentationWaveMinC;
  }

  float tickSpan = (float)(TEMP_WAVE_TOP_TICK - TEMP_WAVE_BOTTOM_TICK);
  float lowerMid = fermentationWaveMinC + (span * (float)TEMP_WAVE_LOWER_MID_TICK / tickSpan);
  float midVal = fermentationWaveMinC + (span * (float)TEMP_WAVE_MID_TICK / tickSpan);
  float upperMid = fermentationWaveMinC + (span * (float)TEMP_WAVE_UPPER_MID_TICK / tickSpan);

  sendText("bottom", String(fermentationWaveMinC, 1));
  sendText("lowermid", String(lowerMid, 1));
  sendText("mid", String(midVal, 1));
  sendText("uppermid", String(upperMid, 1));
  sendText("top", String(fermentationWaveMaxC, 1));
}

uint8_t scaleFermentationTempToWave(float tempC) {
  if (tempC < fermentationWaveMinC) tempC = fermentationWaveMinC;
  if (tempC > fermentationWaveMaxC) tempC = fermentationWaveMaxC;

  float normalized = (tempC - fermentationWaveMinC) / (fermentationWaveMaxC - fermentationWaveMinC);
  int value = (int)round(normalized * TEMP_WAVE_PLOT_MAX);

  if (value < 0) value = 0;
  if (value > TEMP_WAVE_PLOT_MAX) value = TEMP_WAVE_PLOT_MAX;

  return (uint8_t)value;
}

void resetFermentationWaveform(float centerTempC) {
  fermentationWaveMinC = centerTempC - FERMENTATION_WAVE_HALF_SPAN_C;
  fermentationWaveMaxC = centerTempC + FERMENTATION_WAVE_HALF_SPAN_C;
  fermentationWaveRangeInitialized = true;

  clearFermentationWaveformChannels();
  updateFermentationYAxisLabels();
  Serial.println("Fermentation waveform reset");
}

void updateFermentationWaveform(float tempC) {
  if (!fermentationWaveRangeInitialized) {
    resetFermentationWaveform(tempC);
  }

  float desiredMin = tempC - FERMENTATION_WAVE_HALF_SPAN_C;
  float desiredMax = tempC + FERMENTATION_WAVE_HALF_SPAN_C;

  if (fermentationTempCount > 0) {
    desiredMin = min(desiredMin, fermentationMinTempC - 0.5f);
    desiredMax = max(desiredMax, fermentationMaxTempC + 0.5f);
  }

  bool rangeChanged = false;
  if (desiredMin < fermentationWaveMinC) {
    fermentationWaveMinC = desiredMin;
    rangeChanged = true;
  }
  if (desiredMax > fermentationWaveMaxC) {
    fermentationWaveMaxC = desiredMax;
    rangeChanged = true;
  }

  updateFermentationYAxisLabels();

  if (rangeChanged) {
    clearFermentationWaveformChannels();
  }

  uint8_t scaledTemp = scaleFermentationTempToWave(tempC);
  waveFermentationTemp.addValue(FERMENTATION_WAVE_CHANNEL_TEMP, scaledTemp);

  Serial.print("Fermentation waveform -> Temp: ");
  Serial.print(tempC, 2);
  Serial.print(" Scaled: ");
  Serial.println(scaledTemp);
}

void updateFermentationDisplay() {
  liveTempC = getLiveTempC();
  updateFermentationStats(liveTempC);

  float dayAvg = fermentationTempCount > 0
    ? fermentationTempSumC / (float)fermentationTempCount
    : liveTempC;
  float sixHrAvg = fermentationSixHourTempCount > 0
    ? fermentationSixHourTempSumC / (float)fermentationSixHourTempCount
    : liveTempC;
  float minTemp = fermentationTempCount > 0 ? fermentationMinTempC : liveTempC;
  float maxTemp = fermentationTempCount > 0 ? fermentationMaxTempC : liveTempC;

  sendNumber("temp", (int32_t)round(liveTempC * DECIMAL_SCALE));
  sendNumber("daymintemp", (int32_t)round(minTemp * DECIMAL_SCALE));
  sendNumber("daymaxtemp", (int32_t)round(maxTemp * DECIMAL_SCALE));
  sendNumber("sixhravgtemp", (int32_t)round(sixHrAvg * DECIMAL_SCALE));
  sendNumber("dayavgtemp", (int32_t)round(dayAvg * DECIMAL_SCALE));
  if (fermentationLogFileName.length() > 0) {
    sendText("filename", fermentationLogFileName);
  }
  sendText("timeElapsed", formatFermentationElapsedTime(getFermentationElapsedMs()));
  sendText("status", fermentationStatusMessage.length() > 0 ? fermentationStatusMessage : fermentationStateText());
  sendText("fermBtn", fermentationButtonText());
  sendText("sgstatus", fermentationSGStatusMessage.length() > 0 ? fermentationSGStatusMessage : "NO SG YET");
  updateFermentationWaveform(liveTempC);
}

void logFermentationSGEntry(float sgValue) {
  if (fermentationLogFileName.length() == 0 || !fermentationLogActive) {
    fermentationSGStatusMessage = "CREATE FILE FIRST";
    sendText("sgstatus", fermentationSGStatusMessage);
    fermentationStatusMessage = "CREATE FILE FIRST";
    return;
  }

  hasFermentationSG = true;
  lastFermentationSG = sgValue;

  String elapsed = formatFermentationElapsedTime(getFermentationElapsedMs());
  fermentationSGStatusMessage = "SG " + String(sgValue, 3) + " @ " + elapsed;
  sendText("sgstatus", fermentationSGStatusMessage);

  float dayAvg = fermentationTempCount > 0
    ? fermentationTempSumC / (float)fermentationTempCount
    : liveTempC;
  float sixHrAvg = fermentationSixHourTempCount > 0
    ? fermentationSixHourTempSumC / (float)fermentationSixHourTempCount
    : liveTempC;
  float minTemp = fermentationTempCount > 0 ? fermentationMinTempC : liveTempC;
  float maxTemp = fermentationTempCount > 0 ? fermentationMaxTempC : liveTempC;

  appendFermentationLogLine("SG_ENTRY", liveTempC, minTemp, maxTemp, sixHrAvg, dayAvg,
                            getFermentationElapsedMs() / 1000UL, sgValue,
                            getLiveResistanceOhms());

  appendFermentationEventLog("SG_ENTRY", String(sgValue, 3) + " @ " + elapsed);
}

void updateFermentationPageOnEnter() {
  setHeater(false);
  if (fermentationStatusMessage.length() == 0) {
    fermentationStatusMessage = fermentationStateText();
  }
  resetFermentationWaveform(getLiveTempC());
  updateFermentationDisplay();
}

String coolingButtonText() {
  switch (coolingState) {
    case COOLING_IDLE: return "START COOLING";
    case COOLING_ACTIVE: return "END COOLING";
    case COOLING_READY_TO_LOAD: return "LOAD VOLUME";
    case COOLING_COMPLETE: return "LOADED";
    default: return "START COOLING";
  }
}

void updateCoolingYAxisLabels() {
  float yMin = COOLING_MODEL_AMBIENT_TEMP_C;
  float yMax = COOLING_START_TEMP_C;
  float span = yMax - yMin;
  float tickSpan = (float)(TEMP_WAVE_TOP_TICK - TEMP_WAVE_BOTTOM_TICK);
  float lowerMid = yMin + (span * (float)TEMP_WAVE_LOWER_MID_TICK / tickSpan);
  float midVal = yMin + (span * (float)TEMP_WAVE_MID_TICK / tickSpan);
  float upperMid = yMin + (span * (float)TEMP_WAVE_UPPER_MID_TICK / tickSpan);

  sendText("bottom", String(yMin, 1));
  sendText("lowermid", String(lowerMid, 1));
  sendText("mid", String(midVal, 1));
  sendText("uppermid", String(upperMid, 1));
  sendText("top", String(yMax, 1));
}

uint8_t scaleCoolingTempToWave(float tempC) {
  float yMin = COOLING_MODEL_AMBIENT_TEMP_C;
  float yMax = COOLING_START_TEMP_C;

  if (tempC < yMin) tempC = yMin;
  if (tempC > yMax) tempC = yMax;

  float normalized = (tempC - yMin) / (yMax - yMin);
  int value = (int)round(normalized * COOLING_WAVE_PLOT_MAX);

  if (value < 0) value = 0;
  if (value > COOLING_WAVE_PLOT_MAX) value = COOLING_WAVE_PLOT_MAX;

  return (uint8_t)value;
}

void resetCoolingWaveform() {
  clearCoolingWaveformChannels();
  updateCoolingYAxisLabels();
  Serial.println("Cooling waveform reset");
}

void updateCoolingWaveform() {
  float liveT = getLiveTempC();
  updateCoolingYAxisLabels();

  uint8_t liveScaled = scaleCoolingTempToWave(liveT);
  uint8_t targetScaled = scaleCoolingTempToWave(COOLING_TARGET_TEMP_C);

  waveFermentationTemp.addValue(COOLING_WAVE_CHANNEL_LIVE, liveScaled);
  waveFermentationTemp.addValue(COOLING_WAVE_CHANNEL_TARGET, targetScaled);

  Serial.print("Cooling waveform -> Live: ");
  Serial.print(liveScaled);
  Serial.print(" Target: ");
  Serial.println(targetScaled);
}

void updateCoolingEstimateText() {
  if (coolingState != COOLING_ACTIVE) {
    if (coolingState == COOLING_IDLE) {
      sendText("coolingupdate", "Press Start Cooling");
    } else if (coolingState == COOLING_READY_TO_LOAD) {
      sendText("coolingupdate", "Cooling Ended - Load Vol");
    } else if (coolingState == COOLING_COMPLETE) {
      if (coolingCaptureSource == VOLUME_CAPTURE_MANUAL) {
        sendText("coolingupdate", "Manual Vol Loaded");
      } else if (coolingCaptureSource == VOLUME_CAPTURE_TOF) {
        sendText("coolingupdate", "TOF Vol Loaded");
      }
    }
    return;
  }

  float currentTemp = getLiveTempC();
  unsigned long now = millis();

  if (currentTemp <= COOLING_TARGET_TEMP_C) {
    sendText("coolingupdate", "At Pitch Temp");
    return;
  }

  if (lastCoolingEstimateMs == 0) {
    lastCoolingEstimateMs = now;
    lastCoolingEstimateTempC = currentTemp;
    sendText("coolingupdate", "Estimating...");
    return;
  }

  float dtSec = (now - lastCoolingEstimateMs) / 1000.0f;
  float ambientC = processAmbientTempC;
  float prevDelta = lastCoolingEstimateTempC - ambientC;
  float currDelta = currentTemp - ambientC;

  if (dtSec < 2.0f || prevDelta <= COOLING_MIN_MODEL_DELTA_C || currDelta <= COOLING_MIN_MODEL_DELTA_C) {
    sendText("coolingupdate", "Estimating...");
    return;
  }

  float ratio = currDelta / prevDelta;
  if (ratio > 0.0f && ratio < 1.0f) {
    float instantK = -logf(ratio) / dtSec;
    if (instantK > 0.0f && isfinite(instantK)) {
      if (coolingRateConstantPerSec <= 0.0f) {
        coolingRateConstantPerSec = instantK;
      } else {
        coolingRateConstantPerSec =
          coolingRateConstantPerSec + BOIL_RATE_ALPHA * (instantK - coolingRateConstantPerSec);
      }
    }
  }

  float cumulativeK = 0.0f;
  float totalCoolingSec = (now - coolingStartMs) / 1000.0f;
  float startDelta = coolingEstimateStartTempC - ambientC;
  if (totalCoolingSec >= ESTIMATE_MIN_ELAPSED_SEC &&
      startDelta > COOLING_MIN_MODEL_DELTA_C &&
      currDelta > COOLING_MIN_MODEL_DELTA_C &&
      currDelta < startDelta) {
    cumulativeK = logf(startDelta / currDelta) / totalCoolingSec;
  }

  float effectiveK = blendEstimate(coolingRateConstantPerSec, cumulativeK);
  float targetDelta = COOLING_TARGET_TEMP_C - ambientC;
  if (effectiveK <= 0.0f || targetDelta <= COOLING_MIN_MODEL_DELTA_C || currDelta <= targetDelta) {
    sendText("coolingupdate", currentTemp <= COOLING_TARGET_TEMP_C ? "At Pitch Temp" : "Estimating...");
  } else {
    float remainingSec = logf(currDelta / targetDelta) / effectiveK;
    if (!isfinite(remainingSec) || remainingSec < 0.0f || remainingSec > 6.0f * 3600.0f) {
      sendText("coolingupdate", "Estimating...");
    } else {
      sendText("coolingupdate", "ETA to 25C: " + formatElapsedTime((unsigned long)round(remainingSec * 1000.0f)));
    }
  }

  lastCoolingEstimateMs = now;
  lastCoolingEstimateTempC = currentTemp;
}

float getCoolingDisplayVolume() {
  if (coolingVolumeLocked) return lockedCoolingVolumeGal;
  if (boilEndVolumeLocked) return lockedBoilEndVolumeGal;
  return getBoilDisplayVolume();
}

void updateCoolingDisplay() {
  float liveT = getLiveTempC();
  sendNumber("temp", (int32_t)round(liveT * DECIMAL_SCALE));
  sendNumber("vol", processVolToNextion(getCoolingDisplayVolume()));
  sendText("coolingbtn", coolingButtonText());

  unsigned long elapsedMs = 0;
  if (coolingStarted) {
    unsigned long endMs = (coolingState == COOLING_ACTIVE) ? millis() : coolingEndMs;
    elapsedMs = endMs - coolingStartMs;
  }
  sendText("time", formatElapsedTime(elapsedMs));

  updateCoolingEstimateText();
}

void startCoolingProcess() {
  coolingState = COOLING_ACTIVE;
  coolingStarted = true;
  coolingStartMs = millis();
  coolingEndMs = 0;
  coolingVolumeLocked = false;
  coolingCaptureSource = VOLUME_CAPTURE_NONE;
  lastCoolingEstimateMs = millis();
  lastCoolingEstimateTempC = getLiveTempC();
  coolingEstimateStartTempC = lastCoolingEstimateTempC;
  coolingRateConstantPerSec = 0.0f;
  setHeater(false);
  resetCoolingWaveform();
  appendEventLog("COOLING_START", "");
}

void endCoolingProcess() {
  coolingState = COOLING_READY_TO_LOAD;
  coolingEndMs = millis();
  setHeater(false);
  appendEventLog("COOLING_END", formatElapsedTime(coolingEndMs - coolingStartMs));
}

bool captureCoolingVolume(float volumeGal, VolumeCaptureSource source) {
  if (volumeGal < 0.0f) return false;

  lockedCoolingVolumeGal = volumeGal;
  coolingVolumeLocked = true;
  coolingCaptureSource = source;
  coolingState = COOLING_COMPLETE;

  sendNumber("vol", processVolToNextion(lockedCoolingVolumeGal));
  appendEventLog(source == VOLUME_CAPTURE_MANUAL ? "COOLING_VOL_MANUAL" : "COOLING_VOL_TOF",
                 String(lockedCoolingVolumeGal, 2));
  return true;
}

void updateCoolingPageOnEnter() {
  setHeater(false);
  resetCoolingWaveform();
  updateCoolingDisplay();
}
