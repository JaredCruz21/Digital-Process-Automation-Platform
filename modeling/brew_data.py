"""Read the controller's sectioned CSV logs without modifying the source files."""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import pandas as pd


NUMERIC_COLUMNS = {
    "timestamp_s",
    "temp_c",
    "setpoint_c",
    "display_vol_gal",
    "live_vol_gal",
    "live_voltage_v",
    "heater_on",
    "pump_on",
    "time_remaining_s",
    "sg",
    "resistance_ohms",
    "boil_aggression_pct",
    "min_temp_c",
    "max_temp_c",
    "sixhr_avg_temp_c",
    "avg_temp_c",
    "elapsed_s",
}


@dataclass
class BrewRun:
    """Normalized contents of one process or fermentation log."""

    source_path: Path
    section: str
    schema_version: str
    metadata: dict[str, str]
    samples: pd.DataFrame
    events: pd.DataFrame

    @property
    def run_name(self) -> str:
        return self.metadata.get("run_name", self.source_path.stem)

    @property
    def run_type(self) -> str:
        if self.schema_version.startswith("process"):
            return "process"
        if self.schema_version.startswith("fermentation"):
            return "fermentation"
        if self.schema_version.startswith("temperature-calibration"):
            return "calibration"
        return "unknown"


def _trim_trailing_empty(row: Iterable[str]) -> list[str]:
    values = [value.strip() for value in row]
    while values and values[-1] == "":
        values.pop()
    return values


def _find_data_marker(rows: list[list[str]]) -> int:
    for index, row in enumerate(rows):
        if row and row[0].strip() == "[DATA]":
            return index
    raise ValueError("missing [DATA] section")


def _find_next_nonempty(rows: list[list[str]], start: int) -> int:
    for index in range(start, len(rows)):
        if any(value.strip() for value in rows[index]):
            return index
    raise ValueError("missing data header after [DATA]")


def _detect_schema(columns: list[str]) -> str:
    if "page" in columns and "live_voltage_v" in columns:
        return "process-v2"
    if "page" in columns:
        return "process-v1"
    if "elapsed_s" in columns and "state" in columns:
        return "fermentation-v1"
    return "unknown-v1"


def _parse_standalone_calibration(source_path: Path, rows: list[list[str]]) -> BrewRun:
    header = _trim_trailing_empty(rows[0])
    required = {"R1", "T1", "R2", "T2"}
    if not required.issubset(header):
        raise ValueError("missing [DATA] section")

    records = []
    for source_row, raw_row in enumerate(rows[1:], start=2):
        row = _trim_trailing_empty(raw_row)
        if not row:
            continue
        padded = row + [""] * max(0, len(header) - len(row))
        record: dict[str, str | int] = dict(zip(header, padded[: len(header)]))
        record["source_row"] = source_row
        records.append(record)

    samples = pd.DataFrame.from_records(records, columns=[*header, "source_row"])
    for column in required.intersection(samples.columns):
        samples[column] = pd.to_numeric(samples[column], errors="coerce")

    schema_version = "temperature-calibration-v1"
    samples.insert(0, "schema_version", schema_version)
    samples.insert(0, "run_name", source_path.stem)
    samples.insert(0, "source_file", source_path.name)
    events = pd.DataFrame(
        columns=["source_file", "run_name", "schema_version", "timestamp_s", "event_name", "event_detail", "source_row"]
    )
    return BrewRun(
        source_path=source_path,
        section="CALIBRATION",
        schema_version=schema_version,
        metadata={},
        samples=samples,
        events=events,
    )


def _convert_numeric_columns(frame: pd.DataFrame) -> pd.DataFrame:
    frame = frame.copy()
    for column in NUMERIC_COLUMNS.intersection(frame.columns):
        frame[column] = pd.to_numeric(frame[column], errors="coerce")
    return frame


def parse_run_file(path: str | Path) -> BrewRun:
    """Parse one controller log into metadata, samples, and normalized events."""

    source_path = Path(path)
    with source_path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.reader(handle))

    if not rows:
        raise ValueError("empty file")

    try:
        marker_index = _find_data_marker(rows)
    except ValueError:
        return _parse_standalone_calibration(source_path, rows)
    section_row = _trim_trailing_empty(rows[0])
    section = section_row[0].strip("[]") if section_row else "UNKNOWN"

    metadata: dict[str, str] = {}
    for raw_row in rows[1:marker_index]:
        row = _trim_trailing_empty(raw_row)
        if len(row) >= 2 and row[0]:
            metadata[row[0]] = row[1]

    header_index = _find_next_nonempty(rows, marker_index + 1)
    columns = _trim_trailing_empty(rows[header_index])
    if not columns:
        raise ValueError("empty data header")

    records: list[dict[str, str | int]] = []
    for source_row, raw_row in enumerate(rows[header_index + 1 :], start=header_index + 2):
        row = _trim_trailing_empty(raw_row)
        if not row:
            continue
        padded = row + [""] * max(0, len(columns) - len(row))
        record: dict[str, str | int] = dict(zip(columns, padded[: len(columns)]))
        record["source_row"] = source_row
        records.append(record)

    raw = pd.DataFrame.from_records(records, columns=[*columns, "source_row"])
    schema_version = metadata.get("schema_version", "").strip() or _detect_schema(columns)

    if raw.empty:
        event_mask = pd.Series(dtype=bool)
    elif "page" in raw.columns:
        event_mask = raw["page"].astype(str).str.upper().eq("EVENT")
    else:
        event_mask = raw["state"].astype(str).str.upper().eq("EVENT")

    event_rows = raw.loc[event_mask].copy()
    sample_rows = raw.loc[~event_mask].copy()

    if "page" in raw.columns:
        event_names = event_rows.get("state", pd.Series(index=event_rows.index, dtype=str))
        event_details = event_rows.get("temp_c", pd.Series(index=event_rows.index, dtype=str))
    else:
        event_names = event_rows.get("temp_c", pd.Series(index=event_rows.index, dtype=str))
        event_details = event_rows.get("min_temp_c", pd.Series(index=event_rows.index, dtype=str))

    events = pd.DataFrame(
        {
            "timestamp_s": pd.to_numeric(event_rows.get("timestamp_s"), errors="coerce"),
            "event_name": event_names.astype(str),
            "event_detail": event_details.astype(str),
            "source_row": event_rows.get("source_row"),
        }
    )

    samples = _convert_numeric_columns(sample_rows)
    for frame in (samples, events):
        frame.insert(0, "schema_version", schema_version)
        frame.insert(0, "run_name", metadata.get("run_name", source_path.stem))
        frame.insert(0, "source_file", source_path.name)

    return BrewRun(
        source_path=source_path,
        section=section,
        schema_version=schema_version,
        metadata=metadata,
        samples=samples.reset_index(drop=True),
        events=events.reset_index(drop=True),
    )


def load_run_directory(path: str | Path) -> tuple[list[BrewRun], list[dict[str, str]]]:
    """Load every CSV in a directory and return runs plus parse failures."""

    directory = Path(path)
    runs: list[BrewRun] = []
    failures: list[dict[str, str]] = []

    for source_path in sorted(directory.glob("*.csv"), key=lambda item: item.name.lower()):
        try:
            runs.append(parse_run_file(source_path))
        except (OSError, UnicodeError, ValueError, csv.Error) as exc:
            failures.append({"source_file": source_path.name, "error": str(exc)})

    return runs, failures
