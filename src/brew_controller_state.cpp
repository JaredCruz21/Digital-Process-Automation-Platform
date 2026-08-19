#include "brew_controller.h"

Adafruit_MAX31865 thermo(10, 11, 12, 13);

uint8_t currentPage = PAGE_MAIN_MENU;

bool sdReady = false;
String currentLogFileName = "";
bool runLogActive = false;

String runName = "";
uint32_t mashDurationMin = 0;
int32_t mashTempRaw = 0;
float mashTempC = 0.0f;
uint32_t boilDurationMin = 0;

float T1_val = 0.0f;
float R1_val = 100.00f;
float T2_val = 100.0f;
float R2_val = 138.51f;
float processT1_val = 0.0f;
float processR1_val = 100.00f;
float processT2_val = 100.0f;
float processR2_val = 138.51f;
float fermentationT1_val = 0.0f;
float fermentationR1_val = 100.00f;
float fermentationT2_val = 100.0f;
float fermentationR2_val = 138.51f;
TemperatureProbe activeTempCalProbe = TEMP_PROBE_PROCESS;
String tempCalStatusMessage = "";

float levelVol1 = 0.0f;
float levelVol2 = 0.0f;
float levelDis1 = 0.0f;
float levelDis2 = 0.0f;
float levelSlope = 0.0f;
float levelIntercept = 0.0f;
bool levelCalValid = false;
bool levelCalLowerPointCaptured = false;
float rawDistanceMM = 0.0f;
float filtDistanceMM = 0.0f;
float liveVolume = 0.0f;

float filteredDistanceMM = 0.0f;
bool distanceInitialized = false;

float lockedFillVolumeGal = 0.0f;
bool fillVolumeLocked = false;
bool fillStabilizing = false;
unsigned long fillStabilizeStartMs = 0;
float fillStabilizeSum = 0.0f;
uint16_t fillStabilizeCount = 0;
VolumeCaptureSource fillCaptureSource = VOLUME_CAPTURE_NONE;

MashState mashState = MASH_IDLE;
unsigned long mashStartMs = 0;
bool mashStarted = false;
bool mashCaptureStabilizing = false;
unsigned long mashCaptureStartMs = 0;
float mashCaptureSum = 0.0f;
uint16_t mashCaptureCount = 0;
float lockedBoilStartVolumeGal = 0.0f;
bool boilStartVolumeLocked = false;
VolumeCaptureSource mashCaptureSource = VOLUME_CAPTURE_NONE;
float lastLoggedSG = 0.0f;
bool hasLoggedSG = false;
float liveTempC = 0.0f;
bool heaterOn = false;
bool pumpOn = false;

BoilState boilState = BOIL_IDLE;
float boilWaveStartTempC = 20.0f;
float boilRateFilteredCPerMin = 0.0f;
float boilEstimateStartTempC = 20.0f;
bool boilStarted = false;
unsigned long boilStartMs = 0;
unsigned long boilActiveStartMs = 0;
unsigned long lastBoilEstimateMs = 0;
float lastBoilEstimateTempC = 0.0f;
float boilControlPct = 100.0f;
float lockedBoilEndVolumeGal = 0.0f;
bool boilEndVolumeLocked = false;
VolumeCaptureSource boilCaptureSource = VOLUME_CAPTURE_NONE;
CoolingState coolingState = COOLING_IDLE;
bool coolingStarted = false;
unsigned long coolingStartMs = 0;
unsigned long coolingEndMs = 0;
unsigned long lastCoolingEstimateMs = 0;
float lastCoolingEstimateTempC = 0.0f;
float coolingRateConstantPerSec = 0.0f;
float coolingEstimateStartTempC = 20.0f;
float processAmbientTempC = 23.0f;
float lockedCoolingVolumeGal = 0.0f;
bool coolingVolumeLocked = false;
VolumeCaptureSource coolingCaptureSource = VOLUME_CAPTURE_NONE;
FermentationState fermentationState = FERMENTATION_NEEDS_FILE;
String fermentationLogFileName = "";
bool fermentationLogActive = false;
String fermentationStatusMessage = "ENTER FILE";
String fermentationSGStatusMessage = "NO SG YET";
bool fermentationStarted = false;
bool fermentationYeastPitched = false;
unsigned long fermentationStartMs = 0;
unsigned long fermentationPitchMs = 0;
unsigned long fermentationStopMs = 0;
float fermentationMinTempC = 0.0f;
float fermentationMaxTempC = 0.0f;
float fermentationTempSumC = 0.0f;
uint32_t fermentationTempCount = 0;
unsigned long fermentationSixHourStartMs = 0;
float fermentationSixHourTempSumC = 0.0f;
uint32_t fermentationSixHourTempCount = 0;
float lastFermentationSG = 0.0f;
bool hasFermentationSG = false;
unsigned long lastFermentationStatsUpdateMs = 0;
float fermentationWaveMinC = 0.0f;
float fermentationWaveMaxC = 0.0f;
bool fermentationWaveRangeInitialized = false;

NexButton btnToCalMenu(0, 2, "tocal");
NexButton btnToProcessSetup(0, 3, "toprocesssetup");
NexButton btnToManual(0, 4, "tomanual");
NexButton btnToLogs(0, 5, "tologs");

NexButton btnSetupToMenu(1, 2, "tomenup1");
NexButton btnLoadSetup(1, 3, "load");
NexButton btnSetupToFill(1, 10, "toFill");
NexText txtFilename(1, 11, "filename");
NexNumber numMashDuration(1, 6, "duration");
NexNumber numMashTemp(1, 12, "mashTemp");
NexNumber numBoilDuration(1, 14, "boilduration");
NexNumber numLoadMashDur(1, 17, "loadmashdur");
NexNumber numLoadMashTemp(1, 18, "loadmashTemp");
NexNumber numLoadBoilDur(1, 19, "loadBoilDur");
NexText txtSDcard(1, 20, "SDcard");

NexButton btnFillToSetup(2, 12, "b0");
NexButton btnFillToMash(2, 13, "b1");
NexButton btnFillDone(2, 14, "filldone");
NexButton btnFillManualLoad(2, 18, "manLoad");
NexNumber numFillTemp(2, 3, "temp");
NexText txtFillAlarm(2, 5, "alarm");
NexText txtFillHXStatus(2, 8, "hxstatus");
NexText txtFillPumpStatus(2, 10, "pumpstatus");
NexNumber numFillVol(2, 11, "vol");
NexText txtFillResult(2, 16, "fillresult");

NexButton btnMashToFill(3, 13, "b0");
NexButton btnMashToBoil(3, 12, "b1");
NexNumber numMashTempLive(3, 3, "temp");
NexText txtMashAlarm(3, 5, "alarm");
NexText txtMashHXStatus(3, 27, "hxstatus");
NexText txtMashPumpStatus(3, 28, "pumpstatus");
NexNumber numMashVol(3, 11, "vol");
NexButton btnMashAction(3, 3, "actbtn");
NexText txtMashTimeRemain(3, 16, "timeRemain");
NexText txtMashState(3, 7, "mashstate");
NexNumber numMashSG(3, 8, "sg");
NexButton btnMashSGLoad(3, 9, "sgload");
NexText txtMashSGStatus(3, 10, "sgstatus");
NexNumber numMashFillVol(3, 19, "fillvol");
NexNumber numMashVolLive(3, 22, "volLive");
NexWaveform waveMashTemp(3, 12, "waveTemp");
NexNumber numMashTempSP(3, 24, "tempSP");
NexText txtWaveTop(3, 14, "top");
NexText txtWaveMid(3, 15, "mid");
NexText txtWaveUpperMid(3, 16, "uppermid");
NexText txtWaveLowerMid(3, 17, "lowermid");
NexText txtWaveBottom(3, 18, "bottom");

NexButton btnBoilToMash(4, 2, "b0");
NexButton btnBoilToCooling(4, 1, "b1");
NexNumber numBoilTemp(4, 13, "temp");
NexText txtBoilAlarm(4, 12, "alarm");
NexText txtBoilHXStatus(4, 18, "hxstatus");
NexText txtBoilPumpStatus(4, 19, "pumpstatus");
NexNumber numBoilVol(4, 15, "vol");
NexNumber numBoilFillVol(4, 10, "fillvol");
NexText txtBoilStatus(4, 3, "status");
NexText txtBoilTime(4, 4, "time");
NexText txtBoilPhase(4, 11, "phase");
NexWaveform waveBoilTemp(4, 23, "waveTemp");
NexText txtBoilTop(4, 24, "top");
NexText txtBoilUpperMid(4, 26, "uppermid");
NexText txtBoilMid(4, 25, "mid");
NexText txtBoilLowerMid(4, 27, "lowermid");
NexText txtBoilBottom(4, 28, "bottom");
NexButton btnBoilMain(4, 5, "boilbtn");
NexButton btnBoilPlus(4, 6, "plus");
NexButton btnBoilMinus(4, 7, "minus");
NexText txtBoilCtrlPct(4, 8, "contrlprct");
NexText txtBoilUpdates(4, 9, "updates");

NexText txtFermentationFilename(5, 1, "filename");
NexNumber numFermentationTemp(5, 2, "temp");
NexNumber numFermentationDayMinTemp(5, 3, "daymintemp");
NexNumber numFermentationDayMaxTemp(5, 4, "daymaxtemp");
NexNumber numFermentationSixHrAvgTemp(5, 5, "sixhravgtemp");
NexNumber numFermentationDayAvgTemp(5, 6, "dayavgtemp");
NexText txtFermentationTimeElapsed(5, 7, "timeElapsed");
NexButton btnFermentationMain(5, 8, "fermBtn");
NexButton btnFermentationSGLoad(5, 9, "sgload");
NexText txtFermentationSGStatus(5, 10, "sgstatus");
NexText txtFermentationStatus(5, 11, "status");
NexButton btnFermentationHome(5, 17, "b0");
NexNumber numFermentationSG(5, 18, "sg");
NexWaveform waveFermentationTemp(5, 19, "waveTemp");
NexText txtFermentationTop(5, 20, "top");
NexText txtFermentationUpperMid(5, 22, "uppermid");
NexText txtFermentationMid(5, 21, "mid");
NexText txtFermentationLowerMid(5, 23, "lowermid");
NexText txtFermentationBottom(5, 24, "bottom");

NexButton btnRackToCooling(6, 13, "b0");
NexButton btnRackToLogs(6, 12, "b1");

NexButton btnManualToMenu(6, 1, "tomenu");
NexButton btnLogsToMenu(7, 1, "tomenu");
NexButton btnLogsToFermentation(7, 2, "b0");
NexText txtLogsFileList(7, 3, "t0");

NexButton btnTempCalToMenu(8, 1, "tomenu");
NexButton btnFirstCal(8, 15, "firstCal");
NexButton btnSecondCal(8, 16, "secondCal");
NexButton btnLoadCal(8, 14, "loadcal");
NexText txtTempCalProbe(8, 19, "probe");
NexButton btnSwitchTempCalProbe(8, 20, "switchProbe");
NexNumber numCR(8, 17, "cR");
NexNumber numTempCal(8, 18, "temp");
NexText txtTempCalFault(8, 5, "t3");
NexNumber numR1(8, 7, "R1");
NexNumber numT1(8, 8, "T1");
NexNumber numR2(8, 10, "R2");
NexNumber numT2(8, 12, "T2");

NexButton btnLevelCalToMenu(9, 1, "tomenu");
NexButton btnLevelLoadCal(9, 4, "loadcal");
NexNumber numRawDistance(9, 3, "rawVoltage");
NexNumber numLiveVolume(9, 5, "x0");
NexText txtLevelStatus(9, 6, "status");
NexNumber numLevelVol1(9, 7, "vol1");
NexNumber numLevelVol2(9, 8, "vol2");
NexNumber numForceVoltage1(9, 14, "forceVoltage1");
NexNumber numForceVoltage2(9, 13, "forceVoltage2");
NexButton btnLevelForceCal(9, 18, "forceCal");

NexButton btnPumpCalToMenu(10, 1, "tomenu");
NexButton btnCalToTempCal(11, 1, "b0");
NexButton btnCalToLevelCal(11, 2, "b1");
NexButton btnCalToPumpCal(11, 3, "b2");

NexTouch *nex_listen_list[] = {
  &btnToCalMenu, &btnToProcessSetup, &btnToManual, &btnToLogs,
  &btnSetupToMenu, &btnSetupToFill,
  &btnFillToSetup, &btnFillToMash, &btnFillDone, &btnFillManualLoad,
  &btnMashToFill, &btnMashToBoil, &btnMashAction, &btnMashSGLoad,
  &btnBoilToMash, &btnBoilToCooling, &btnBoilMain, &btnBoilPlus, &btnBoilMinus,
  &btnFermentationMain, &btnFermentationSGLoad, &btnFermentationHome,
  &btnRackToLogs,
  &btnManualToMenu, &btnLogsToMenu, &btnLogsToFermentation,
  &btnTempCalToMenu, &btnLevelCalToMenu, &btnPumpCalToMenu,
  &btnCalToTempCal, &btnCalToLevelCal, &btnCalToPumpCal,
  &btnLoadSetup,
  &btnFirstCal, &btnSecondCal, &btnLoadCal,
  &btnSwitchTempCalProbe,
  &btnLevelLoadCal, &btnLevelForceCal,
  NULL
};

void sendTerminator() {
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}

void sendText(const char *objName, const String &txt) {
  nexSerial.print(objName);
  nexSerial.print(".txt=\"");
  nexSerial.print(txt);
  nexSerial.print("\"");
  sendTerminator();
}

void sendNumber(const char *objName, int32_t value) {
  nexSerial.print(objName);
  nexSerial.print(".val=");
  nexSerial.print(value);
  sendTerminator();
}

void setCurrentPage(uint8_t page) {
  currentPage = page;
  Serial.print("Current Page: ");
  Serial.println(currentPage);

  if (currentPage == PAGE_TEMP_CAL) {
    updateTempCalPageOnEnter();
  } else if (currentPage == PAGE_LEVEL_CAL) {
    updateLevelCalStoredValuesOnHMI();
    updateLevelLoadCalButtonText();
    updateLevelCalibrationStatus();
  } else if (currentPage == PAGE_FERMENTATION) {
    updateFermentationPageOnEnter();
  } else if (currentPage == PAGE_LOGS) {
    updateLogsFileList();
  }
}

void handleNextionPageRefreshEvents() {
  while (nexSerial.available() > 0 && nexSerial.peek() == 0x66) {
    unsigned long startMs = millis();
    while (nexSerial.available() < 5 && (millis() - startMs) < 20UL) {
      delay(1);
    }

    if (nexSerial.available() < 5) return;

    nexSerial.read();
    uint8_t pageId = nexSerial.read();
    uint8_t end1 = nexSerial.read();
    uint8_t end2 = nexSerial.read();
    uint8_t end3 = nexSerial.read();

    if (end1 == 0xFF && end2 == 0xFF && end3 == 0xFF) {
      setCurrentPage(pageId);
    }
  }
}

int32_t processVolToNextion(float volumeGal) {
  return (int32_t)round(volumeGal * PROCESS_VOL_DISPLAY_SCALE);
}

void clearMashWaveformChannels() {
  nexSerial.print("cle ");
  nexSerial.print(MASH_WAVE_COMPONENT_ID);
  nexSerial.print(",0");
  sendTerminator();
  nexSerial.print("cle ");
  nexSerial.print(MASH_WAVE_COMPONENT_ID);
  nexSerial.print(",1");
  sendTerminator();
}

void clearBoilWaveformChannels() {
  nexSerial.print("cle ");
  nexSerial.print(BOIL_WAVE_COMPONENT_ID);
  nexSerial.print(",0");
  sendTerminator();
  nexSerial.print("cle ");
  nexSerial.print(BOIL_WAVE_COMPONENT_ID);
  nexSerial.print(",1");
  sendTerminator();
}

void clearCoolingWaveformChannels() {
  nexSerial.print("cle ");
  nexSerial.print(COOLING_WAVE_COMPONENT_ID);
  nexSerial.print(",0");
  sendTerminator();
  nexSerial.print("cle ");
  nexSerial.print(COOLING_WAVE_COMPONENT_ID);
  nexSerial.print(",1");
  sendTerminator();
}

void clearFermentationWaveformChannels() {
  clearCoolingWaveformChannels();
}
