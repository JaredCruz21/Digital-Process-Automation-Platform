# Brewing Process Modeling

This directory contains the read-only data foundation and first heating models for the automation project.

Physical equipment values, operating assumptions, and model acceptance criteria are maintained in [`docs/system-specifications.md`](../docs/system-specifications.md).

## Current Scope

- Parse sectioned controller CSV files.
- Preserve setup metadata, process samples, and HMI events as separate tables.
- Catalog schema and data-quality differences between runs.
- Extract `MASH_START` to `STRIKE_REACHED` segments.
- Compare an adaptive online heating-rate estimator with an effective thermal model.

The generated output is ignored by Git because it is derived from run data stored outside the repository.

## Run The Analysis

From the repository root:

```powershell
python -m modeling.analyze_runs --input "Z:\BeerProject\Beer Runs"
```

Generated files are written to `modeling/output`:

- `run_catalog.csv`: one quality and provenance row per source file.
- `events.csv`: normalized HMI and process events.
- `heating_segments.csv`: extracted heat-to-strike intervals and eligibility reasons.
- `heating_online_evaluation.csv`: adaptive ETA results by observation window.
- `heating_thermal_evaluation.csv`: leave-one-run-out thermal-model results.
- `data_quality_report.md`: data-foundation summary.
- `heating_model_report.md`: model comparison and initial recommendation.
- `heating_to_strike.png`: measured and predicted heating curves.

## Run Tests

```powershell
python -m unittest discover -s modeling/tests -v
```

## Model Boundary

The first HMI-ready model is intentionally simple: estimate the current run's heating rate after a short observation period and project the strike ETA. The effective thermal model is the bridge to OpenModelica, but it cannot yet be considered a fully physical model because ambient temperature, heater power, and vessel thermal mass are not recorded.

Mash-in recovery and mash-hold control will be modeled separately after the heating plant model is reliable.

## Current Pump Assumption

The pump is manually controlled and its physical state is not yet routed back to a controller pin. Until that feedback exists, models and future HMI replay use this operating assumption:

- Pump on during heat-to-strike and mash.
- Pump off when the fluid reaches boiling.

The logged `pump_on` field is retained as raw evidence but is not treated as authoritative for these runs.

## HMI Workflow Replay

The importer keeps event rows separate from sensor samples so a future simulator can replay both the physical response and the operator workflow. The intended replay order is:

1. Load setup values into the simulated Process Setup screen.
2. Advance the process state from normalized events such as `FILL_LOCKED`, `MASH_START`, `STRIKE_REACHED`, and `MASH_IN`.
3. Update HMI measurements from the nearest timestamped sample.
4. Apply the manual pump assumption until pump feedback is instrumented.
5. Allow a simulated operator to make the same confirmations and transitions required by the physical HMI.

The normalized `events.csv` output is the initial contract for that replay layer.

## Next Modeling Step: Mash Control

After the heating model is stable, analysis will split each mash into two intervals:

- Mash-in recovery: the temperature disturbance immediately after `MASH_IN`.
- Mash hold: the settled portion before `MASH_OUT`.

The controller comparison should measure overshoot, undershoot, settling time, mean absolute setpoint error, integrated absolute error, heater switching, and commanded duty. Candidate controls can then be tested against the same thermal plant before any firmware change is made.
