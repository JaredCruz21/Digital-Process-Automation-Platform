# Digital Process Automation Platform

Hands-on automation project for a Teensy 4.1 process controller that models, controls, and logs a small-batch brewing workflow.

Read the [project overview](docs/project-overview.md) for the complete goal, current progress, and development roadmap.
See the [modeling software evaluation](docs/modeling-software-evaluation.md) for the recommended simulation, data, and visualization tools.
Record physical equipment, process assumptions, HMI behavior, and model requirements in the [system specifications](docs/system-specifications.md).
Resume development from the ordered [project checklist](docs/next-steps.md).

## What This Project Shows

- Embedded process automation with a Teensy 4.1, Nextion HMI, SD logging, RTD temperature sensing, and hydrostatic level sensing.
- Digital modeling of a physical process: fill, mash, boil, cooling, fermentation, and calibration.
- Control logic for heating, temperature hold, boil power, volume capture, sensor calibration, and state transitions.
- Structured CSV logs designed for later analysis, model tuning, anomaly detection, and AI-assisted process prediction.

## Digital Process Demo

Open the browser demo at:

```text
portfolio/index.html
```

The demo is an early digital twin interface. It lets a user adjust batch parameters, run a simulated brew day, inspect the state machine, and see how the physical firmware maps into measurable process data.

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

## Data And Modeling

The first Python data foundation and heat-to-strike model are documented in [`modeling/README.md`](modeling/README.md). The tools parse the controller's mixed metadata, sample, and HMI event records without modifying the source run files.
