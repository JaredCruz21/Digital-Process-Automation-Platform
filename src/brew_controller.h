#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>
#include <Adafruit_MAX31865.h>
#include "Nextion.h"
#include <math.h>

#define nexSerial Serial1

inline constexpr float RREF = 430.0f;
inline constexpr float RNOMINAL = 100.0f;
inline constexpr uint8_t HEATER_RELAY_PIN = 4;
inline constexpr uint8_t LEVEL_PRESSURE_SENSOR_PIN = 14;

inline constexpr int SD_CHIP_SELECT = BUILTIN_SDCARD;

inline constexpr int32_t DECIMAL_SCALE = 100;
inline constexpr int32_t PROCESS_VOL_DISPLAY_SCALE = 100;
inline constexpr int32_t LEVEL_VOLUME_XFLOAT_SCALE = 1000;
inline constexpr int32_t SG_DISPLAY_SCALE = 1000;

inline constexpr int TEMP_WAVE_GRID_DH_PX = 10;
inline constexpr int TEMP_WAVE_BOTTOM_TICK = 0;
inline constexpr int TEMP_WAVE_LOWER_MID_TICK = 5;
inline constexpr int TEMP_WAVE_MID_TICK = 11;
inline constexpr int TEMP_WAVE_UPPER_MID_TICK = 16;
inline constexpr int TEMP_WAVE_TOP_TICK = 21;
inline constexpr int TEMP_WAVE_PLOT_MAX = TEMP_WAVE_TOP_TICK * TEMP_WAVE_GRID_DH_PX;

inline constexpr uint8_t MASH_WAVE_COMPONENT_ID = 12;
inline constexpr uint8_t MASH_WAVE_CHANNEL_LIVE = 0;
inline constexpr uint8_t MASH_WAVE_CHANNEL_SP = 1;
inline constexpr float MASH_WAVE_HEAT_HALF_SPAN_C = 10.0f;
inline constexpr float MASH_WAVE_HOLD_HALF_SPAN_C = 2.0f;
inline constexpr int MASH_WAVE_PLOT_MAX = TEMP_WAVE_PLOT_MAX;

inline constexpr float STRIKE_OFFSET_C = 2.0f;
inline constexpr float TEMP_HYSTERESIS_C = 0.25f;
inline constexpr unsigned long MASH_HOLD_CONTROL_WINDOW_MS = 30000;
inline constexpr unsigned long MASH_HOLD_MIN_SWITCH_MS = 3000;
inline constexpr float MASH_HOLD_BASE_DUTY_PCT = 35.0f;
inline constexpr float MASH_HOLD_GAIN_PCT_PER_C = 35.0f;
inline constexpr float MASH_HOLD_MAX_DUTY_PCT = 80.0f;
inline constexpr float MASH_HOLD_FORCE_ON_BELOW_C = 1.25f;
inline constexpr float MASH_HOLD_FORCE_OFF_ABOVE_C = 0.25f;
inline constexpr unsigned long MASH_CAPTURE_STABILIZE_TIME_MS = 10000;

inline constexpr uint8_t LEVEL_ADC_BITS = 12;
inline constexpr int LEVEL_ADC_MAX_COUNT = (1 << LEVEL_ADC_BITS) - 1;
inline constexpr float LEVEL_ADC_REFERENCE_V = 3.3f;
inline constexpr float MIN_VALID_LEVEL_SENSOR_V = 0.0f;
inline constexpr float MAX_VALID_LEVEL_SENSOR_V = 3.3f;
inline constexpr float MAX_VALID_LEVEL_SENSOR_JUMP_V = 0.35f;
inline constexpr unsigned long FILL_STABILIZE_TIME_MS = 10000;

inline constexpr float BOIL_START_TEMP_C = 100.0f;
inline constexpr float BOIL_HEAT_CYCLE_START_TEMP_C = 98.0f;
inline constexpr unsigned long BOIL_DUTY_CYCLE_MS = 5000;
inline constexpr float BOIL_CTRL_STEP_PCT = 2.5f;
inline constexpr float BOIL_RATE_ALPHA = 0.20f;
inline constexpr uint8_t BOIL_WAVE_COMPONENT_ID = 23;
inline constexpr uint8_t BOIL_WAVE_CHANNEL_LIVE = 0;
inline constexpr uint8_t BOIL_WAVE_CHANNEL_REF = 1;
inline constexpr int BOIL_WAVE_PLOT_MAX = TEMP_WAVE_PLOT_MAX;
inline constexpr uint8_t COOLING_WAVE_COMPONENT_ID = 19;
inline constexpr uint8_t COOLING_WAVE_CHANNEL_LIVE = 0;
inline constexpr uint8_t COOLING_WAVE_CHANNEL_TARGET = 1;
inline constexpr int COOLING_WAVE_PLOT_MAX = TEMP_WAVE_PLOT_MAX;
inline constexpr uint8_t FERMENTATION_WAVE_CHANNEL_TEMP = 0;
inline constexpr float FERMENTATION_WAVE_HALF_SPAN_C = 2.0f;
inline constexpr float FERMENTATION_WAVE_MIN_SPAN_C = 1.0f;
inline constexpr float COOLING_START_TEMP_C = 100.0f;
inline constexpr float COOLING_TARGET_TEMP_C = 25.0f;
inline constexpr float COOLING_MODEL_AMBIENT_TEMP_C = 23.0f;
inline constexpr float COOLING_MIN_MODEL_DELTA_C = 0.25f;

enum HMIPage : uint8_t {
  PAGE_MAIN_MENU = 0,
  PAGE_PROCESS_SETUP = 1,
  PAGE_FILL = 2,
  PAGE_MASH = 3,
  PAGE_BOIL = 4,
  PAGE_FERMENTATION = 5,
  PAGE_MANUAL = 6,
  PAGE_LOGS = 7,
  PAGE_TEMP_CAL = 8,
  PAGE_LEVEL_CAL = 9,
  PAGE_PUMP_CAL = 10,
  PAGE_CAL = 11,
  PAGE_PAUSE = 12,
  PAGE_ALARM = 13,
  PAGE_KEYBDB = 14,
  PAGE_KEYBDA = 15
};

enum MashState : uint8_t {
  MASH_IDLE = 0,
  MASH_HEATING_STRIKE,
  MASH_WAIT_FOR_BAG,
  MASH_HOLD,
  MASH_WAIT_REMOVE_BAG,
  MASH_CAPTURE_VOLUME,
  MASH_COMPLETE
};

enum BoilState : uint8_t {
  BOIL_IDLE = 0,
  BOIL_HEATING,
  BOIL_READY_CONFIRM,
  BOIL_ACTIVE,
  BOIL_STOPPED,
  BOIL_COOLING_ACTIVE,
  BOIL_COOLING_READY_TO_LOAD,
  BOIL_COMPLETE
};

enum VolumeCaptureSource : uint8_t {
  VOLUME_CAPTURE_NONE = 0,
  VOLUME_CAPTURE_TOF,
  VOLUME_CAPTURE_MANUAL
};

enum CoolingState : uint8_t {
  COOLING_IDLE = 0,
  COOLING_ACTIVE,
  COOLING_READY_TO_LOAD,
  COOLING_COMPLETE
};

enum FermentationState : uint8_t {
  FERMENTATION_NEEDS_FILE = 0,
  FERMENTATION_FILE_READY,
  FERMENTATION_LOGGING,
  FERMENTATION_PITCHED,
  FERMENTATION_STOPPED
};

enum TemperatureProbe : uint8_t {
  TEMP_PROBE_PROCESS = 0,
  TEMP_PROBE_FERMENTATION
};

extern Adafruit_MAX31865 thermo;

extern uint8_t currentPage;

extern bool sdReady;
extern String currentLogFileName;
extern bool runLogActive;

extern String runName;
extern uint32_t mashDurationMin;
extern int32_t mashTempRaw;
extern float mashTempC;
extern uint32_t boilDurationMin;

extern float T1_val;
extern float R1_val;
extern float T2_val;
extern float R2_val;
extern float processT1_val;
extern float processR1_val;
extern float processT2_val;
extern float processR2_val;
extern float fermentationT1_val;
extern float fermentationR1_val;
extern float fermentationT2_val;
extern float fermentationR2_val;
extern TemperatureProbe activeTempCalProbe;
extern String tempCalStatusMessage;

extern float levelVol1;
extern float levelVol2;
extern float levelDis1;
extern float levelDis2;
extern float levelSlope;
extern float levelIntercept;
extern bool levelCalValid;
extern bool levelCalLowerPointCaptured;
extern float rawDistanceMM;
extern float filtDistanceMM;
extern float liveVolume;

extern float filteredDistanceMM;
extern bool distanceInitialized;

extern float lockedFillVolumeGal;
extern bool fillVolumeLocked;
extern bool fillStabilizing;
extern unsigned long fillStabilizeStartMs;
extern float fillStabilizeSum;
extern uint16_t fillStabilizeCount;
extern VolumeCaptureSource fillCaptureSource;

extern MashState mashState;
extern unsigned long mashStartMs;
extern bool mashStarted;
extern bool mashCaptureStabilizing;
extern unsigned long mashCaptureStartMs;
extern float mashCaptureSum;
extern uint16_t mashCaptureCount;
extern float lockedBoilStartVolumeGal;
extern bool boilStartVolumeLocked;
extern VolumeCaptureSource mashCaptureSource;
extern float lastLoggedSG;
extern bool hasLoggedSG;
extern float liveTempC;
extern bool heaterOn;
extern bool pumpOn;

extern BoilState boilState;
extern float boilWaveStartTempC;
extern float boilRateFilteredCPerMin;
extern float boilEstimateStartTempC;
extern bool boilStarted;
extern unsigned long boilStartMs;
extern unsigned long boilActiveStartMs;
extern unsigned long lastBoilEstimateMs;
extern float lastBoilEstimateTempC;
extern float boilControlPct;
extern float lockedBoilEndVolumeGal;
extern bool boilEndVolumeLocked;
extern VolumeCaptureSource boilCaptureSource;
extern CoolingState coolingState;
extern bool coolingStarted;
extern unsigned long coolingStartMs;
extern unsigned long coolingEndMs;
extern unsigned long lastCoolingEstimateMs;
extern float lastCoolingEstimateTempC;
extern float coolingRateConstantPerSec;
extern float coolingEstimateStartTempC;
extern float processAmbientTempC;
extern float lockedCoolingVolumeGal;
extern bool coolingVolumeLocked;
extern VolumeCaptureSource coolingCaptureSource;
extern FermentationState fermentationState;
extern String fermentationLogFileName;
extern bool fermentationLogActive;
extern String fermentationStatusMessage;
extern String fermentationSGStatusMessage;
extern bool fermentationStarted;
extern bool fermentationYeastPitched;
extern unsigned long fermentationStartMs;
extern unsigned long fermentationPitchMs;
extern unsigned long fermentationStopMs;
extern float fermentationMinTempC;
extern float fermentationMaxTempC;
extern float fermentationTempSumC;
extern uint32_t fermentationTempCount;
extern unsigned long fermentationSixHourStartMs;
extern float fermentationSixHourTempSumC;
extern uint32_t fermentationSixHourTempCount;
extern float lastFermentationSG;
extern bool hasFermentationSG;
extern unsigned long lastFermentationStatsUpdateMs;
extern float fermentationWaveMinC;
extern float fermentationWaveMaxC;
extern bool fermentationWaveRangeInitialized;

extern NexButton btnToCalMenu;
extern NexButton btnToProcessSetup;
extern NexButton btnToManual;
extern NexButton btnToLogs;
extern NexButton btnSetupToMenu;
extern NexButton btnLoadSetup;
extern NexButton btnSetupToFill;
extern NexText txtFilename;
extern NexNumber numMashDuration;
extern NexNumber numMashTemp;
extern NexNumber numBoilDuration;
extern NexNumber numLoadMashDur;
extern NexNumber numLoadMashTemp;
extern NexNumber numLoadBoilDur;
extern NexText txtSDcard;
extern NexButton btnFillToSetup;
extern NexButton btnFillToMash;
extern NexButton btnFillDone;
extern NexButton btnFillManualLoad;
extern NexNumber numFillTemp;
extern NexText txtFillAlarm;
extern NexText txtFillHXStatus;
extern NexText txtFillPumpStatus;
extern NexNumber numFillVol;
extern NexText txtFillResult;
extern NexButton btnMashToFill;
extern NexButton btnMashToBoil;
extern NexNumber numMashTempLive;
extern NexText txtMashAlarm;
extern NexText txtMashHXStatus;
extern NexText txtMashPumpStatus;
extern NexNumber numMashVol;
extern NexButton btnMashAction;
extern NexText txtMashTimeRemain;
extern NexText txtMashState;
extern NexNumber numMashSG;
extern NexButton btnMashSGLoad;
extern NexText txtMashSGStatus;
extern NexNumber numMashFillVol;
extern NexNumber numMashVolLive;
extern NexWaveform waveMashTemp;
extern NexNumber numMashTempSP;
extern NexText txtWaveTop;
extern NexText txtWaveMid;
extern NexText txtWaveUpperMid;
extern NexText txtWaveLowerMid;
extern NexText txtWaveBottom;
extern NexButton btnBoilToMash;
extern NexButton btnBoilToCooling;
extern NexNumber numBoilTemp;
extern NexText txtBoilAlarm;
extern NexText txtBoilHXStatus;
extern NexText txtBoilPumpStatus;
extern NexNumber numBoilVol;
extern NexNumber numBoilFillVol;
extern NexText txtBoilStatus;
extern NexText txtBoilTime;
extern NexText txtBoilPhase;
extern NexWaveform waveBoilTemp;
extern NexText txtBoilTop;
extern NexText txtBoilUpperMid;
extern NexText txtBoilMid;
extern NexText txtBoilLowerMid;
extern NexText txtBoilBottom;
extern NexButton btnBoilMain;
extern NexButton btnBoilPlus;
extern NexButton btnBoilMinus;
extern NexText txtBoilCtrlPct;
extern NexText txtBoilUpdates;
extern NexText txtFermentationFilename;
extern NexNumber numFermentationTemp;
extern NexNumber numFermentationDayMinTemp;
extern NexNumber numFermentationDayMaxTemp;
extern NexNumber numFermentationSixHrAvgTemp;
extern NexNumber numFermentationDayAvgTemp;
extern NexText txtFermentationTimeElapsed;
extern NexButton btnFermentationMain;
extern NexButton btnFermentationSGLoad;
extern NexText txtFermentationSGStatus;
extern NexText txtFermentationStatus;
extern NexButton btnFermentationHome;
extern NexNumber numFermentationSG;
extern NexWaveform waveFermentationTemp;
extern NexText txtFermentationTop;
extern NexText txtFermentationUpperMid;
extern NexText txtFermentationMid;
extern NexText txtFermentationLowerMid;
extern NexText txtFermentationBottom;
extern NexButton btnRackToCooling;
extern NexButton btnRackToLogs;
extern NexButton btnManualToMenu;
extern NexButton btnLogsToMenu;
extern NexButton btnLogsToFermentation;
extern NexText txtLogsFileList;
extern NexButton btnTempCalToMenu;
extern NexButton btnFirstCal;
extern NexButton btnSecondCal;
extern NexButton btnLoadCal;
extern NexText txtTempCalProbe;
extern NexButton btnSwitchTempCalProbe;
extern NexNumber numCR;
extern NexNumber numTempCal;
extern NexText txtTempCalFault;
extern NexNumber numR1;
extern NexNumber numT1;
extern NexNumber numR2;
extern NexNumber numT2;
extern NexButton btnLevelCalToMenu;
extern NexButton btnLevelLoadCal;
extern NexNumber numLevelVol1;
extern NexNumber numLevelVol2;
extern NexNumber numForceVoltage1;
extern NexNumber numForceVoltage2;
extern NexButton btnLevelForceCal;
extern NexNumber numRawDistance;
extern NexNumber numLiveVolume;
extern NexText txtLevelStatus;
extern NexButton btnPumpCalToMenu;
extern NexButton btnCalToTempCal;
extern NexButton btnCalToLevelCal;
extern NexButton btnCalToPumpCal;
extern NexTouch *nex_listen_list[];

void sendTerminator();
void sendText(const char *objName, const String &txt);
void sendNumber(const char *objName, int32_t value);
void setCurrentPage(uint8_t page);
void handleNextionPageRefreshEvents();
int32_t processVolToNextion(float volumeGal);
void clearMashWaveformChannels();
void clearBoilWaveformChannels();
void clearCoolingWaveformChannels();
void clearFermentationWaveformChannels();

bool readTextValue(NexText &obj, String &out);
bool readNumberWithRetry(NexNumber &obj, uint32_t &out, uint8_t attempts = 3);
bool readComponentNumberValue(const char *objName, uint32_t &out, uint8_t attempts = 3);
bool readVolumeInputGallons(const char *objName, float &out, uint8_t attempts = 3);

void saveCalibrationToEEPROM();
void loadCalibrationFromEEPROM();
void saveLevelCalibrationToEEPROM();
void loadLevelCalibrationFromEEPROM();

float readRTDResistanceOhms();
float calculateCalibratedTempC(float measuredResistance);
float calculateCalibratedTempCForProbe(float measuredResistance, TemperatureProbe probe);
float getLiveTempC();
float getLiveResistanceOhms();

bool initSDCard();
void updateSDCardStatus(const String &status);
String sanitizeFileBaseName(String input);
String makeUniqueCsvFileName(const String &requestedBase, bool &hadToRename);
bool createRunFileWithSetup();
bool finishRunLog(const String &reason);
bool appendTempCalibrationLog();
bool appendRunLogLine(const String &page, const String &state, float tempC, float setpointC,
                      float displayVol, float liveVol, float liveVoltage,
                      bool heater, bool pump,
                      long timeRemainingSec, float sgValue, float resistanceOhms,
                      float controlAggressionPct);
bool appendEventLog(const String &eventName, const String &detail);
bool createFermentationLogFile(const String &requestedName, bool &hadToRename);
bool appendFermentationLogLine(const String &state, float tempC, float minTempC,
                               float maxTempC, float sixHrAvgTempC, float avgTempC,
                               unsigned long elapsedSec, float sgValue,
                               float resistanceOhms);
bool appendFermentationEventLog(const String &eventName, const String &detail);
void updateLogsFileList();

void updateProcessSetupLoadedObjects();
const char *temperatureProbeName(TemperatureProbe probe);
void applyActiveTempProbeCalibration();
void storeActiveTempProbeCalibration();
void updateTempCalProbeText();
void setTempCalStatusText(const String &status);
void updateTempCalStoredValuesOnHMI();
void updateTempCalStatusText();
void updateTempCalLiveDisplay();
void updateTempCalPageOnEnter();

void updateLevelStatusText(const String &status);
void updateLevelCalibrationStatus();
void updateLevelCalStoredValuesOnHMI();
bool computeLevelCalibration();
float calculateLiveVolumeFromDistance(float distanceMM);
bool isLevelVoltageInReliableRange(float voltageV);
bool isLiveLevelVolumeReliable();
float readHydrostaticPressureSensorVoltage();
int32_t levelVolumeToXFloat(float volumeGal);
void updateLevelLoadCalButtonText();
float filterDistance(float newReadingMM);
void resetLevelLiveState();
void updateLevelLiveDisplay();
float getLiveVolumeFromTOF();

void updateFillResultText(const String &txt);
void updateFillStatusObjects();
void updateFillLiveDisplay();
void startFillStabilization();
void processFillStabilization();

String formatSignedTime(long sec);
float getMashDisplayVolume();
float getMashLiveCaptureVolume();
float getCurrentMashSetpointC();
long getMashTimeRemainingSec();
String mashStateText();
String mashActionButtonText();
void setMashActionButtonText(const String &txt);
void setHeater(bool on);
void controlMashTemperature();
void updateMashStatusObjects();
void updateMashYAxisLabels(float setpointC);
uint8_t scaleMashTempToWave(float tempC, float setpointC);
void resetMashWaveform();
void updateMashWaveformFixedRange();
void updateMashDisplay();
void startMashProcess();
void markMashIn();
void markMashOut();
void startMashVolumeCapture();
void processMashVolumeCapture();
void updateMashStateMachine();
void logMashSGEntry(float sgValue);
void updateMashPageOnEnter();
bool isBoilAllowedToStart();
void stopActiveProcessControls();

String formatElapsedTime(unsigned long elapsedMs);
long getBoilTimeRemainingSec();
String boilStateText();
String boilButtonText();
void updateBoilStatusObjects();
uint8_t scaleBoilTempToWave(float tempC, float yMin, float yMax);
void updateBoilYAxisLabels(float yMin, float yMax);
void resetBoilWaveform();
void updateBoilWaveform();
String formatBoilControlPct();
void updateBoilControlText();
void updateBoilEstimateText();
void updateBoilDisplay();
float getBoilDisplayVolume();
bool captureBoilEndVolume(float volumeGal, VolumeCaptureSource source);
void startBoilProcess();
void enterBoilingState();
void stopBoilProcess();
void startBoilCoolingProcess();
void stopBoilCoolingProcess();
void controlBoilHeater();
void updateBoilStateMachine();
void incrementBoilControlPct();
void decrementBoilControlPct();
void updateBoilPageOnEnter();

String coolingButtonText();
void updateCoolingYAxisLabels();
uint8_t scaleCoolingTempToWave(float tempC);
void resetCoolingWaveform();
void updateCoolingWaveform();
void updateCoolingEstimateText();
float getCoolingDisplayVolume();
void updateCoolingDisplay();
void startCoolingProcess();
void endCoolingProcess();
bool captureCoolingVolume(float volumeGal, VolumeCaptureSource source);
void updateCoolingPageOnEnter();

String fermentationStateText();
String fermentationButtonText();
unsigned long getFermentationElapsedMs();
void resetFermentationStats(float initialTempC);
void createFermentationFileFromHMI();
void startFermentationLogging();
void pitchFermentationYeast();
void stopFermentationProcess();
void updateFermentationStats(float tempC);
void updateFermentationDisplay();
void resetFermentationWaveform(float centerTempC);
void updateFermentationWaveform(float tempC);
void logFermentationSGEntry(float sgValue);
void updateFermentationPageOnEnter();

void setupController();
void loopController();
