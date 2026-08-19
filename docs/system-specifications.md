# Brewing Automation System And Modeling Specifications

## Purpose

This is the living specification for the physical system, controller behavior, HMI workflow, logged data, and digital models. Complete or revise fields as the equipment and process become better understood.

Use these status values:

- `CONFIRMED`: measured, observed, or verified from hardware or code.
- `ASSUMED`: temporarily treated as true for modeling.
- `TBD`: not yet known or measured.
- `PLANNED`: intended future behavior that is not implemented yet.

When possible, include units, measurement method, date, and uncertainty. Do not replace a measured value with a nominal equipment rating without noting the difference.

## Start Here: Highest-Value Inputs

These values have the greatest effect on the first heating and mash models.

| Specification | Value | Unit | Status | Notes / measurement method |
| --- | --- | --- | --- | --- |
| Heater rated power | 2000 | W | CONFIRMED | Rating printed on element |
| Heater measured voltage under load | TBD | V | TBD | Currently Unknown |
| Heater measured current under load | TBD | A | TBD | Currently unknown |
| Vessel empty mass | TBD | kg | TBD | Currently Unknown |
| Vessel material | 316/316L stainless steel |  | CONFIRMED | Vessel marking: MAWP 3.1 bar at 177 C; Alloy Products Corp. |
| Typical starting water volume | 2.5 | gal | TBD | Around 3 gallons for a half batch; record the actual recipe-specific starting volume. |
| Temperature-probe location | Inline, upstream of recirculation pump |  | CONFIRMED | Located between the bottom heater/manifold assembly and pump inlet. |
| Recirculation return location | Top of vessel |  | CONFIRMED | Return is approximately 1.5 m above the pump. |
| Pump approximate flow during heating | TBD | L/min | TBD | Nameplate rated capacity is 8/12 L/min at 50/60 Hz; actual installed flow must be measured. |
| Ambient temperature during runs | TBD | C | TBD | Expected to be slightly above room temperature; measure each run. |
| Grain mass for a typical mash | TBD | kg | TBD | Varies by recipe and batch size; typical mash liquid volume is about 3 gal before grain. |
| Grain temperature at mash-in | TBD | C | TBD | Depends on recipe. |
| Vessel insulation and lid state | Uninsulated; lid on for heating and mash |  | CONFIRMED | Lid is removed for boil. |

## Specification Control

| Field | Value | Status |
| --- | --- | --- |
| Project name | Digital Process Automation Platform | CONFIRMED |
| Specification owner | Jared Cruz | CONFIRMED |
| Specification version | 0.3 | CONFIRMED |
| Last physical-system review | 2026-08-18 | CONFIRMED |
| Physical evidence reviewed | Vessel exterior/interior and pump nameplate photos | CONFIRMED |
| Firmware version or Git commit used for next run | Embedded automatically at build | CONFIRMED |
| Log schema version used for next run | `process-v3` | CONFIRMED |

## System Architecture

| Component | Specification | Status | Notes |
| --- | --- | --- | --- |
| Main controller | Teensy 4.1 | CONFIRMED | Arduino framework through PlatformIO |
| Operator interface | Nextion HMI | CONFIRMED | Serial connection |
| Process temperature interface | MAX31865 | CONFIRMED | `RREF = 430 ohm`, `RNOMINAL = 100 ohm` |
| Process temperature sensor | PT100 RTD | ASSUMED | Confirm probe class, wiring, and construction |
| Level measurement | Analog hydrostatic pressure sensor | CONFIRMED | 12-bit ADC, 3.3 V reference |
| Run storage | Teensy built-in SD card | CONFIRMED | Sectioned CSV logs |
| Heater command | Relay output on Teensy pin 4 | CONFIRMED | Specify relay or SSR hardware below |
| Pump command | Manual control | CONFIRMED | No feedback or control pin currently routed |

## Physical Plant

### Vessel

| Specification | Value | Unit | Status | Notes |
| --- | --- | --- | --- | --- |
| Manufacturer and model | TBD |  | TBD |  |
| Nominal capacity | Approx. 36 | gal | ASSUMED | Geometric estimate confirmed as roughly correct; not a measured fill capacity. |
| Safe working volume | 2 | gal | ASSUMED | Current minimum operating volume; verify heater coverage at this volume. |
| Empty mass | TBD | kg | TBD | Vessel is too large for the currently available scale. |
| Material | 316/316L stainless steel |  | CONFIRMED | Vessel marking. |
| Internal diameter | 17.875 | inch | ASSUMED | Approximate measurement: 17 7/8 inches. |
| Internal height | 33.5 | inch | ASSUMED | Approximate total height from bottom port to top. |
| Wall thickness | 0.125 | inch | TBD | not 100 percent sure |
| Bottom geometry | Rounded/dished toward center outlet |  | CONFIRMED | Observed in vessel photos; not a flat-bottom cylinder. |
| Drain port | Single central low-point sanitary port |  | CONFIRMED | Rounded bottom directs free-draining liquid to this outlet. |
| Insulation material | None |  | CONFIRMED | Vessel is uninsulated. |
| Insulation thickness | 0 | mm | CONFIRMED |  |
| Lid used during heat-to-strike | Yes | yes/no | CONFIRMED |  |
| Lid used during mash | Yes | yes/no | CONFIRMED |  |
| Lid used during boil | No | yes/no | CONFIRMED | Removed to allow evaporation. |
| Heater location | Bottom outlet manifold |  | CONFIRMED | Dernord 120 V, 2000 W immersion heater. |
| Estimated vessel residual volume | Near zero when free-draining | gal | ASSUMED | Excludes wetting film, solids, and external plumbing hold-up. |

The rounded bottom means a flat-cylinder height-to-volume equation is not valid at low fill levels. Build the level model from measured fill-volume calibration points; model external hose and fitting hold-up separately from vessel residual volume.

### Heater And Electrical Supply

| Specification | Value | Unit | Status | Notes |
| --- | --- | --- | --- | --- |
| Heater type | Immersion |  | CONFIRMED | Mounted in the bottom outlet manifold. |
| Rated power | 2000 | W | CONFIRMED | Nominal equipment rating, not measured input power. |
| Rated voltage | 120 | V AC | CONFIRMED | Nominal equipment rating. |
| Measured voltage under load | TBD | V AC | TBD |  |
| Measured current under load | TBD | A | TBD |  |
| Switching device | TBD |  | TBD | Relay, SSR, contactor |
| Switching polarity | Active high |  | CONFIRMED | Firmware sets relay pin HIGH for ON |
| Maximum permitted duty | 100 | % | TBD | Hardware and safety limit |
| Minimum switching interval | 3 | s | CONFIRMED | Current mash firmware value |
| Dry-fire protection | TBD |  | TBD | Describe sensor or interlock |

### Pump And Recirculation

| Specification | Value | Unit | Status | Notes |
| --- | --- | --- | --- | --- |
| Pump manufacturer | TBD |  | TBD | Manufacturer is not visible on the reviewed nameplate. |
| Pump model | MP-15RM-110 magnetic drive pump |  | CONFIRMED | Nameplate photo. |
| Rated frequency | 50/60 | Hz | CONFIRMED | Slash-separated nameplate values use 50/60 Hz order. |
| Rated voltage | 100/110 | V AC | CONFIRMED | Nameplate values; actual supply voltage is TBD. |
| Rated motor power | 10 | W | CONFIRMED | Nameplate value. |
| Rated current | 150 | mA | CONFIRMED | Nameplate value. |
| Pump rated flow | 8/12 | L/min | CONFIRMED | Nameplate rated capacity at 50/60 Hz. |
| Pump maximum flow | 16/19 | L/min | CONFIRMED | Nameplate maximum capacity at 50/60 Hz. |
| Pump rated head | 1.5 | m | CONFIRMED | Nameplate value. |
| Pump maximum head | 2.7/3.4 | m | CONFIRMED | Nameplate maximum at 50/60 Hz. |
| Rated speed | 2600/3000 | r/min | CONFIRMED | Nameplate values at 50/60 Hz. |
| Protection grade | IP44 |  | CONFIRMED | Nameplate value. |
| Nameplate operating restriction | Do not run dry |  | CONFIRMED | Recorded as manufacturer equipment guidance. |
| Measured operating flow | TBD | L/min | TBD | Include valve position and head |
| Pump control | Manual |  | CONFIRMED | Controller log does not reflect physical state |
| Pump during heat-to-strike | ON |  | ASSUMED | Confirmed operating rule for current models |
| Pump during mash | ON |  | ASSUMED | Confirmed operating rule for current models |
| Pump at boiling | OFF |  | ASSUMED | Confirm exact switch-off condition |
| Pump during cooling | ON |  | TBD |  |
| Pump during transfer | ON |  | TBD | Pump is used to transfer fluid from vessel to fermenation vessel |
| Pickup location | Central bottom drain port |  | CONFIRMED | Single low-point outlet on rounded vessel bottom. |
| Return location | Top of vessel |  | CONFIRMED | Approximately 1.5 m above pump; direction and diffuser remain TBD. |
| Hose inside diameter | 3/8 | inch | CONFIRMED | Flexible food-grade silicone tubing. |
| Total recirculation hose length | 5.5 | ft | TBD |  |
| Estimated loop hold-up volume | TBD | L | TBD | Fluid would not be emptied or recovered.  |

## Instrumentation

### Process Temperature

| Specification | Value | Unit | Status | Notes |
| --- | --- | --- | --- | --- |
| Sensor type | PT100 RTD |  | ASSUMED | MECCANIXITY PT100 RTD Thermocouple Stainless Steel NPT1/4 Temperature Sensor Probe -50 to 500C 6.6ft 5x50mm for inline Mash and MECCANIXITY PT100 RTD Thermocouple Stainless Steel M8 Temperature Sensor Probe -50 to 500C 6.6ft 4x100mm for Fermenation |
| RTD wiring | 3 wire | 2/3/4 wire | TBD |  |
| Accuracy class | TBD |  | TBD | Class A, Class B, other |
| Probe insertion depth | 50 / 100 | mm | TBD | 50 mm for the inline and 100mm inside the fermentor |
| Probe physical location | TBD |  | TBD | Threaded into a NPT1/4 to adaptor to a 3 way TC  the input to the 3way is coming from a 4 way TC that has the heater, bottom of vessel, and hydrostatic pressure sensor. Output of the 3 way connection is the recirc pump |
| Distance from heater | 5 | inch | TBD |  |
| MAX31865 reference resistance | 430 | ohm | CONFIRMED | Teyleten Robot MAX31865 PT100 to PT1000 RTD-to-Digital Converter Board Temperature Thermocouple Sensor Amplifier Module 3.3V/5V for Arduino 3pcs|
| Nominal RTD resistance | 100 | ohm | CONFIRMED | Firmware constant |
| Current calibration point 1 | 100 at 0 | ohm / C | CONFIRMED | Current run metadata |
| Current calibration point 2 | 138.50 at 100 | ohm / C | CONFIRMED | Process probe metadata |
| Controller update interval | 0.25 | s | CONFIRMED | Mash and boil control update |
| Mash log interval | 1 | s | CONFIRMED | Increased resolution for 30 s duty-window analysis. |
| Boil log interval | 5 | s | CONFIRMED |  |
| Fill log interval | 1 | s | CONFIRMED |  |
| Fermentation log interval | 30 | s | CONFIRMED |  |
| Independent reference thermometer | Thermapen ONE by ThermoWorks |  | CONFIRMED | Stated accuracy: +/- 0.3 C. |

### Level And Volume

| Specification | Value | Unit | Status | Notes |
| --- | --- | --- | --- | --- |
| Sensor manufacturer and model | TBD |  | TBD | Generic 50.5 mm sanitary-clamp pressure level transmitter. |
| Sensor output range | 4-20 | mA | CONFIRMED | Nominal sensor output. |
| Pressure range | 10 | kPa | CONFIRMED | Nominal sensor range. |
| Sensor mounting location | Horizontal branch of four-way sanitary fitting below vessel |  | CONFIRMED | Connected near the central bottom outlet. |
| Current-to-voltage interface | TBD |  | TBD | Record shunt resistance or signal conditioner and measured 4 mA/20 mA voltages. |
| ADC resolution | 12 | bit | CONFIRMED |  |
| ADC reference | 3.3 | V | CONFIRMED |  |
| Current low calibration point | 2.29 at 0.74 | gal / V | CONFIRMED | From recent run metadata |
| Current high calibration point | 4.36 at 0.85 | gal / V | CONFIRMED | From recent run metadata |
| Calibration reference method | TBD |  | TBD | Weighed water, marked vessel, other |
| Expected valid volume range | TBD | gal | TBD | unknown |
| Acceptable volume error | TBD | gal | TBD | unknown |

### Additional Measurements

| Measurement | Available now | Desired sensor or method | Priority |
| --- | --- | --- | --- |
| Ambient temperature | No | Separate temperature sensor | High |
| Heater voltage/current | No | Voltage and current measurement | High |
| Pump flow | No | Flow meter or timed-volume test | High |
| Coolant inlet temperature | No | Temperature sensor | Medium |
| Coolant outlet temperature | No | Temperature sensor | Medium |
| Coolant flow | No | Flow meter or fixed test condition | Medium |
| Independent bulk-liquid temperature | No | Second RTD at a different location | High |

## Current Control Settings

These values are copied from the current firmware and should be updated here when control logic changes.

| Setting | Current value | Unit | Status |
| --- | --- | --- | --- |
| Strike offset above mash target | 2.0 | C | CONFIRMED |
| Non-hold temperature hysteresis | 0.25 | C | CONFIRMED |
| Mash hold control window | 30 | s | CONFIRMED |
| Mash hold minimum switch time | 3 | s | CONFIRMED |
| Mash hold base duty | 35 | % | CONFIRMED |
| Mash hold gain | 35 | %/C | CONFIRMED |
| Mash hold maximum normal duty | 80 | % | CONFIRMED |
| Force heater on below setpoint error | 1.25 | C | CONFIRMED |
| Force heater off above setpoint | 0.25 | C | CONFIRMED |
| Boil confirmation threshold | 98 | C | CONFIRMED |
| Boil reference temperature | 100 | C | CONFIRMED |
| Boil duty window | 5 | s | CONFIRMED |
| Boil adjustment step | 2.5 | % | CONFIRMED |
| Cooling target | 25 | C | CONFIRMED |
| Current default cooling ambient | 23 | C | CONFIRMED |

## Process And HMI Workflow

| Stage | Entry action or event | Automatic behavior | Operator action | Exit event | Pump assumption |
| --- | --- | --- | --- | --- | --- |
| Setup | Load process screen | Create run metadata and CSV | Enter run name, mash target/time, boil time | Run file created | OFF |
| Fill | Enter Fill page | Read and stabilize volume | Confirm fill completion or enter manual volume | `FILL_LOCKED` | OFF |
| Heat to strike | `MASH_START` | Heater drives to mash target plus strike offset | Monitor process | `STRIKE_REACHED` | ON |
| Add grain/bag | `STRIKE_REACHED` | Heater waits or follows current firmware logic | Add grain/bag and press Mash In | `MASH_IN` | ON |
| Mash-in recovery | `MASH_IN` | Proposed separate transient model | TBD | Recovery complete | ON |
| Mash hold | Recovery complete | Time-proportional temperature control | Monitor and enter SG if desired | Timer complete / `MASH_OUT` | ON |
| Mash out | `MASH_OUT` | Stop normal mash hold | Remove grain/bag | Volume capture requested | ON until TBD |
| Pre-boil volume | `BOIL_VOL_CAPTURE_START` | Stabilize and lock volume | Enter manual volume if needed | `BOIL_VOL_LOCKED` | TBD |
| Heat to boil | `BOIL_START` | Heater at full output | Monitor process | `BOIL_READY_CONFIRM` | ON until boiling threshold, then OFF |
| Active boil | Operator confirms visible boil | Timed duty control | Adjust boil intensity if needed | `BOIL_STOPPED` | OFF |
| Cooling | `COOLING_START` | Estimate cooling rate and ETA | Start/stop coolant and pump as specified | `COOLING_END` | TBD |
| Post-boil transfer | Cooling complete | PLANNED | Transfer to fermenter and capture volume | Transfer complete | TBD |
| Fermentation | File created / start / pitch | Temperature and SG logging | Pitch, enter SG, stop run | Fermentation stopped | TBD |

## HMI Simulation Requirements

| Requirement | Specification | Status |
| --- | --- | --- |
| Simulation modes | Historical replay and free simulation | PLANNED |
| Setup interaction | Same editable run inputs as physical HMI | PLANNED |
| State transitions | Follow firmware events and operator confirmations | PLANNED |
| Replay timing | Real time plus accelerated time scale | PLANNED |
| Manual-action behavior | Pause until the simulated operator confirms | PLANNED |
| Sensor display | Show measured, simulated, or predicted source clearly | PLANNED |
| Pump display | Apply documented manual assumption until feedback exists | ASSUMED |
| Fault injection | Temperature failure, level failure, SD failure, timeout | PLANNED |
| Prediction display | Value, confidence, observation time, and model version | PLANNED |
| Run comparison | Overlay measured and simulated traces | PLANNED |

### HMI Questions To Decide

| Question | Decision | Status |
| --- | --- | --- |
| Should replay reproduce the original button timing exactly? | TBD | TBD |
| Can a user branch away from historical actions during replay? | TBD | TBD |
| Should simulated time pause at every manual confirmation? | TBD | TBD |
| Which faults should be recoverable from the HMI? | TBD | TBD |
| Which controls are simulation-only and must never command hardware? | TBD | TBD |

## Per-Run Specification Template

Complete this information for controlled experiments and important production runs.

| Field | Value | Unit / allowed values |
| --- | --- | --- |
| Run ID | TBD | Unique name |
| Date and local start time | TBD | ISO date/time |
| Firmware Git commit | TBD | Commit hash |
| Log schema version | `process-v2` |  |
| Run type | TBD | Water test / beer / calibration |
| Purpose of run | TBD |  |
| Starting water volume | TBD | gal |
| Starting water temperature | TBD | C |
| Mash target | TBD | C |
| Strike target | TBD | C |
| Mash duration | TBD | min |
| Grain mass | TBD | kg |
| Grain temperature | TBD | C |
| Ambient temperature | TBD | C |
| Heater voltage under load | TBD | V |
| Heater current under load | TBD | A |
| Pump state during heating | ON | Manual assumption |
| Pump state during mash | ON | Manual assumption |
| Pump switch-off temperature/event | At boiling | Confirm exact threshold |
| Pump valve position or flow | TBD | % or L/min |
| Lid state during heating | TBD | Open / closed / partial |
| Insulation configuration | TBD |  |
| Cooling configuration | TBD |  |
| Known interruptions or manual changes | TBD |  |
| Independent temperature observations | TBD |  |
| Notes | TBD |  |

## Logging Contract

### Currently Logged

- Schema version, firmware Git revision/build state, and build timestamp.
- Mash-controller version, logging interval, and active controller constants.
- Relative timestamp.
- HMI page and process state.
- Temperature and setpoint.
- Display and live volume.
- Raw level-sensor voltage.
- Heater relay state.
- Pump software state, which is not currently physical feedback.
- Time remaining and specific gravity.
- RTD resistance.
- Boil control percentage.
- Setup, calibration, and event records.

### Recommended Additions

| Field | Reason | Priority | Status |
| --- | --- | --- | --- |
| `schema_version` | Parse files without inferring firmware era | High | CONFIRMED |
| `firmware_commit` | Tie behavior to exact source code | High | CONFIRMED |
| `run_id` | Join reset or continuation files | High | PLANNED |
| `record_type` | Keep samples and events in a fixed-width schema | High | PLANNED |
| `sensor_valid` | Reject failed measurements before control/modeling | High | PLANNED |
| `fault_code` | Explain invalid or stopped behavior | High | PLANNED |
| `commanded_heater_duty_pct` | Analyze mash control independently of sampled relay state | High | PLANNED |
| `pump_state_source` | Distinguish assumed, commanded, and measured state | High | PLANNED |
| `ambient_temp_c` | Identify heat loss | High | PLANNED |
| `heater_power_w` | Build a physical energy model | High | PLANNED |
| `grain_mass_kg` | Model mash thermal mass | High | PLANNED |
| `grain_temp_c` | Predict mash-in temperature drop | High | PLANNED |
| `coolant_in_temp_c` | Predict cooling performance | Medium | PLANNED |
| `coolant_flow_l_min` | Predict cooling performance | Medium | PLANNED |

## Model Specifications

### Heating To Strike

| Item | Specification | Unit | Status |
| --- | --- | --- | --- |
| Primary output | Temperature trajectory and ETA to strike |  | CONFIRMED |
| Current model inputs | Temperature, heater state, locked volume, elapsed time |  | CONFIRMED |
| Pump behavior | Assume manual ON |  | ASSUMED |
| Proposed OpenModelica structure | Recirculation/probe node coupled to bulk-vessel node |  | PLANNED |
| Training unit | Complete run segment |  | CONFIRMED |
| Validation method | Hold out complete runs |  | CONFIRMED |
| Initial temperature MAE target | TBD | C | TBD |
| Initial ETA median error target | 2 | min | ASSUMED |
| Maximum acceptable ETA error | 5 | min | ASSUMED |
| Minimum observation time before ETA | TBD | min | TBD |

### Mash Control

| Item | Specification | Unit | Status |
| --- | --- | --- | --- |
| Separate mash-in recovery | Yes |  | PLANNED |
| Controlled variable | Process temperature |  | CONFIRMED |
| Manipulated variable | Heater duty |  | CONFIRMED |
| Disturbances | Grain addition, heat loss, flow/mixing, lid changes |  | ASSUMED |
| Primary metrics | Overshoot, undershoot, settling time, MAE, IAE, heater switching |  | PLANNED |
| Maximum acceptable overshoot | TBD | C | TBD |
| Maximum acceptable undershoot | TBD | C | TBD |
| Acceptable steady-state band | TBD | +/- C | TBD |
| Desired settling time after mash-in | TBD | min | TBD |
| Candidate controllers | Current time-proportional control, PI/PID, model-based feedforward |  | PLANNED |

## Safety And Operating Limits

These specifications must be confirmed before any model-generated recommendation is allowed to affect hardware.

| Limit or interlock | Value / behavior | Status |
| --- | --- | --- |
| Minimum safe liquid volume for heater | TBD | TBD |
| Maximum process temperature | TBD | TBD |
| Maximum continuous heater-on time | TBD | TBD |
| Temperature sensor invalid behavior | Heater OFF and fault state | PLANNED |
| Level sensor invalid behavior | Prevent automatic volume lock | CONFIRMED |
| SD logging failure behavior | TBD | TBD |
| Pump failure behavior during heating/mash | TBD | TBD |
| Emergency stop behavior | TBD | TBD |
| Manual override behavior | TBD | TBD |
| Simulation-to-hardware command boundary | Simulation cannot command hardware by default | PLANNED |

## Open Questions And Decisions

| Priority | Question | Answer / decision | Status |
| --- | --- | --- | --- |
| High | What is the heater's actual electrical power under load? | TBD | TBD |
| High | Is the process RTD in the vessel or recirculation loop? | TBD | TBD |
| High | Where does recirculated liquid return to the vessel? | TBD | TBD |
| High | What pump flow is used during heating and mash? | TBD | TBD |
| High | What physical event defines pump switch-off before boiling? | TBD | TBD |
| High | What are the vessel mass, material, insulation, and lid conditions? | TBD | TBD |
| High | Which existing CSV files are water tests versus actual grain runs? | TBD | TBD |
| Medium | Should the mash timer include mash-in recovery? | TBD | TBD |
| Medium | Is boil control intended to regulate temperature or boil intensity? | TBD | TBD |
| Medium | Is cooling performed through a coil, plate chiller, immersion chiller, or other method? | TBD | TBD |
| Medium | Which HMI actions must remain manual in simulation? | TBD | TBD |

## Change Log

| Version | Date | Change | Author |
| --- | --- | --- | --- |
| 0.1 | TBD | Initial system, HMI, data, and model specification template | Codex / Jared Cruz |
