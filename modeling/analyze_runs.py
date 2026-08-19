"""Build the run catalog, quality report, and first heating-model evaluation."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from modeling.brew_data import BrewRun, load_run_directory
from modeling.heating_model import (
    HeatingSegment,
    evaluate_effective_thermal_model,
    evaluate_online_models,
    extract_heating_segment,
    fit_online_rate,
    fit_transient_rate,
    predict_online_model,
    segment_summary,
)


DEFAULT_INPUT = Path("Z:/BeerProject/Beer Runs")
DEFAULT_OUTPUT = Path(__file__).resolve().parent / "output"


def _catalog_row(run: BrewRun) -> dict[str, object]:
    samples = run.samples
    temperatures = samples.get("temp_c", pd.Series(dtype=float))
    valid_temperatures = temperatures.loc[temperatures.between(-20.0, 105.0)]
    duration_min = np.nan
    if not samples.empty and "timestamp_s" in samples:
        timestamps = samples["timestamp_s"].dropna()
        if len(timestamps) >= 2:
            duration_min = float((timestamps.max() - timestamps.min()) / 60.0)

    live_volume = samples.get("live_vol_gal", pd.Series(dtype=float))
    out_of_range_volume = int((~live_volume.between(0.0, 10.0) & live_volume.notna()).sum())
    zero_volume = int(live_volume.eq(0.0).sum())
    pump_on_samples = int(samples.get("pump_on", pd.Series(dtype=float)).eq(1.0).sum())

    flags = []
    if run.schema_version == "process-v1":
        flags.append("legacy_process_schema")
    invalid_temp_count = int(len(temperatures) - len(valid_temperatures))
    if invalid_temp_count:
        flags.append("invalid_temperature")
    if out_of_range_volume:
        flags.append("volume_out_of_range")
    if zero_volume:
        flags.append("zero_volume_samples")
    if run.run_type == "process" and pump_on_samples == 0:
        flags.append("pump_state_uninstrumented")

    return {
        "source_file": run.source_path.name,
        "run_name": run.run_name,
        "run_type": run.run_type,
        "schema_version": run.schema_version,
        "started_local": run.metadata.get(
            "log_started_local_pacific", run.metadata.get("log_created_local_pacific", "")
        ),
        "samples": len(samples),
        "events": len(run.events),
        "duration_min": duration_min,
        "valid_temp_min_c": float(valid_temperatures.min()) if not valid_temperatures.empty else np.nan,
        "valid_temp_max_c": float(valid_temperatures.max()) if not valid_temperatures.empty else np.nan,
        "invalid_temp_samples": invalid_temp_count,
        "volume_out_of_range_samples": out_of_range_volume,
        "zero_volume_samples": zero_volume,
        "pump_on_samples": pump_on_samples,
        "quality_flags": ";".join(flags),
    }


def _markdown_table(frame: pd.DataFrame, columns: list[str]) -> str:
    if frame.empty:
        return "No records."
    display = frame.loc[:, columns].copy().fillna("")
    header = "| " + " | ".join(columns) + " |"
    separator = "| " + " | ".join("---" for _ in columns) + " |"
    rows = [
        "| " + " | ".join(str(value).replace("|", "\\|") for value in row) + " |"
        for row in display.itertuples(index=False, name=None)
    ]
    return "\n".join([header, separator, *rows])


def _write_quality_report(
    path: Path,
    input_path: Path,
    catalog: pd.DataFrame,
    failures: list[dict[str, str]],
    segments: pd.DataFrame,
) -> None:
    schema_counts = catalog.groupby("schema_version").size().reset_index(name="files")
    usable_segments = int(segments.get("usable", pd.Series(dtype=bool)).sum())
    content = f"""# Run Data Quality Report

## Source

- Input directory: `{input_path}`
- CSV files parsed: {len(catalog)}
- Parse failures: {len(failures)}
- Heat-to-strike segments found: {len(segments)}
- Heat-to-strike segments passing initial quality rules: {usable_segments}

## Schemas

{_markdown_table(schema_counts, ["schema_version", "files"])}

## Heat-To-Strike Segments

{_markdown_table(segments.round(3), ["source_file", "schema_version", "usable", "quality_flags", "duration_min", "start_temp_c", "end_temp_c", "target_c", "volume_gal"])}

## Current Data Rules

- Process-v1 files are retained in the catalog but excluded from model fitting.
- Temperatures outside -20 C to 105 C are marked invalid.
- Heat-to-strike fitting requires at least eight samples, 60 seconds of data, 3 C of temperature rise, a locked volume, mostly-on heater output, and no sample gap over 15 seconds.
- Raw files remain unchanged; all generated artifacts are written under `modeling/output`.

## Known Limitations

- Ambient temperature and heater power are not recorded.
- The pump output is not instrumented in the current logs. The process model assumes the manually controlled pump is on during heating and mash, then off when boiling begins.
- Live volume contains zero and out-of-range values in several runs.
- Water commissioning runs should not be assumed to represent grain mash behavior.
"""
    path.write_text(content, encoding="utf-8")


def _write_heating_report(
    path: Path,
    online: pd.DataFrame,
    thermal: pd.DataFrame,
    segments: list[HeatingSegment],
) -> tuple[str, float]:
    if online.empty:
        path.write_text("# Heating Model Report\n\nNo usable heating segments.\n", encoding="utf-8")
        return "linear_rate", 60.0

    window_summary = (
        online.groupby(["model", "observation_window_s"])
        .agg(
            runs=("source_file", "count"),
            median_abs_eta_error_min=("absolute_eta_error_min", "median"),
            mean_abs_eta_error_min=("absolute_eta_error_min", "mean"),
            max_abs_eta_error_min=("absolute_eta_error_min", "max"),
            mean_curve_mae_c=("curve_mae_c", "mean"),
        )
        .reset_index()
    )
    eligible_comparisons = window_summary.loc[window_summary["runs"].ge(3)]
    selected = eligible_comparisons.sort_values(
        ["mean_abs_eta_error_min", "median_abs_eta_error_min", "observation_window_s"]
    ).iloc[0]
    best_model = str(selected["model"])
    best_window_s = float(selected["observation_window_s"])
    best_online = online.loc[
        online["model"].eq(best_model) & online["observation_window_s"].eq(best_window_s)
    ].copy()
    accepted = bool(
        float(selected["median_abs_eta_error_min"]) <= 2.0
        and float(selected["max_abs_eta_error_min"]) <= 5.0
    )
    thermal_summary = pd.DataFrame()
    if not thermal.empty:
        thermal_summary = pd.DataFrame(
            [
                {
                    "held_out_runs": len(thermal),
                    "mean_curve_mae_c": thermal["curve_mae_c"].mean(),
                    "median_abs_final_error_c": thermal["final_error_c"].abs().median(),
                }
            ]
        )

    content = f"""# Heating-To-Strike Model Report

## Result

The best tested online candidate is **{best_model}**, but it is **{"accepted" if accepted else "not yet accepted"}** for an HMI ETA. The initial acceptance rule is a median absolute ETA error no greater than 2 minutes and no individual error greater than 5 minutes.

Selected observation window from the current comparison: **{best_window_s / 60.0:.1f} minutes**. The estimator should display `Estimating...` before this window is complete.

## Observation-Window Comparison

{_markdown_table(window_summary.round(3), list(window_summary.columns))}

## Selected Adaptive-Rate Results

{_markdown_table(best_online.round(3), ["source_file", "model", "slope_c_per_min", "predicted_duration_min", "actual_duration_min", "eta_error_min", "curve_mae_c"])}

## Effective Thermal Model Cross-Validation

The effective thermal model uses `dT/dt = heat_gain / volume - loss * (T - start_temperature)` and holds out one complete run at a time. It is a bridge toward OpenModelica, not yet a final physical model because heater power and ambient temperature are missing.

{_markdown_table(thermal_summary.round(3), list(thermal_summary.columns) if not thermal_summary.empty else [])}

## Interpretation

- Treat the fast initial temperature change as a separate transient instead of extrapolating it across the entire heat-up.
- The current logs suggest a fast probe or recirculation-loop response followed by slower bulk heating. Because the pump is manually on but not instrumented, a two-node thermal model should represent the recirculating sensor path and the bulk vessel as the next hypothesis.
- Update the HMI estimate as new samples arrive and expose prediction confidence based on elapsed observation time.
- Continue developing the effective thermal model as equipment metadata becomes available.
- Do not tune mash control from heat-up segments. Mash-in recovery and mash hold should be evaluated as separate phases after the plant model is stable.
"""
    path.write_text(content, encoding="utf-8")
    return best_model, best_window_s


def _plot_heating_segments(
    path: Path, segments: list[HeatingSegment], model: str, observation_window_s: float
) -> None:
    usable = [segment for segment in segments if segment.usable]
    if not usable:
        return

    columns = 2
    rows = int(np.ceil(len(usable) / columns))
    figure, axes = plt.subplots(rows, columns, figsize=(12, 4 * rows), squeeze=False)

    for axis, segment in zip(axes.flat, usable):
        frame = segment.samples
        elapsed_min = frame["elapsed_s"] / 60.0
        axis.plot(elapsed_min, frame["temp_c"], color="#167c80", linewidth=2, label="Measured")

        fit_function = fit_transient_rate if model == "transient_rate" else fit_online_rate
        fit = fit_function(segment, observation_window_s)
        if fit is not None:
            predicted = predict_online_model(fit, frame["elapsed_s"].to_numpy(dtype=float))
            axis.plot(elapsed_min, predicted, color="#d65a31", linestyle="--", label=model)
            axis.axvline(observation_window_s / 60.0, color="#777777", linewidth=1, alpha=0.7)

        axis.axhline(segment.target_c, color="#333333", linewidth=1, alpha=0.55, label="Strike target")
        axis.set_title(segment.source_file)
        axis.set_xlabel("Elapsed minutes")
        axis.set_ylabel("Temperature C")
        axis.grid(alpha=0.2)
        axis.legend(fontsize=8)

    for axis in axes.flat[len(usable) :]:
        axis.remove()

    figure.suptitle("Heat-to-strike model comparison", fontsize=15)
    figure.tight_layout()
    figure.savefig(path, dpi=160)
    plt.close(figure)


def build_analysis(input_path: Path, output_path: Path) -> None:
    runs, failures = load_run_directory(input_path)
    if not runs:
        raise RuntimeError(f"no readable CSV runs found in {input_path}")

    output_path.mkdir(parents=True, exist_ok=True)
    catalog = pd.DataFrame.from_records([_catalog_row(run) for run in runs])
    catalog.to_csv(output_path / "run_catalog.csv", index=False)

    all_events = pd.concat([run.events for run in runs if not run.events.empty], ignore_index=True)
    all_events.to_csv(output_path / "events.csv", index=False)

    heating_segments = [segment for run in runs if (segment := extract_heating_segment(run))]
    segments_frame = segment_summary(heating_segments)
    segments_frame.to_csv(output_path / "heating_segments.csv", index=False)

    online_results = evaluate_online_models(heating_segments)
    online_results.to_csv(output_path / "heating_online_evaluation.csv", index=False)
    thermal_results = evaluate_effective_thermal_model(heating_segments)
    thermal_results.to_csv(output_path / "heating_thermal_evaluation.csv", index=False)

    _write_quality_report(
        output_path / "data_quality_report.md",
        input_path,
        catalog,
        failures,
        segments_frame,
    )
    best_model, best_window_s = _write_heating_report(
        output_path / "heating_model_report.md", online_results, thermal_results, heating_segments
    )
    _plot_heating_segments(
        output_path / "heating_to_strike.png", heating_segments, best_model, best_window_s
    )

    print(f"Parsed {len(runs)} CSV files from {input_path}")
    print(f"Found {len(heating_segments)} heat-to-strike segments")
    print(f"Usable segments: {sum(segment.usable for segment in heating_segments)}")
    print(f"Reports written to {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT, help="Directory containing run CSVs")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Generated report directory")
    arguments = parser.parse_args()
    build_analysis(arguments.input, arguments.output)


if __name__ == "__main__":
    main()
