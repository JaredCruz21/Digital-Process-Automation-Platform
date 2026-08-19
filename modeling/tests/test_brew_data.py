from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from modeling.brew_data import parse_run_file
from modeling.heating_model import extract_heating_segment, fit_online_rate, fit_transient_rate


PROCESS_HEADER = (
    "timestamp_s,page,state,temp_c,setpoint_c,display_vol_gal,live_vol_gal,"
    "live_voltage_v,heater_on,pump_on,time_remaining_s,sg,resistance_ohms,"
    "boil_aggression_pct"
)


class BrewDataTests(unittest.TestCase):
    def _write_fixture(self, contents: str) -> Path:
        temporary = tempfile.NamedTemporaryFile(mode="w", suffix=".csv", delete=False, encoding="utf-8")
        temporary.write(contents)
        temporary.close()
        self.addCleanup(Path(temporary.name).unlink, missing_ok=True)
        return Path(temporary.name)

    def test_process_log_splits_samples_and_hmi_events(self) -> None:
        path = self._write_fixture(
            "\n".join(
                [
                    "[SETUP]",
                    "run_name,fixture",
                    "process_temp_cal_R2,138.50",
                    "",
                    "[DATA]",
                    PROCESS_HEADER,
                    "0,EVENT,MASH_START,heating_to_strike",
                    "5,MASH,HEATING TO STRIKE,20,67,4,4,0.8,1,0,60,0,108,0",
                    "65,MASH,HEATING TO STRIKE,68,67,4,4,0.8,1,0,0,0,125,0",
                    "70,EVENT,STRIKE_REACHED,67.00",
                ]
            )
        )

        run = parse_run_file(path)

        self.assertEqual(run.schema_version, "process-v2")
        self.assertEqual(run.run_name, "fixture")
        self.assertEqual(len(run.samples), 2)
        self.assertEqual(run.events["event_name"].tolist(), ["MASH_START", "STRIKE_REACHED"])
        self.assertEqual(run.events.iloc[1]["event_detail"], "67.00")

    def test_fermentation_event_columns_are_normalized(self) -> None:
        path = self._write_fixture(
            "\n".join(
                [
                    "[FERMENTATION]",
                    "run_name,ferm_fixture",
                    "",
                    "[DATA]",
                    "timestamp_s,state,temp_c,min_temp_c,max_temp_c,sixhr_avg_temp_c,avg_temp_c,elapsed_s,sg,resistance_ohms",
                    "10,EVENT,FILE_CREATED,ferm_fixture.csv",
                    "40,LOGGING TEMP,20.1,20.1,20.1,20.1,20.1,30,0,108.1",
                ]
            )
        )

        run = parse_run_file(path)

        self.assertEqual(run.schema_version, "fermentation-v1")
        self.assertEqual(run.events.iloc[0]["event_name"], "FILE_CREATED")
        self.assertEqual(run.events.iloc[0]["event_detail"], "ferm_fixture.csv")
        self.assertAlmostEqual(float(run.samples.iloc[0]["temp_c"]), 20.1)

    def test_standalone_temperature_calibration_is_classified(self) -> None:
        path = self._write_fixture("R1,T1,R2,T2\n100.00,0.00,138.50,100.00\n")

        run = parse_run_file(path)

        self.assertEqual(run.schema_version, "temperature-calibration-v1")
        self.assertEqual(run.run_type, "calibration")
        self.assertEqual(len(run.samples), 1)
        self.assertAlmostEqual(float(run.samples.iloc[0]["R2"]), 138.5)

    def test_heating_segment_and_online_rate_are_extracted(self) -> None:
        data_rows = ["0,EVENT,MASH_START,heating_to_strike"]
        for timestamp in range(5, 126, 5):
            temperature = 20.0 + 0.1 * timestamp
            data_rows.append(
                f"{timestamp},MASH,HEATING TO STRIKE,{temperature:.2f},34,4,4,0.8,1,0,60,0,108,0"
            )
        data_rows.append("130,EVENT,STRIKE_REACHED,33.00")
        path = self._write_fixture(
            "\n".join(
                [
                    "[SETUP]",
                    "run_name,heat_fixture",
                    "process_temp_cal_R2,138.50",
                    "",
                    "[DATA]",
                    PROCESS_HEADER,
                    *data_rows,
                ]
            )
        )

        segment = extract_heating_segment(parse_run_file(path))
        self.assertIsNotNone(segment)
        assert segment is not None
        self.assertTrue(segment.usable)

        result = fit_online_rate(segment, 60.0)
        self.assertIsNotNone(result)
        assert result is not None
        self.assertAlmostEqual(float(result["slope_c_per_min"]), 6.0, places=6)

        transient_result = fit_transient_rate(segment, 60.0)
        self.assertIsNotNone(transient_result)
        assert transient_result is not None
        self.assertGreater(float(transient_result["predicted_duration_min"]), 0.0)


if __name__ == "__main__":
    unittest.main()
