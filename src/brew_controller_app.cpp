#include "brew_controller.h"

namespace {
void toCalMenuPushCallback(void *ptr) { setCurrentPage(PAGE_CAL); }

void toProcessSetupPushCallback(void *ptr) {
  setCurrentPage(PAGE_PROCESS_SETUP);
  updateProcessSetupLoadedObjects();
  updateSDCardStatus(sdReady ? "READY" : "NO CARD");
  if (runName.length() > 0) sendText("filename", runName);
}

void toManualPushCallback(void *ptr) { setCurrentPage(PAGE_MANUAL); }
void toLogsPushCallback(void *ptr) {
  setCurrentPage(PAGE_LOGS);
}
void setupToMenuPushCallback(void *ptr) { setCurrentPage(PAGE_MAIN_MENU); }

void toFillPushCallback(void *ptr) {
  setCurrentPage(PAGE_FILL);
  updateFillStatusObjects();
  updateFillLiveDisplay();

  if (fillStabilizing) {
    updateFillResultText("STABILIZING");
  } else if (fillVolumeLocked) {
    String prefix = (fillCaptureSource == VOLUME_CAPTURE_MANUAL) ? "MANUAL " : "LOCKED ";
    updateFillResultText(prefix + String(lockedFillVolumeGal, 2) + " gal");
  } else {
    updateFillResultText("FILLING");
  }
}

void fillToSetupPushCallback(void *ptr) { setCurrentPage(PAGE_PROCESS_SETUP); }

void fillToMashPushCallback(void *ptr) {
  setCurrentPage(PAGE_MASH);
  updateMashPageOnEnter();
}

void boilMainPushCallback(void *ptr) {
  if (currentPage != PAGE_BOIL) {
    setCurrentPage(PAGE_BOIL);
  }

  if (boilState == BOIL_IDLE) {
    if (!isBoilAllowedToStart()) {
      sendText("status", "Mash Not Ready");
      return;
    }
    startBoilProcess();
  } else if (boilState == BOIL_READY_CONFIRM) {
    enterBoilingState();
  } else if (boilState == BOIL_ACTIVE) {
    stopBoilProcess();
  } else if (boilState == BOIL_STOPPED) {
    startBoilCoolingProcess();
  } else if (boilState == BOIL_COOLING_ACTIVE) {
    stopBoilCoolingProcess();
  } else if (boilState == BOIL_COOLING_READY_TO_LOAD) {
    float manualVolumeGal = 0.0f;
    if (readVolumeInputGallons("fillvol", manualVolumeGal, 3) && manualVolumeGal > 0.0005f) {
      captureBoilEndVolume(manualVolumeGal, VOLUME_CAPTURE_MANUAL);
    } else {
      if (!levelCalValid) {
        sendText("status", "No Level Cal");
        return;
      }
      if (!isLiveLevelVolumeReliable()) {
        sendText("status", "Low Level");
        return;
      }
      captureBoilEndVolume(getLiveVolumeFromTOF(), VOLUME_CAPTURE_TOF);
    }
  }

  updateBoilDisplay();
}

void boilPlusPushCallback(void *ptr) {
  if (boilState == BOIL_ACTIVE) {
    incrementBoilControlPct();
    updateBoilDisplay();
  }
}

void boilMinusPushCallback(void *ptr) {
  if (boilState == BOIL_ACTIVE) {
    decrementBoilControlPct();
    updateBoilDisplay();
  }
}

void mashToFillPushCallback(void *ptr) { setCurrentPage(PAGE_FILL); }

void mashToBoilPushCallback(void *ptr) {
  if (!isBoilAllowedToStart()) {
    stopActiveProcessControls();
    setCurrentPage(PAGE_BOIL);
    updateBoilPageOnEnter();
    sendText("status", "Mash Stopped");
    return;
  }

  setCurrentPage(PAGE_BOIL);
  updateBoilPageOnEnter();
}

void boilToMashPushCallback(void *ptr) {
  setCurrentPage(PAGE_MASH);
  updateMashPageOnEnter();
}

void boilToCoolingPushCallback(void *ptr) {
  stopActiveProcessControls();

  if (boilState == BOIL_COMPLETE) {
    finishRunLog("boil_complete_to_logs");
  }

  setCurrentPage(PAGE_LOGS);
}

void fermentationHomePushCallback(void *ptr) { setCurrentPage(PAGE_MAIN_MENU); }

void fermentationMainPushCallback(void *ptr) {
  if (currentPage != PAGE_FERMENTATION) {
    setCurrentPage(PAGE_FERMENTATION);
  }

  switch (fermentationState) {
    case FERMENTATION_NEEDS_FILE:
    case FERMENTATION_STOPPED:
      createFermentationFileFromHMI();
      break;
    case FERMENTATION_FILE_READY:
      startFermentationLogging();
      break;
    case FERMENTATION_LOGGING:
      pitchFermentationYeast();
      break;
    case FERMENTATION_PITCHED:
      stopFermentationProcess();
      break;
  }

  updateFermentationDisplay();
}

void fermentationSGLoadPushCallback(void *ptr) {
  if (currentPage != PAGE_FERMENTATION) {
    setCurrentPage(PAGE_FERMENTATION);
  }

  uint32_t sg_raw = 0;

  if (!readNumberWithRetry(numFermentationSG, sg_raw, 3)) {
    fermentationSGStatusMessage = "SG READ FAIL";
    sendText("sgstatus", fermentationSGStatusMessage);
    Serial.println("FERM SG READ FAIL");
    return;
  }

  float sgValue = sg_raw / 1000.0f;

  if (sgValue < 0.900f || sgValue > 1.200f) {
    fermentationSGStatusMessage = "SG OUT OF RANGE";
    sendText("sgstatus", fermentationSGStatusMessage);
    Serial.print("FERM SG OUT OF RANGE: ");
    Serial.println(sgValue, 3);
    return;
  }

  logFermentationSGEntry(sgValue);
}

void manualToMenuPushCallback(void *ptr) { setCurrentPage(PAGE_MAIN_MENU); }
void rackToLogsPushCallback(void *ptr) {
  setCurrentPage(PAGE_LOGS);
}
void logsToMenuPushCallback(void *ptr) { setCurrentPage(PAGE_MAIN_MENU); }
void logsToFermentationPushCallback(void *ptr) { setCurrentPage(PAGE_FERMENTATION); }
void tempCalToMenuPushCallback(void *ptr) { setCurrentPage(PAGE_MAIN_MENU); }
void levelCalToMenuPushCallback(void *ptr) { setCurrentPage(PAGE_MAIN_MENU); }
void pumpCalToMenuPushCallback(void *ptr) { setCurrentPage(PAGE_MAIN_MENU); }

void calToTempCalPushCallback(void *ptr) {
  setCurrentPage(PAGE_TEMP_CAL);
}

void calToLevelCalPushCallback(void *ptr) {
  setCurrentPage(PAGE_LEVEL_CAL);

  sendNumber("rawVoltage", (int32_t)round(rawDistanceMM * LEVEL_VOLUME_XFLOAT_SCALE));
  sendNumber("x0", levelVolumeToXFloat(liveVolume));
}

void calToPumpCalPushCallback(void *ptr) { setCurrentPage(PAGE_PUMP_CAL); }

void loadSetupPushCallback(void *ptr) {
  uint32_t tempMashDur = 0;
  uint32_t tempMashTempRawUnsigned = 0;
  uint32_t tempBoilDur = 0;
  String tempRunName = "";

  bool gotName = readTextValue(txtFilename, tempRunName);
  bool gotMashDur = numMashDuration.getValue(&tempMashDur);
  bool gotMashTemp = numMashTemp.getValue(&tempMashTempRawUnsigned);
  bool gotBoilDur = numBoilDuration.getValue(&tempBoilDur);

  if (!gotName || tempRunName.length() == 0) tempRunName = "RUN_UNNAMED";
  if (gotMashDur) mashDurationMin = tempMashDur;
  if (gotMashTemp) {
    mashTempRaw = (int32_t)tempMashTempRawUnsigned;
    mashTempC = mashTempRaw / 100.0f;
  }
  if (gotBoilDur) boilDurationMin = tempBoilDur;

  runName = tempRunName;
  runName.trim();

  updateProcessSetupLoadedObjects();

  initSDCard();
  if (!sdReady) {
    updateSDCardStatus("NO CARD");
    return;
  }

  bool renamed = false;
  currentLogFileName = makeUniqueCsvFileName(runName, renamed);

  if (currentLogFileName.length() == 0) {
    updateSDCardStatus("NAME FAIL");
    return;
  }

  bool createOK = createRunFileWithSetup();
  updateSDCardStatus(createOK ? (renamed ? "EXISTS->NEW" : "CREATED") : "WRITE FAIL");
}

void firstCalPushCallback(void *ptr) {
  float measuredResistance = readRTDResistanceOhms();
  R1_val = measuredResistance;
  T1_val = 0.0f;
  storeActiveTempProbeCalibration();
  updateTempCalStoredValuesOnHMI();
  setTempCalStatusText(String(temperatureProbeName(activeTempCalProbe)) + " 0C SNAP");
}

void secondCalPushCallback(void *ptr) {
  float measuredResistance = readRTDResistanceOhms();
  R2_val = measuredResistance;
  T2_val = 100.0f;
  storeActiveTempProbeCalibration();
  updateTempCalStoredValuesOnHMI();
  setTempCalStatusText(String(temperatureProbeName(activeTempCalProbe)) + " 100C SNAP");
}

void loadCalPushCallback(void *ptr) {
  uint32_t r1_raw = 0, t1_raw = 0, r2_raw = 0, t2_raw = 0;

  bool okR1 = numR1.getValue(&r1_raw);
  bool okT1 = numT1.getValue(&t1_raw);
  bool okR2 = numR2.getValue(&r2_raw);
  bool okT2 = numT2.getValue(&t2_raw);

  if (!(okR1 && okT1 && okR2 && okT2)) return;

  R1_val = r1_raw / 100.0f;
  T1_val = t1_raw / 100.0f;
  R2_val = r2_raw / 100.0f;
  T2_val = t2_raw / 100.0f;
  storeActiveTempProbeCalibration();

  saveCalibrationToEEPROM();
  updateTempCalStoredValuesOnHMI();
  setTempCalStatusText(String(temperatureProbeName(activeTempCalProbe)) + " CAL SAVED");
  appendTempCalibrationLog();
}

void switchTempCalProbePushCallback(void *ptr) {
  storeActiveTempProbeCalibration();
  activeTempCalProbe = (activeTempCalProbe == TEMP_PROBE_PROCESS)
    ? TEMP_PROBE_FERMENTATION
    : TEMP_PROBE_PROCESS;
  applyActiveTempProbeCalibration();
  updateTempCalStoredValuesOnHMI();
  updateTempCalProbeText();
  setTempCalStatusText(String(temperatureProbeName(activeTempCalProbe)) + " SELECTED");
}

void levelLoadCalPushCallback(void *ptr) {
  uint32_t vol_raw = 0;
  float capturedVoltage = distanceInitialized ? filtDistanceMM : readHydrostaticPressureSensorVoltage();

  if (!isfinite(capturedVoltage) ||
      capturedVoltage < MIN_VALID_LEVEL_SENSOR_V ||
      capturedVoltage > MAX_VALID_LEVEL_SENSOR_V) {
    levelCalValid = false;
    updateLevelStatusText("BAD VOLTAGE");
    return;
  }

  if (!levelCalLowerPointCaptured) {
    if (!numLevelVol1.getValue(&vol_raw)) {
      updateLevelStatusText("VOL1 READ FAIL");
      return;
    }

    levelVol1 = vol_raw / (float)LEVEL_VOLUME_XFLOAT_SCALE;
    levelDis1 = capturedVoltage;
    levelCalLowerPointCaptured = true;
    levelCalValid = false;

    updateLevelCalStoredValuesOnHMI();
    updateLevelLoadCalButtonText();
    updateLevelCalibrationStatus();
    return;
  }

  if (!numLevelVol2.getValue(&vol_raw)) {
    updateLevelStatusText("VOL2 READ FAIL");
    return;
  }

  levelVol2 = vol_raw / (float)LEVEL_VOLUME_XFLOAT_SCALE;
  levelDis2 = capturedVoltage;

  bool ok = computeLevelCalibration();
  if (ok) {
    saveLevelCalibrationToEEPROM();
    levelCalLowerPointCaptured = false;
    updateLevelCalStoredValuesOnHMI();
    updateLevelLoadCalButtonText();
    updateLevelCalibrationStatus();
  } else {
    updateLevelStatusText("BAD CAL");
  }
}

void levelForceCalPushCallback(void *ptr) {
  uint32_t vol1_raw = 0;
  uint32_t vol2_raw = 0;
  uint32_t forceVoltage1Raw = 0;
  uint32_t forceVoltage2Raw = 0;

  bool gotVol1 = numLevelVol1.getValue(&vol1_raw);
  bool gotVol2 = numLevelVol2.getValue(&vol2_raw);
  bool gotForceVoltage1 = numForceVoltage1.getValue(&forceVoltage1Raw);
  bool gotForceVoltage2 = numForceVoltage2.getValue(&forceVoltage2Raw);

  if (!(gotVol1 && gotVol2)) {
    updateLevelStatusText("FORCE VOL READ FAIL");
    return;
  }

  if (!(gotForceVoltage1 && gotForceVoltage2)) {
    updateLevelStatusText("FORCE V READ FAIL");
    return;
  }

  if (forceVoltage1Raw == 0 || forceVoltage2Raw == 0) {
    updateLevelStatusText("FORCE V ZERO");
    return;
  }

  float forceVoltage1 = forceVoltage1Raw / (float)LEVEL_VOLUME_XFLOAT_SCALE;
  float forceVoltage2 = forceVoltage2Raw / (float)LEVEL_VOLUME_XFLOAT_SCALE;

  if (!isfinite(forceVoltage1) || !isfinite(forceVoltage2) ||
      forceVoltage1 < MIN_VALID_LEVEL_SENSOR_V ||
      forceVoltage1 > MAX_VALID_LEVEL_SENSOR_V ||
      forceVoltage2 < MIN_VALID_LEVEL_SENSOR_V ||
      forceVoltage2 > MAX_VALID_LEVEL_SENSOR_V) {
    updateLevelStatusText("FORCE V BAD");
    return;
  }

  levelVol1 = vol1_raw / (float)LEVEL_VOLUME_XFLOAT_SCALE;
  levelVol2 = vol2_raw / (float)LEVEL_VOLUME_XFLOAT_SCALE;
  levelDis1 = forceVoltage1;
  levelDis2 = forceVoltage2;

  bool ok = computeLevelCalibration();
  if (ok) {
    saveLevelCalibrationToEEPROM();
    levelCalLowerPointCaptured = false;
    updateLevelCalStoredValuesOnHMI();
    updateLevelLoadCalButtonText();
    updateLevelStatusText("FORCED CAL SAVED");
  } else {
    updateLevelStatusText("BAD FORCE CAL");
  }
}

void fillDonePushCallback(void *ptr) {
  if (!levelCalValid) {
    updateFillResultText("NO LEVEL CAL");
    return;
  }

  if (fillStabilizing) return;

  fillVolumeLocked = false;
  startFillStabilization();
}

void fillManualLoadPushCallback(void *ptr) {
  float manualVolumeGal = 0.0f;

  if (!readVolumeInputGallons("fillvol", manualVolumeGal, 3)) {
    updateFillResultText("MAN VOL READ FAIL");
    return;
  }

  if (manualVolumeGal <= 0.0005f) {
    updateFillResultText("ENTER MANUAL VOL");
    return;
  }

  fillStabilizing = false;
  lockedFillVolumeGal = manualVolumeGal;
  fillVolumeLocked = true;
  fillCaptureSource = VOLUME_CAPTURE_MANUAL;

  sendNumber("vol", processVolToNextion(lockedFillVolumeGal));
  updateFillResultText("MANUAL " + String(lockedFillVolumeGal, 2) + " gal");
  appendEventLog("FILL_LOCKED_MANUAL", String(lockedFillVolumeGal, 2));
}

void mashActionPushCallback(void *ptr) {
  switch (mashState) {
    case MASH_IDLE: startMashProcess(); break;
    case MASH_HEATING_STRIKE: break;
    case MASH_WAIT_FOR_BAG: markMashIn(); break;
    case MASH_HOLD:
      if (getMashTimeRemainingSec() <= 0) {
        markMashOut();
      }
      break;
    case MASH_WAIT_REMOVE_BAG: startMashVolumeCapture(); break;
    case MASH_CAPTURE_VOLUME:
    case MASH_COMPLETE:
      break;
  }

  updateMashDisplay();
}

void mashSGLoadPushCallback(void *ptr) {
  uint32_t sg_raw = 0;

  if (!readNumberWithRetry(numMashSG, sg_raw, 3)) {
    sendText("sgstatus", "SG READ FAIL");
    Serial.println("SG READ FAIL");
    return;
  }

  float sgValue = sg_raw / 1000.0f;

  if (sgValue < 0.900f || sgValue > 1.200f) {
    sendText("sgstatus", "SG OUT OF RANGE");
    Serial.print("SG OUT OF RANGE: ");
    Serial.println(sgValue, 3);
    return;
  }

  logMashSGEntry(sgValue);
}
}  // namespace

void setupController() {
  Serial.begin(115200);
  nexSerial.begin(9600);

  pinMode(HEATER_RELAY_PIN, OUTPUT);
  digitalWrite(HEATER_RELAY_PIN, LOW);
  pinMode(LEVEL_PRESSURE_SENSOR_PIN, INPUT);
  analogReadResolution(LEVEL_ADC_BITS);
  analogReadAveraging(16);

  thermo.begin(MAX31865_3WIRE);
  nexInit();

  loadCalibrationFromEEPROM();
  loadLevelCalibrationFromEEPROM();
  initSDCard();

  btnToCalMenu.attachPush(toCalMenuPushCallback);
  btnToProcessSetup.attachPush(toProcessSetupPushCallback);
  btnToManual.attachPush(toManualPushCallback);
  btnToLogs.attachPush(toLogsPushCallback);
  btnSetupToMenu.attachPush(setupToMenuPushCallback);
  btnSetupToFill.attachPush(toFillPushCallback);
  btnFillToSetup.attachPush(fillToSetupPushCallback);
  btnFillToMash.attachPush(fillToMashPushCallback);
  btnFillDone.attachPush(fillDonePushCallback);
  btnFillManualLoad.attachPush(fillManualLoadPushCallback);
  btnMashToFill.attachPush(mashToFillPushCallback);
  btnMashToBoil.attachPush(mashToBoilPushCallback);
  btnMashAction.attachPush(mashActionPushCallback);
  btnMashSGLoad.attachPush(mashSGLoadPushCallback);
  btnBoilToMash.attachPush(boilToMashPushCallback);
  btnBoilToCooling.attachPush(boilToCoolingPushCallback);
  btnBoilMain.attachPush(boilMainPushCallback);
  btnBoilPlus.attachPush(boilPlusPushCallback);
  btnBoilMinus.attachPush(boilMinusPushCallback);
  btnFermentationMain.attachPush(fermentationMainPushCallback);
  btnFermentationSGLoad.attachPush(fermentationSGLoadPushCallback);
  btnFermentationHome.attachPush(fermentationHomePushCallback);
  btnRackToLogs.attachPush(rackToLogsPushCallback);
  btnManualToMenu.attachPush(manualToMenuPushCallback);
  btnLogsToMenu.attachPush(logsToMenuPushCallback);
  btnLogsToFermentation.attachPush(logsToFermentationPushCallback);
  btnTempCalToMenu.attachPush(tempCalToMenuPushCallback);
  btnLevelCalToMenu.attachPush(levelCalToMenuPushCallback);
  btnPumpCalToMenu.attachPush(pumpCalToMenuPushCallback);
  btnCalToTempCal.attachPush(calToTempCalPushCallback);
  btnCalToLevelCal.attachPush(calToLevelCalPushCallback);
  btnCalToPumpCal.attachPush(calToPumpCalPushCallback);
  btnLoadSetup.attachPush(loadSetupPushCallback);
  btnFirstCal.attachPush(firstCalPushCallback);
  btnSecondCal.attachPush(secondCalPushCallback);
  btnLoadCal.attachPush(loadCalPushCallback);
  btnSwitchTempCalProbe.attachPush(switchTempCalProbePushCallback);
  btnLevelLoadCal.attachPush(levelLoadCalPushCallback);
  btnLevelForceCal.attachPush(levelForceCalPushCallback);

  setHeater(false);

  Serial.println("Brew controller sketch started");
  Serial.print("Current Page: ");
  Serial.println(currentPage);
}

void loopController() {
  static unsigned long lastTempCalStatusUpdate = 0;
  static unsigned long lastLevelUpdate = 0;
  static unsigned long lastFillUpdate = 0;
  static unsigned long lastFillLog = 0;
  static unsigned long lastMashUpdate = 0;
  static unsigned long lastMashLog = 0;
  static unsigned long lastMashWaveUpdate = 0;
  static unsigned long lastBoilUpdate = 0;
  static unsigned long lastBoilLog = 0;
  static unsigned long lastBoilWaveUpdate = 0;
  static unsigned long lastFermentationUpdate = 0;
  static unsigned long lastFermentationLog = 0;
  handleNextionPageRefreshEvents();
  nexLoop(nex_listen_list);

  if (currentPage == PAGE_LEVEL_CAL || currentPage == PAGE_FILL || currentPage == PAGE_MASH || currentPage == PAGE_BOIL) {
    if (millis() - lastLevelUpdate >= 400) {
      lastLevelUpdate = millis();
      updateLevelLiveDisplay();
    }
  }

  if (currentPage == PAGE_TEMP_CAL) {
    updateTempCalLiveDisplay();

    if (millis() - lastTempCalStatusUpdate >= 1000) {
      lastTempCalStatusUpdate = millis();
      updateTempCalStatusText();
    }

    delay(200);
  }

  if (currentPage == PAGE_FILL) {
    if (millis() - lastFillUpdate >= 250) {
      lastFillUpdate = millis();
      updateFillLiveDisplay();
    }

    if (millis() - lastFillLog >= 1000) {
      lastFillLog = millis();
      appendRunLogLine(
        "FILL",
        fillStabilizing ? "STABILIZING" : (fillVolumeLocked ? "LOCKED" : "FILLING"),
        getLiveTempC(),
        0.0f,
        fillVolumeLocked ? lockedFillVolumeGal : getLiveVolumeFromTOF(),
        getLiveVolumeFromTOF(),
        filtDistanceMM,
        heaterOn,
        pumpOn,
        0,
        0.0f,
        getLiveResistanceOhms(),
        0.0f
      );
    }

    processFillStabilization();
  }

  if (currentPage == PAGE_MASH) {
    if (millis() - lastMashUpdate >= 250) {
      lastMashUpdate = millis();

      controlMashTemperature();
      updateMashStateMachine();
      updateMashDisplay();
      processMashVolumeCapture();
    }

    if (millis() - lastMashLog >= MASH_LOG_INTERVAL_MS) {
      lastMashLog = millis();
      appendRunLogLine(
        "MASH",
        mashStateText(),
        getLiveTempC(),
        getCurrentMashSetpointC(),
        getMashDisplayVolume(),
        getMashLiveCaptureVolume(),
        filtDistanceMM,
        heaterOn,
        pumpOn,
        getMashTimeRemainingSec(),
        hasLoggedSG ? lastLoggedSG : 0.0f,
        getLiveResistanceOhms(),
        0.0f
      );
    }

    if (millis() - lastMashWaveUpdate >= 2000) {
      lastMashWaveUpdate = millis();
      updateMashWaveformFixedRange();
    }
  }

  if (currentPage == PAGE_BOIL) {
    if (millis() - lastBoilUpdate >= 250) {
      lastBoilUpdate = millis();

      controlBoilHeater();
      updateBoilStateMachine();
      updateBoilDisplay();
    }

    if (millis() - lastBoilLog >= 5000) {
      lastBoilLog = millis();
      appendRunLogLine(
        "BOIL",
        boilStateText(),
        getLiveTempC(),
        (boilState == BOIL_ACTIVE ? 100.0f : BOIL_START_TEMP_C),
        getBoilDisplayVolume(),
        getLiveVolumeFromTOF(),
        filtDistanceMM,
        heaterOn,
        pumpOn,
        getBoilTimeRemainingSec(),
        0.0f,
        getLiveResistanceOhms(),
        ((boilState == BOIL_READY_CONFIRM || boilState == BOIL_ACTIVE) ? boilControlPct : 0.0f)
      );
    }

    if (millis() - lastBoilWaveUpdate >= 2000) {
      lastBoilWaveUpdate = millis();
      updateBoilWaveform();
    }
  }

  bool fermentationRunning =
    (fermentationState == FERMENTATION_LOGGING || fermentationState == FERMENTATION_PITCHED);

  if (currentPage == PAGE_FERMENTATION || fermentationRunning) {
    if (millis() - lastFermentationUpdate >= 1000) {
      lastFermentationUpdate = millis();

      if (currentPage == PAGE_FERMENTATION) {
        updateFermentationDisplay();
      } else if (fermentationRunning) {
        liveTempC = getLiveTempC();
        updateFermentationStats(liveTempC);
      }
    }

    if (fermentationRunning && millis() - lastFermentationLog >= 30000) {
      lastFermentationLog = millis();
      float tempC = getLiveTempC();
      float dayAvg = fermentationTempCount > 0
        ? fermentationTempSumC / (float)fermentationTempCount
        : tempC;
      float sixHrAvg = fermentationSixHourTempCount > 0
        ? fermentationSixHourTempSumC / (float)fermentationSixHourTempCount
        : tempC;
      float minTemp = fermentationTempCount > 0 ? fermentationMinTempC : tempC;
      float maxTemp = fermentationTempCount > 0 ? fermentationMaxTempC : tempC;

      appendFermentationLogLine(
        fermentationStateText(),
        tempC,
        minTemp,
        maxTemp,
        sixHrAvg,
        dayAvg,
        getFermentationElapsedMs() / 1000UL,
        hasFermentationSG ? lastFermentationSG : 0.0f,
        getLiveResistanceOhms()
      );
    }
  }

  if (currentPage != PAGE_MASH && currentPage != PAGE_BOIL &&
      mashState == MASH_IDLE && boilState != BOIL_HEATING && boilState != BOIL_ACTIVE) {
    setHeater(false);
  }
}
