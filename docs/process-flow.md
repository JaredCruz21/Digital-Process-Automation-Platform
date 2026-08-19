# Brew Controller Process Flow

This document summarizes the current HMI process flow and the data captured or monitored by each stage.

## Main Brew Flow

```mermaid
flowchart TD
  Menu[Main Menu] --> Setup[Process Setup]
  Setup -->|load| SetupLoaded[Load run name, mash duration, mash temp, boil duration]
  SetupLoaded -->|create run CSV| Fill[Fill]

  Fill -->|filldone| FillLock[Lock fill volume]
  FillLock -->|hydrostatic level sensor| FillTOF[Average live volume for 10 seconds]
  Fill -->|manual load| FillManual[Manual fill volume]
  FillTOF --> Mash[Mash]
  FillManual --> Mash

  Mash -->|actbtn START| Strike[Heat to strike temp]
  Strike -->|strike reached| AddBag[Add bag]
  AddBag -->|actbtn MASH IN| Hold[Mash hold timer]
  Hold -->|timer done, actbtn MASH OUT| RemoveBag[Remove bag]
  RemoveBag -->|actbtn CAPTURE VOL| BoilVol[Lock boil-start volume]
  BoilVol -->|manual fillvol if entered| BoilVolManual[Manual volume]
  BoilVol -->|otherwise hydrostatic sensor| BoilVolSensor[Average live volume for 10 seconds]
  BoilVolManual --> Boil[Boil]
  BoilVolSensor --> Boil

  Boil -->|boilbtn START| HeatBoil[Heating to boil]
  HeatBoil -->|98 C threshold| ConfirmBoil[Confirm visible boil]
  ConfirmBoil -->|boilbtn CONFIRM| ActiveBoil[Boiling timer]
  ActiveBoil -->|boilbtn STOP| BoilStopped[Boil stopped]
  BoilStopped -->|boilbtn START COOLING| Cooling[Cooling active]
  Cooling -->|boilbtn END COOLING| CoolingStopped[Cooling stopped, load volume]
  CoolingStopped -->|manual fillvol if entered| EndVolManual[Manual boil-end volume]
  CoolingStopped -->|otherwise hydrostatic sensor| EndVolSensor[Live sensor volume]
  EndVolManual --> Logs[Logs]
  EndVolSensor --> Logs
```

## Fermentation Flow

```mermaid
flowchart TD
  Logs[Logs Page] --> Fermentation[Fermentation Page]
  Menu[Main Menu] --> Fermentation
  Fermentation -->|fermBtn CREATE| FermFile[Create fermentation CSV]
  FermFile -->|fermBtn START| TempLogging[Temperature logging]
  TempLogging -->|fermBtn PITCH| Pitched[Yeast pitched, elapsed timer resets]
  Pitched -->|sgload| SGEntry[Log SG entry]
  Pitched -->|fermBtn STOP| FermStopped[Stop fermentation]
```

## Calibration And Support Flow

```mermaid
flowchart TD
  CalMenu[Calibration Menu] --> TempCal[Temperature Calibration]
  CalMenu --> LevelCal[Level Calibration]
  CalMenu --> PumpCal[Pump Calibration]

  LevelCal -->|loadcal LOAD LOW| LowPoint[Capture low volume and live voltage]
  LowPoint -->|loadcal LOAD HIGH| HighPoint[Capture high volume and live voltage]
  HighPoint --> LevelEquation[Compute level equation and save EEPROM]

  LevelCal -->|forceCal| ForceLevel[Use vol1, vol2, forceVoltage1, forceVoltage2]
  ForceLevel --> LevelEquation
```

## Captured And Monitored Data

| Stage | Monitored live data | Captured / saved data | Log behavior |
| --- | --- | --- | --- |
| Process Setup | SD card status | Run name, mash duration, mash temp, boil duration | Creates run CSV with setup/calibration metadata |
| Fill | Live temperature, heater status, pump status, live volume | Locked fill volume from hydrostatic average or manual entry | Event: `FILL_LOCKED` or `FILL_LOCKED_MANUAL` |
| Mash Heating | Live temp, setpoint, heater/pump, live volume, waveform | Strike reached event | Run log lines during Mash; event: `MASH_START`, `STRIKE_REACHED` |
| Mash Hold | Live temp, mash setpoint, timer remaining, waveform | SG entries if loaded | Run log lines; event: `MASH_IN`, `SG_ENTRY` |
| Mash Out / Capture | Live volume, locked boil-start volume | Manual `fillvol` or averaged hydrostatic volume | Event: `MASH_OUT`, `BOIL_VOL_CAPTURE_START`, `BOIL_VOL_LOCKED` |
| Boil Heating | Live temp, boil status, ETA/update text, waveform | Boil start state | Run log lines during Boil; event: `BOIL_START`, `BOIL_REACHED`, `BOIL_READY_CONFIRM` |
| Active Boil | Live temp, boil timer, control percent, waveform | Boil stop event | Run log lines; event: `BOIL_STOPPED` |
| Cooling On Boil Page | Live temp, cooling estimate, waveform | Cooling start/end, boil-end volume | Event: `COOLING_START`, `COOLING_END`, `BOIL_END_VOL_MANUAL` or `BOIL_END_VOL_TOF` |
| Fermentation | Live temp, min/max temp, 6-hour avg, day avg, elapsed time, SG status, waveform | Fermentation file, yeast pitch, SG entries | Fermentation log every 30 seconds while running; event log for file/start/pitch/stop/SG |
| Level Calibration | Raw voltage, live volume | `vol1`, `vol2`, low/high voltages, slope/intercept | EEPROM save; status shown on LevelCal `status` |
| Temperature Calibration | RTD resistance, live temp | Probe R/T calibration points | EEPROM save and optional calibration log |

## Key Sensors And Derived Values

| Signal | Source | Used for |
| --- | --- | --- |
| Process temperature | MAX31865 RTD path | Fill, Mash, Boil, Cooling estimates, Fermentation logging |
| Level voltage | Hydrostatic pressure sensor analog input | Live volume and fill/boil volume locks |
| Level volume | `levelSlope * voltage + levelIntercept` | Process volume display and automatic volume capture |
| Specific gravity | Manual HMI xfloat input | Mash SG event entries and fermentation SG entries |
| Heater state | Relay output state | Status display and run log |
| Pump state | Pump state variable | Status display and run log |

