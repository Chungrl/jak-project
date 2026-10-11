"""Asset-free regression checks for benchmark analysis, runnable on Windows."""

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import summarize_texture_benchmark as benchmark


class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def csv(self, content, name="frames.csv"):
        path = self.root / name
        path.write_text(content, encoding="utf-8-sig")
        return path

    def frames(self, values):
        return self.csv("ProcessID,SwapChainAddress,MsBetweenPresents\n" +
                        "".join(f"42,0x1,{value}\n" for value in values))

    def test_known_statistics_and_slow_tail(self):
        result = benchmark.summarize_frames(self.frames([10] * 99 + [100]), 42)
        self.assertAlmostEqual(result["mean_ms"], 10.9)
        self.assertAlmostEqual(result["mean_fps"], 1000 / 10.9)
        self.assertAlmostEqual(result["p99_ms"], 10.9)
        self.assertEqual(result["p95_ms"], 10)
        self.assertEqual(result["median_ms"], 10)
        self.assertEqual(result["one_percent_low_fps"], 10)
        self.assertEqual(result["over_50_ms"], 1)

    def test_process_filter_and_initial_zero(self):
        path = self.csv("ProcessID,SwapChainAddress,MsBetweenPresents\n"
                        "99,other,NA\n42,0x1,0\n42,0x1,20\n")
        result = benchmark.summarize_frames(path, 42)
        self.assertEqual(result["excluded_rows"], 1)
        self.assertTrue(result["initial_zero_skipped"])
        self.assertEqual(result["frames"], 1)
        self.assertEqual(result["p99_ms"], 20)

    def test_multiple_streams_require_selection(self):
        path = self.csv("ProcessID,SwapChainAddress,MsBetweenPresents\n"
                        "42,0x1,10\n42,0x2,30\n")
        with self.assertRaisesRegex(ValueError, "Multiple swap chains"):
            benchmark.summarize_frames(path, 42)
        self.assertEqual(benchmark.summarize_frames(path, 42, "0x2")["mean_ms"], 30)

    def test_bad_intervals_fail_instead_of_hiding_stutters(self):
        for value in ["NA", "", "nan", "inf", "-1", "0"]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                benchmark.summarize_frames(self.frames([10, value]), 42)

    def test_empty_missing_duplicate_or_unmatched(self):
        for content in ["", "ProcessID,MsBetweenPresents\n42,10\n",
                        "ProcessID,ProcessID,SwapChainAddress,MsBetweenPresents\n",
                        "ProcessID,SwapChainAddress,MsBetweenPresents\n99,0x1,10\n"]:
            with self.subTest(content=content), self.assertRaises(ValueError):
                benchmark.summarize_frames(self.csv(content), 42)

    def test_all_zero_fails(self):
        with self.assertRaises(ValueError):
            benchmark.summarize_frames(self.frames([0]), 42)

    def test_vram_units_and_zero(self):
        for unit, value in [("MiB", 1024), ("GiB", 1), ("MB", 1073.741824),
                            ("bytes", 1073741824)]:
            with self.subTest(unit=unit):
                path = self.csv(f"GPU allocation\n0\n{value}\n", "vram.csv")
                result = benchmark.summarize_vram(path, "GPU allocation", unit)
                self.assertAlmostEqual(result["peak_mib"], 1024)
                self.assertAlmostEqual(result["median_mib"], 512)

    def test_bad_vram_fails(self):
        for value in ["nan", "inf", "-1", "NA", ""]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                benchmark.summarize_vram(self.csv(f"memory\n{value}\n"), "memory", "MiB")

    def test_unquoted_decimal_comma_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "extra CSV fields"):
            benchmark.summarize_vram(self.csv("memory\n1,024\n"), "memory", "MiB")

    def test_cli_writes_json_with_paths_containing_spaces(self):
        frames = self.frames([10, 20])
        memory = self.csv("memory\n512\n1024\n", "GPU memory.csv")
        output = self.root / "comparison result.json"
        benchmark.main([str(frames), "--process-id", "42", "--label", "baseline",
                        "--vram-csv", str(memory), "--vram-column", "memory",
                        "--vram-unit", "MiB", "--output", str(output)])
        result = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(result["label"], "baseline")
        self.assertEqual(result["frames"]["mean_ms"], 15)
        self.assertEqual(result["vram"]["peak_mib"], 1024)

    def test_cli_requires_complete_vram_options_and_positive_pid(self):
        for options in [["--process-id", "0"],
                        ["--process-id", "42", "--vram-csv", "memory.csv"]]:
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                benchmark.main([str(self.frames([10])), "--label", "test"] + options)
            self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
