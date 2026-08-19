# Digital Process Automation Platform: Project Overview

## Project Goal

This is a hands-on learning project built around automating a real physical brewing process. The main goal is to gain practical experience with embedded control, instrumentation, process modeling, data collection, and predictive analysis while developing a complete automation project that can support future job opportunities.

The physical system is a Teensy 4.1-based brewing process controller. It coordinates temperature, liquid volume, heating, pumping, timing, operator actions, calibration, and data logging across the full brewing workflow.

The long-term goal is to build a practical digital twin that can:

- Reflect the current state of the physical process.
- Simulate a complete run using adjustable process inputs.
- Compare expected behavior with real sensor data.
- Predict heating, cooling, and stage completion times.
- Detect unusual process behavior and possible equipment issues.
- Show how the controller and models improve as more runs are recorded.

## Why This Project Exists

The project is a way to learn automation by building the entire system rather than studying each topic separately. It connects software, sensors, equipment, operator interaction, and process data in one working application.

The work develops practical experience in:

- Embedded systems development.
- Process control and state-machine design.
- Sensor calibration and signal conversion.
- Human-machine interface integration.
- Operational data logging.
- Dynamic process simulation.
- System identification and predictive modeling.
- Industrial visualization and data architecture.

## What Has Been Completed

### Physical Process Controller

The firmware currently models and controls the following process stages:

1. Process setup and run creation.
2. Vessel filling and volume capture.
3. Heating to the mash strike temperature.
4. Mash temperature control and timing.
5. Mash-out and pre-boil volume capture.
6. Heating to boil and operator confirmation.
7. Timed boil control.
8. Cooling and post-boil volume capture.
9. Fermentation temperature and specific-gravity tracking.
10. Temperature, level, and pump calibration workflows.

The controller integrates a Teensy 4.1, Nextion HMI, RTD temperature sensing, hydrostatic level sensing, relay-based outputs, EEPROM calibration storage, and SD-card CSV logging.

### Process Data Foundation

The firmware records the information needed for future analysis and model training, including:

- Timestamped temperature readings.
- Calibrated liquid-volume readings.
- Heater and pump states.
- Process stage and event changes.
- Mash and boil settings.
- Cooling behavior.
- Specific-gravity entries.
- Sensor calibration values.

This creates a link between operator decisions, controller actions, physical measurements, and final process outcomes.

### Process Documentation

The current workflow has been documented with process diagrams and stage-by-stage data tables in [`process-flow.md`](process-flow.md). This provides a readable map between the physical workflow, HMI actions, firmware state changes, and captured data.

### Interactive Digital Process Model

An initial browser-based digital process demonstration has been created in the [`portfolio`](../portfolio) directory. It allows a user to:

- Adjust mash temperature, mash duration, boil duration, and fill volume.
- Run a simulated process cycle.
- Watch the vessel and process state change over time.
- Inspect key process metrics.
- View a modeled temperature forecast.
- Explore the firmware state machine.
- Understand how logged data can support future predictive models.

The browser application has a repeatable static-site build and a working hosted preview. Its current simulation is illustrative; connecting it to a validated physical model and real process logs is future work.

### Source Control And Deployment

The complete project is connected to GitHub at:

<https://github.com/JaredCruz21/Digital-Process-Automation-Platform>

The repository contains the embedded firmware, process documentation, interactive simulation, and a repeatable static-site build process.

## Current System Structure

```text
Physical equipment and sensors
            |
            v
Teensy 4.1 control firmware
            |
            v
SD-card process and event logs
            |
            v
Data preparation and model training
            |
            v
Predictions, anomaly detection, and digital twin updates
            |
            v
Browser-based process visualization
```

The firmware, logging foundation, documentation, and initial browser simulation are in place. The next major connection is importing real run data and using it to identify and validate the physical process model.

## Modeling Direction

The recommended tools and alternatives are compared in [`modeling-software-evaluation.md`](modeling-software-evaluation.md). The proposed starting stack is:

- Python for CSV processing, parameter estimation, prediction, and model evaluation.
- OpenModelica for a reusable physics-based process model.
- Ignition Maker Edition later for SCADA, historian, alarm, and industrial visualization experience.
- Node-RED later as an optional live-data bridge between the Teensy and other services.

The modeling work should begin with understandable physical equations and simple statistical baselines. More complex machine-learning models should only be added when enough real runs exist to evaluate whether they improve prediction accuracy.

## Initial Prediction Targets

The first useful modeling targets are:

- Time required to reach the mash strike temperature.
- Time required to reach a stable boil.
- Cooling time to the target fermentation temperature.
- Temperature overshoot and recovery during mash control.
- Expected liquid loss between fill, boil, and cooling stages.
- Detection of sensor drift, weak heating performance, or unusual cooling behavior.

Each prediction should store its inputs, predicted value, actual result, and error. This will make progress measurable across physical runs.

## Next Milestones

1. Collect representative CSV logs from real brewing runs.
2. Define a stable dataset schema for process samples and events.
3. Build a first-principles heating and cooling model in Python.
4. Estimate model parameters from real process data.
5. Validate predictions against complete runs that were not used for fitting.
6. Add a browser workflow for loading and replaying an actual run.
7. Reproduce the physical model in OpenModelica and compare results.
8. Add anomaly flags and model-confidence indicators.
9. Connect live controller data to the digital process visualization.
10. Add an industrial SCADA layer after the core data path is reliable.

## Definition Of Success

The project will be successful when the complete system can:

- Control and log a physical brewing process reliably.
- Explain its sequence through clear state and process diagrams.
- Replay an actual run through the digital visualization.
- Predict important stage outcomes from measured inputs.
- Compare predictions with actual results using defined error metrics.
- Identify abnormal process behavior without hiding the underlying evidence.
- Demonstrate steady improvement through testing and documented experiments.

The finished system is an evolving engineering project that combines embedded control, process modeling, data engineering, predictive analysis, and industrial automation concepts in one practical learning experience.
