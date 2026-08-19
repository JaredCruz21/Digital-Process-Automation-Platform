# Digital Process Automation Platform

Recruiter-facing portfolio project for a Teensy 4.1 process controller that models, controls, and logs a small-batch brewing workflow.

## What This Project Shows

- Embedded process automation with a Teensy 4.1, Nextion HMI, SD logging, RTD temperature sensing, and hydrostatic level sensing.
- Digital modeling of a physical process: fill, mash, boil, cooling, fermentation, and calibration.
- Control logic for heating, temperature hold, boil power, volume capture, sensor calibration, and state transitions.
- Structured CSV logs designed for later analysis, model tuning, anomaly detection, and AI-assisted process prediction.

## Portfolio Demo

Open the browser demo at:

```text
portfolio/index.html
```

The demo is a digital twin-style resume artifact. It lets a recruiter adjust batch parameters, run a simulated brew day, inspect the state machine, and see how the physical firmware maps into measurable process data.

## Firmware Highlights

- `src/brew_controller_process.cpp`: process state machines and control behavior.
- `src/brew_controller_storage.cpp`: calibration, CSV logging, and sensor conversion.
- `src/brew_controller_state.cpp`: HMI objects and runtime state.
- `docs/process-flow.md`: Mermaid process maps and data capture summary.

## AI Modeling Roadmap

The firmware already captures the data needed for AI-assisted modeling:

- Temperature response curves during heating, mash hold, boil, and cooling.
- Volume readings from calibrated hydrostatic pressure.
- Heater and pump states.
- Specific gravity entries.
- Calibration metadata and event markers.

Next steps are to export SD-card CSV logs, train simple forecasting models, and compare predicted stage completion times against real process outcomes.
