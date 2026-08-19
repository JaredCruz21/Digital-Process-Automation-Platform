# Project Resume Checklist

Last updated: 2026-08-18

## Resume Here

The review branch is `feature/versioned-logging-mash-foundation`. It is intended to merge into `main` after the firmware and CSV validation steps below pass.

Completed in the current work session:

- [x] Document the rounded, self-draining vessel bottom and central outlet.
- [x] Record the MP-15RM-110 pump nameplate specifications.
- [x] Define `process-v3` and `fermentation-v2` CSV schema versions.
- [x] Embed the Git revision, dirty-build indicator, and build timestamp in new logs.
- [x] Log the mash-controller version and active controller constants.
- [x] Change mash sampling from 5 seconds to 1 second.
- [x] Update the Python parser and heating model for `process-v3`.
- [x] Build the Teensy 4.1 firmware successfully.
- [x] Pass all five Python modeling tests.

The new firmware has **not** been uploaded to the Teensy yet.

## First Tasks Next Session

- [ ] Review the feature-branch diff against `main`.
- [ ] Upload the new firmware to the Teensy 4.1.
- [ ] Create a short test run and inspect the SD-card CSV.
- [ ] Confirm that `firmware_commit` no longer ends in `-dirty` after building from a clean commit.
- [ ] Merge `feature/versioned-logging-mash-foundation` into `main` after validation.
- [ ] Push the updated `main` branch to GitHub.

## CSV Validation Run

Use a short water-only mash test before the next full run.

- [ ] Confirm `schema_version,process-v3` appears in `[SETUP]`.
- [ ] Confirm `firmware_commit` and `firmware_built_at` are populated.
- [ ] Confirm `mash_controller_version` is populated.
- [ ] Confirm all mash-controller constants are recorded.
- [ ] Confirm mash rows are approximately one second apart.
- [ ] Confirm `MASH_START`, `STRIKE_REACHED`, `MASH_IN`, and `MASH_OUT` events are present.
- [ ] Check for timestamp gaps or visible HMI slowdown from SD writes.
- [ ] Copy the resulting CSV into `Z:/BeerProject/Beer Runs`.
- [ ] Parse the file with the Python modeling tools and verify it is classified as `process-v3`.

## Mash-Control Baseline

Eight existing files contain mash-hold data, but their firmware versions and water/grain status are not fully known.

- [ ] Classify each mash-hold file as water test, grain mash, or unknown.
- [ ] Build a baseline report for MAE, overshoot, undershoot, settling time, heater duty, and switching count.
- [ ] Analyze mash-in recovery separately from steady mash hold.
- [ ] Exclude runs with incompatible calibration or controller versions from pooled tuning.

Files to classify:

- [ ] `fullwaterrun1_01.csv`
- [ ] `newmashctrl.csv`
- [ ] `waterrun1.csv`
- [ ] `waterrun_01.csv`
- [ ] `waterRUNreset_01.csv`
- [ ] `wayeract.csv`
- [ ] `wayeractnoi.csv`
- [ ] `wayeractnoiact.csv`

## Logging Improvements For Controller Tuning

- [ ] Add `timestamp_ms` or millisecond heater-transition events.
- [ ] Add `commanded_heater_duty_pct` to every mash sample.
- [ ] Add `control_error_c`.
- [ ] Add proportional, integral, and feed-forward output terms.
- [ ] Add control-window ID and elapsed window time.
- [ ] Distinguish physical pump assumptions from software pump commands.
- [ ] Add sensor-valid and fault-code fields.
- [ ] Move samples and events toward a fixed-width record schema.

## Candidate Mash Controller

The current controller is a biased, time-proportional P controller. The next candidate is a feed-forward biased PI controller without derivative action.

- [ ] Latch the duty command at the beginning of each 30-second window.
- [ ] Keep mash-in recovery separate from steady mash hold.
- [ ] Add integral anti-windup and reset/freeze rules.
- [ ] Create a Python shadow-controller simulation before changing hardware behavior.
- [ ] Start simulation near 30-35% base duty, 20%/C proportional gain, and 1%/(C min) integral gain.
- [ ] Retain the 30-second window, 3-second minimum pulse, and 80% normal limit until switching hardware is confirmed.
- [ ] Sweep candidate gains against the identified thermal model.
- [ ] Validate the selected controller on water before a grain mash.

## Physical Measurements

- [ ] Measure installed pump flow at the normal return height.
- [ ] Record exact starting water volume for each controlled run.
- [ ] Record ambient temperature.
- [ ] Compare the inline RTD with the Thermapen at recorded times.
- [ ] Record heater voltage and current under load when appropriate measurement equipment is available.
- [ ] Estimate vessel thermal mass.
- [ ] Measure external plumbing hold-up separately from vessel residual volume.

## Useful Commands

Build the firmware:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Run the modeling tests:

```powershell
python -m unittest discover -s modeling/tests -v
```

Run the current data analysis:

```powershell
python -m modeling.analyze_runs --input "Z:/BeerProject/Beer Runs"
```
