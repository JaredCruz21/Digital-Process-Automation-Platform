"""Extract and evaluate heat-to-strike models from normalized controller runs."""

from __future__ import annotations

import re
from dataclasses import dataclass

import numpy as np
import pandas as pd

from modeling.brew_data import BrewRun


@dataclass
class HeatingSegment:
    source_file: str
    run_name: str
    schema_version: str
    samples: pd.DataFrame
    start_time_s: float
    end_time_s: float
    target_c: float
    volume_gal: float
    heater_fraction: float
    quality_flags: tuple[str, ...]

    @property
    def duration_s(self) -> float:
        return self.end_time_s - self.start_time_s

    @property
    def usable(self) -> bool:
        return not self.quality_flags


def _first_number(value: object) -> float | None:
    match = re.search(r"[-+]?\d+(?:\.\d+)?", str(value))
    return float(match.group(0)) if match else None


def _event_after(run: BrewRun, name: str, after_s: float = -np.inf) -> pd.Series | None:
    matches = run.events.loc[
        run.events["event_name"].eq(name) & run.events["timestamp_s"].gt(after_s)
    ]
    if matches.empty:
        return None
    return matches.sort_values("timestamp_s").iloc[0]


def extract_heating_segment(run: BrewRun) -> HeatingSegment | None:
    """Extract the first MASH_START to STRIKE_REACHED interval from a process run."""

    if run.run_type != "process" or run.samples.empty:
        return None

    start_event = _event_after(run, "MASH_START")
    if start_event is None:
        return None
    start_time_s = float(start_event["timestamp_s"])

    end_event = _event_after(run, "STRIKE_REACHED", start_time_s)
    if end_event is None:
        return None
    end_time_s = float(end_event["timestamp_s"])

    samples = run.samples.copy()
    samples = samples.loc[
        samples["timestamp_s"].between(start_time_s, end_time_s)
        & samples.get("page", "").astype(str).eq("MASH")
    ].copy()
    samples = samples.loc[samples["temp_c"].between(0.0, 105.0)].copy()
    samples = samples.sort_values("timestamp_s")
    if samples.empty:
        return None

    samples["elapsed_s"] = samples["timestamp_s"] - start_time_s
    volume_values = samples.loc[samples["display_vol_gal"].between(0.25, 15.0), "display_vol_gal"]
    volume_gal = float(volume_values.median()) if not volume_values.empty else np.nan
    heater_fraction = float(samples["heater_on"].mean())

    target_c = _first_number(end_event["event_detail"])
    if target_c is None:
        setpoints = samples.loc[samples["setpoint_c"].between(1.0, 105.0), "setpoint_c"]
        target_c = float(setpoints.median()) if not setpoints.empty else float(samples["temp_c"].iloc[-1])

    flags: list[str] = []
    if run.schema_version not in {"process-v2", "process-v3"}:
        flags.append("legacy_schema")
    if len(samples) < 8:
        flags.append("too_few_samples")
    if end_time_s - start_time_s < 60.0:
        flags.append("segment_under_60s")
    if float(samples["temp_c"].iloc[-1] - samples["temp_c"].iloc[0]) < 3.0:
        flags.append("temperature_rise_under_3c")
    if not np.isfinite(volume_gal):
        flags.append("missing_locked_volume")
    if heater_fraction < 0.8:
        flags.append("heater_not_continuously_on")

    gaps = samples["timestamp_s"].diff().dropna()
    if not gaps.empty and float(gaps.max()) > 15.0:
        flags.append("sample_gap_over_15s")

    return HeatingSegment(
        source_file=run.source_path.name,
        run_name=run.run_name,
        schema_version=run.schema_version,
        samples=samples,
        start_time_s=start_time_s,
        end_time_s=end_time_s,
        target_c=float(target_c),
        volume_gal=volume_gal,
        heater_fraction=heater_fraction,
        quality_flags=tuple(flags),
    )


def segment_summary(segments: list[HeatingSegment]) -> pd.DataFrame:
    records = []
    for segment in segments:
        temperatures = segment.samples["temp_c"]
        records.append(
            {
                "source_file": segment.source_file,
                "run_name": segment.run_name,
                "schema_version": segment.schema_version,
                "usable": segment.usable,
                "quality_flags": ";".join(segment.quality_flags),
                "samples": len(segment.samples),
                "duration_min": segment.duration_s / 60.0,
                "start_temp_c": float(temperatures.iloc[0]),
                "end_temp_c": float(temperatures.iloc[-1]),
                "target_c": segment.target_c,
                "volume_gal": segment.volume_gal,
                "heater_fraction": segment.heater_fraction,
                "pump_assumption": "manual_on_during_heat_and_mash",
            }
        )
    return pd.DataFrame.from_records(records)


def fit_online_rate(segment: HeatingSegment, observation_window_s: float) -> dict[str, float | str] | None:
    """Fit a live linear heating rate from the beginning of one run."""

    observed = segment.samples.loc[segment.samples["elapsed_s"].le(observation_window_s)]
    if len(observed) < 5 or segment.duration_s <= observation_window_s:
        return None

    elapsed = observed["elapsed_s"].to_numpy(dtype=float)
    temperatures = observed["temp_c"].to_numpy(dtype=float)
    design = np.column_stack((elapsed, np.ones_like(elapsed)))
    slope_c_per_s, intercept_c = np.linalg.lstsq(design, temperatures, rcond=None)[0]
    if slope_c_per_s <= 0.0:
        return None

    predicted_duration_s = (segment.target_c - intercept_c) / slope_c_per_s
    all_elapsed = segment.samples["elapsed_s"].to_numpy(dtype=float)
    predictions = intercept_c + slope_c_per_s * all_elapsed
    curve_mae_c = float(np.mean(np.abs(predictions - segment.samples["temp_c"].to_numpy(dtype=float))))

    return {
        "model": "linear_rate",
        "source_file": segment.source_file,
        "observation_window_s": observation_window_s,
        "intercept_c": intercept_c,
        "slope_c_per_s": slope_c_per_s,
        "slope_c_per_min": slope_c_per_s * 60.0,
        "predicted_duration_min": predicted_duration_s / 60.0,
        "actual_duration_min": segment.duration_s / 60.0,
        "eta_error_min": (predicted_duration_s - segment.duration_s) / 60.0,
        "absolute_eta_error_min": abs(predicted_duration_s - segment.duration_s) / 60.0,
        "curve_mae_c": curve_mae_c,
    }


def fit_transient_rate(
    segment: HeatingSegment, observation_window_s: float
) -> dict[str, float | str] | None:
    """Fit steady heating plus a fast sensor/local-volume thermal transient."""

    observed = segment.samples.loc[segment.samples["elapsed_s"].le(observation_window_s)]
    if len(observed) < 8 or segment.duration_s <= observation_window_s:
        return None

    elapsed = observed["elapsed_s"].to_numpy(dtype=float)
    temperatures = observed["temp_c"].to_numpy(dtype=float)
    linear_design = np.column_stack((np.ones_like(elapsed), elapsed))
    linear_intercept, linear_slope = np.linalg.lstsq(
        linear_design, temperatures, rcond=None
    )[0]
    linear_fitted = linear_design @ np.array([linear_intercept, linear_slope])
    linear_error = float(np.mean((linear_fitted - temperatures) ** 2))
    best: tuple[float, float, float, float, float] | None = None
    if linear_slope > 0.0:
        best = (
            linear_error,
            60.0,
            float(linear_intercept),
            float(linear_slope),
            0.0,
        )

    for tau_s in np.geomspace(10.0, 900.0, 120):
        transient = 1.0 - np.exp(-elapsed / tau_s)
        design = np.column_stack((np.ones_like(elapsed), elapsed, transient))
        intercept_c, slope_c_per_s, transient_gain_c = np.linalg.lstsq(
            design, temperatures, rcond=None
        )[0]
        if slope_c_per_s <= 0.0 or transient_gain_c < 0.0:
            continue
        fitted = design @ np.array([intercept_c, slope_c_per_s, transient_gain_c])
        mean_square_error = float(np.mean((fitted - temperatures) ** 2))
        candidate = (
            mean_square_error,
            float(tau_s),
            float(intercept_c),
            float(slope_c_per_s),
            float(transient_gain_c),
        )
        if best is None or candidate[0] < best[0]:
            best = candidate

    if best is None:
        return None

    _, tau_s, intercept_c, slope_c_per_s, transient_gain_c = best

    def predict(elapsed_s: np.ndarray | float) -> np.ndarray | float:
        return (
            intercept_c
            + slope_c_per_s * elapsed_s
            + transient_gain_c * (1.0 - np.exp(-np.asarray(elapsed_s) / tau_s))
        )

    lower_s = 0.0
    upper_s = max(segment.duration_s * 2.0, 600.0)
    while float(predict(upper_s)) < segment.target_c and upper_s < 4.0 * 3600.0:
        upper_s *= 2.0
    if float(predict(upper_s)) < segment.target_c:
        return None

    for _ in range(60):
        midpoint_s = (lower_s + upper_s) / 2.0
        if float(predict(midpoint_s)) < segment.target_c:
            lower_s = midpoint_s
        else:
            upper_s = midpoint_s
    predicted_duration_s = (lower_s + upper_s) / 2.0

    all_elapsed = segment.samples["elapsed_s"].to_numpy(dtype=float)
    predictions = np.asarray(predict(all_elapsed), dtype=float)
    curve_mae_c = float(
        np.mean(np.abs(predictions - segment.samples["temp_c"].to_numpy(dtype=float)))
    )

    return {
        "model": "transient_rate",
        "source_file": segment.source_file,
        "observation_window_s": observation_window_s,
        "intercept_c": intercept_c,
        "slope_c_per_s": slope_c_per_s,
        "slope_c_per_min": slope_c_per_s * 60.0,
        "transient_gain_c": transient_gain_c,
        "tau_s": tau_s,
        "predicted_duration_min": predicted_duration_s / 60.0,
        "actual_duration_min": segment.duration_s / 60.0,
        "eta_error_min": (predicted_duration_s - segment.duration_s) / 60.0,
        "absolute_eta_error_min": abs(predicted_duration_s - segment.duration_s) / 60.0,
        "curve_mae_c": curve_mae_c,
    }


def evaluate_online_models(
    segments: list[HeatingSegment], windows_s: tuple[float, ...] = (60.0, 120.0, 180.0)
) -> pd.DataFrame:
    records = []
    for segment in segments:
        if not segment.usable:
            continue
        for window_s in windows_s:
            for fit_model in (fit_online_rate, fit_transient_rate):
                result = fit_model(segment, window_s)
                if result is not None:
                    records.append(result)
    return pd.DataFrame.from_records(records)


def predict_online_model(result: dict[str, object] | pd.Series, elapsed_s: np.ndarray) -> np.ndarray:
    intercept_c = float(result["intercept_c"])
    slope_c_per_s = float(result["slope_c_per_s"])
    predictions = intercept_c + slope_c_per_s * elapsed_s
    if result["model"] == "transient_rate":
        predictions = predictions + float(result["transient_gain_c"]) * (
            1.0 - np.exp(-elapsed_s / float(result["tau_s"]))
        )
    return np.asarray(predictions, dtype=float)


def _derivative_rows(segments: list[HeatingSegment]) -> tuple[np.ndarray, np.ndarray]:
    features: list[list[float]] = []
    targets: list[float] = []

    for segment in segments:
        frame = segment.samples.copy()
        smoothed = frame["temp_c"].rolling(window=3, center=True, min_periods=1).median()
        times = frame["elapsed_s"].to_numpy(dtype=float)
        temperatures = smoothed.to_numpy(dtype=float)
        heater = frame["heater_on"].fillna(0.0).to_numpy(dtype=float)

        for index in range(len(frame) - 1):
            delta_s = times[index + 1] - times[index]
            if delta_s <= 0.0 or delta_s > 15.0:
                continue
            rate = (temperatures[index + 1] - temperatures[index]) / delta_s
            features.append(
                [
                    heater[index] / segment.volume_gal,
                    -(temperatures[index] - temperatures[0]),
                ]
            )
            targets.append(rate)

    return np.asarray(features, dtype=float), np.asarray(targets, dtype=float)


def fit_effective_thermal_model(segments: list[HeatingSegment]) -> tuple[float, float]:
    """Fit dT/dt = heat_gain / volume - loss * (T - starting temperature)."""

    features, targets = _derivative_rows(segments)
    if len(targets) < 2:
        raise ValueError("at least two derivative samples are required")

    heat_gain, loss = np.linalg.lstsq(features, targets, rcond=None)[0]
    heat_gain = max(float(heat_gain), 0.0)
    loss = max(float(loss), 0.0)
    return heat_gain, loss


def simulate_effective_thermal_model(
    segment: HeatingSegment, heat_gain_c_gal_per_s: float, loss_per_s: float
) -> np.ndarray:
    frame = segment.samples
    times = frame["elapsed_s"].to_numpy(dtype=float)
    heater = frame["heater_on"].fillna(0.0).to_numpy(dtype=float)
    predictions = np.empty(len(frame), dtype=float)
    predictions[0] = float(frame["temp_c"].iloc[0])
    reference_c = predictions[0]

    for index in range(1, len(frame)):
        delta_s = max(times[index] - times[index - 1], 0.0)
        rate = (
            heat_gain_c_gal_per_s * heater[index - 1] / segment.volume_gal
            - loss_per_s * (predictions[index - 1] - reference_c)
        )
        predictions[index] = predictions[index - 1] + rate * delta_s

    return predictions


def evaluate_effective_thermal_model(segments: list[HeatingSegment]) -> pd.DataFrame:
    """Evaluate the shared thermal model by holding out one complete run at a time."""

    usable = [segment for segment in segments if segment.usable]
    records = []
    if len(usable) < 3:
        return pd.DataFrame()

    for held_out in usable:
        training = [segment for segment in usable if segment is not held_out]
        heat_gain, loss = fit_effective_thermal_model(training)
        predictions = simulate_effective_thermal_model(held_out, heat_gain, loss)
        actual = held_out.samples["temp_c"].to_numpy(dtype=float)
        records.append(
            {
                "source_file": held_out.source_file,
                "heat_gain_c_gal_per_s": heat_gain,
                "loss_per_s": loss,
                "curve_mae_c": float(np.mean(np.abs(predictions - actual))),
                "final_error_c": float(predictions[-1] - actual[-1]),
            }
        )

    return pd.DataFrame.from_records(records)
