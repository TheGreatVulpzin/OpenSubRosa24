"""Test process failures, pixel checks, artifact reports and comparisons."""
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("client_smoke", Path(__file__).with_name("smoke.py"))
smoke = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(smoke)


def bitmap(path, rows, top_down=False):
    height, width = len(rows), len(rows[0])
    stride = (width * 3 + 3) & ~3
    header = bytearray(54)
    struct.pack_into("<2sI4xIIiiHHI", header, 0, b"BM", 54 + stride * height, 54, 40,
                     width, -height if top_down else height, 1, 24, 0)
    data = bytearray(header)
    for row in rows if top_down else reversed(rows):
        for r, g, b in row:
            data.extend((b, g, r))
        data.extend(bytes(stride - width * 3))
    path.write_bytes(data)


class SmokeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.shots = self.folder / "shots"
        self.shots.mkdir()
        self.image = self.shots / "01-main.bmp"
        bitmap(self.image, [[(0, 0, 0), (255, 0, 0)]])
        self.log = self.folder / "run.log"
        self.log.write_text("SMOKE READY menus\nSMOKE PASS menus\n", encoding="utf-8")

    def result(self):
        return {"scenario": "menus", "ok": True, "error": "", "log": str(self.log)}

    def manifest(self, entry=None):
        entry = entry or {"name": "main", "path": "shots/01-main.bmp", "ok": True}
        (self.folder / "shots.jsonl").write_text(json.dumps(entry) + "\n", encoding="utf-8")

    def test_metrics_read_saved_pixels(self):
        result = smoke.image_metrics(self.image)
        self.assertTrue(result["ok"])
        self.assertEqual(result["black_fraction"], 0.5)
        self.assertEqual(result["distinct_colors"], 2)
        self.assertEqual(result["mean_rgb"], [127.5, 0, 0])

    def test_black_and_single_color_fail(self):
        for color in [(0, 0, 0), (255, 255, 255), (70, 80, 90)]:
            with self.subTest(color=color):
                bitmap(self.image, [[color] * 3])
                self.assertFalse(smoke.image_metrics(self.image)["ok"])

    def test_padding_and_row_direction(self):
        other = self.shots / "other.bmp"
        rows = [[(255, 0, 0)], [(0, 255, 0)]]
        bitmap(self.image, rows)
        bitmap(other, rows, top_down=True)
        self.assertEqual(smoke.pixel_difference(self.image, other), 0)

    def test_difference_percentage(self):
        other = self.shots / "other.bmp"
        bitmap(other, [[(0, 0, 0), (0, 255, 0)]])
        self.assertEqual(smoke.pixel_difference(self.image, other), 50)

    def test_different_dimensions(self):
        other = self.shots / "other.bmp"
        bitmap(other, [[(0, 0, 0)]])
        self.assertEqual(smoke.pixel_difference(self.image, other), 100)

    def test_bad_bitmap_rejected(self):
        for data in [b"", b"not a bitmap", self.image.read_bytes()[:-1]]:
            self.image.write_bytes(data)
            with self.assertRaises(ValueError):
                smoke.read_bitmap(self.image)

    def test_missing_capture_fails(self):
        result = self.result()
        smoke.collect_artifacts(self.folder, result)
        self.assertFalse(result["ok"])
        self.assertIn("no framebuffer captures", result["error"])
        self.assertTrue((self.folder / "index.md").exists())

    def test_capture_metrics_cannot_hide_black_pixels(self):
        self.manifest()
        bitmap(self.image, [[(0, 0, 0)] * 2])
        result = self.result()
        smoke.collect_artifacts(self.folder, result)
        self.assertFalse(result["ok"])
        self.assertIn("all black", result["error"])

    def test_bad_manifest_and_escape_rejected(self):
        self.manifest({"name": "escape", "path": "../outside.bmp"})
        result = self.result()
        smoke.collect_artifacts(self.folder, result)
        self.assertIn("outside shots", result["error"])
        (self.folder / "shots.jsonl").write_text('{"name":', encoding="utf-8")
        result = self.result()
        smoke.collect_artifacts(self.folder, result)
        self.assertFalse(result["ok"])

    def test_crash_retains_capture_and_failure(self):
        self.manifest()
        result = self.result()
        result.update(ok=False, error="exception 0xc0000005")
        smoke.collect_artifacts(self.folder, result)
        self.assertFalse(result["ok"])
        self.assertEqual(len(result["shots"]), 1)
        text = (self.folder / "index.md").read_text(encoding="utf-8")
        self.assertIn("shots/01-main.bmp", text)
        self.assertIn("Black: 50.000000%", text)
        self.assertIn("exception 0xc0000005", text)

    def process(self, code, **kwargs):
        return smoke.run_process([sys.executable, "-I", "-u", "-c", code], "menus", self.folder,
                                 os.environ.copy(), self.log, kwargs.pop("timeout", 3), **kwargs)

    def test_process_success_requires_both_markers(self):
        self.assertTrue(self.process("print('SMOKE READY menus'); print('SMOKE PASS menus')")["ok"])
        self.assertFalse(self.process("print('SMOKE READY menus')")["ok"])
        self.assertFalse(self.process("print('SMOKE PASS menus')")["ok"])

    def test_nonzero_exit_and_explicit_failure(self):
        self.assertFalse(self.process("print('SMOKE READY menus'); print('SMOKE PASS menus'); raise SystemExit(7)")["ok"])
        result = self.process("print('SMOKE READY menus'); print('SMOKE FAIL menus: no sound'); print('SMOKE PASS menus')")
        self.assertFalse(result["ok"])
        self.assertIn("no sound", result["error"])

    def test_process_timeout_and_missing_driver(self):
        result = self.process("import time; print('SMOKE READY menus'); print('SMOKE STAGE world load'); time.sleep(10)", timeout=0.4)
        self.assertTrue(result["timeout"])
        self.assertEqual(result["stage"], "world load")
        result = self.process("import time; time.sleep(10)", ready_seconds=0.2)
        self.assertIn("no READY", result["error"])

    def test_timeout_marker_fails_even_with_exit_zero(self):
        result = self.process("print('SMOKE READY menus'); print('SMOKE TIMEOUT spawn'); print('SMOKE PASS menus')")
        self.assertTrue(result["timeout"])
        self.assertFalse(result["ok"])
        self.assertEqual(result["stage"], "spawn")

    def test_last_error_keeps_crash_module(self):
        self.assertEqual(smoke.last_error("exception module: ig8icd32.dll+0x10\nexception 0xc0000005"),
                         "ig8icd32.dll+0x10 | exception 0xc0000005")

    def test_restore_mutable_inputs(self):
        building = self.folder / "data/building"
        building.mkdir(parents=True)
        config = self.folder / "config.txt"
        config.write_bytes(b"private")
        saved = building / "one.sbb"
        saved.write_bytes(b"before")
        created = building / "two.sbb"
        with self.assertRaises(RuntimeError):
            with smoke.preserve_inputs(self.folder):
                config.write_bytes(b"changed")
                saved.unlink()
                created.write_bytes(b"new")
                raise RuntimeError("test")
        self.assertEqual(config.read_bytes(), b"private")
        self.assertEqual(saved.read_bytes(), b"before")
        self.assertFalse(created.exists())

    def test_comparison_warns_without_failure(self):
        self.manifest()
        a = self.result()
        smoke.collect_artifacts(self.folder, a)
        other = self.folder / "second"
        (other / "shots").mkdir(parents=True)
        bitmap(other / "shots/01-main.bmp", [[(0, 0, 0), (0, 255, 0)]])
        b = dict(a, index=str(other / "index.md"))
        comparison = smoke.compare_results([a], [b])
        self.assertEqual(comparison[0]["different_percent"], 50)
        self.assertTrue(a["ok"])
        self.assertIn("50.000000%", a["comparison"][0])
        b["shots"] = []
        smoke.compare_results([a], [b])
        self.assertIn("missing counterpart", a["comparison"][0])

    def test_comparison_matches_names_after_sequence_change(self):
        self.manifest()
        a = self.result()
        smoke.collect_artifacts(self.folder, a)
        other = self.folder / "second"
        (other / "shots").mkdir(parents=True)
        bitmap(other / "shots/07-main.bmp", [[(0, 0, 0), (255, 0, 0)]])
        b = dict(a, index=str(other / "index.md"), shots=[dict(a["shots"][0], path="shots/07-main.bmp")])
        comparison = smoke.compare_results([a], [b])
        self.assertEqual(comparison[0]["different_percent"], 0)
        self.assertEqual(comparison[0]["compare_path"], "shots/07-main.bmp")

    def test_specific_error_precedes_shutdown_error(self):
        text = "SMOKE FAIL play: no sound\nSMOKE FAIL play: incomplete run\n"
        self.assertEqual(smoke.last_error(text), "SMOKE FAIL play: no sound")

    def test_window_lock_rejects_second_executor(self):
        with patch.object(smoke, "ROOT", self.folder), smoke.client_lock():
            with self.assertRaises(RuntimeError):
                with smoke.client_lock():
                    self.fail("second executor entered")
    def test_invalid_timeout_rejected_before_launch(self):
        for value in ["0", "-1", "nan", "inf"]:
            with self.subTest(value=value), self.assertRaises(SystemExit) as context:
                smoke.main(["absent.exe", "--timeout", value])
            self.assertEqual(context.exception.code, 2)

    def test_suite_passes_capture_points_and_clears_old_artifacts(self):
        exe = self.folder / "client.exe"
        exe.write_bytes(b"SR_SMOKE")
        output = self.folder / "output"
        directory = output / "menus"
        (directory / "shots").mkdir(parents=True)
        stale = directory / "shots/99-stale.bmp"
        stale.write_bytes(b"stale")

        def fake_process(command, scenario, cwd, env, log, timeout):
            self.assertEqual(env["SR_SMOKE_SHOTS"], "main")
            self.assertEqual(Path(env["SR_SMOKE_OUTPUT"]), directory)
            self.assertFalse(stale.exists())
            bitmap(directory / "shots/01-main.bmp", [[(0, 0, 0), (255, 0, 0)]])
            (directory / "shots.jsonl").write_text('{"name":"main","path":"shots/01-main.bmp"}\n', encoding="utf-8")
            log.write_text("SMOKE READY menus\nSMOKE PASS menus\n", encoding="utf-8")
            return dict(self.result(), log=str(log), timeout=False)

        with patch.object(smoke, "ROOT", self.folder), patch.object(smoke, "run_process", side_effect=fake_process):
            results = smoke.run_suite(exe, ["menus"], output, None, "main")
        self.assertTrue(results[0]["ok"])
        self.assertTrue((output / "summary.json").exists())


if __name__ == "__main__":
    unittest.main()