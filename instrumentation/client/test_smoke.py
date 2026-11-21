"""Check process completion, crash, timeout and input restoration rules."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest

SOURCE = Path(__file__).with_name("smoke.py")
SPEC = importlib.util.spec_from_file_location("client_smoke", SOURCE)
smoke = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(smoke)


class SmokeTests(unittest.TestCase):
    def setUp(self):
        folder = smoke.ROOT / "build/agent-k"
        folder.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="smoke12-unit-", dir=folder)
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def run_child(self, source, timeout=5, ready=2):
        return smoke.run_process([sys.executable, "-I", "-u", "-c", source], "menus", self.folder,
                                 os.environ.copy(), self.folder / "child.log", timeout, ready)

    def test_complete(self):
        result = self.run_child("print('SMOKE READY menus'); print('SMOKE PASS menus')")
        self.assertTrue(result["ok"])
        self.assertEqual(result["exit_code"], 0)

    def test_exit_without_completion(self):
        result = self.run_child("print('SMOKE READY menus')")
        self.assertFalse(result["ok"])
        self.assertIn("missing completion", result["error"])

    def test_crash_after_completion(self):
        result = self.run_child("print('SMOKE READY menus'); print('SMOKE PASS menus'); raise SystemExit(7)")
        self.assertFalse(result["ok"])
        self.assertEqual(result["exit_code"], 7)

    def test_failure_after_completion(self):
        result = self.run_child("print('SMOKE READY menus'); print('SMOKE PASS menus'); print('SMOKE FAIL menus: quit')")
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"], "SMOKE FAIL menus: quit")

    def test_timeout_stops_process(self):
        result = self.run_child("import time; print('SMOKE READY menus'); print('SMOKE STAGE editor'); time.sleep(20)", timeout=0.4)
        self.assertFalse(result["ok"])
        self.assertTrue(result["timeout"])
        self.assertEqual(result["stage"], "editor")
        self.assertIsNotNone(result["exit_code"])

    def test_missing_driver_stops_process(self):
        result = self.run_child("import time; time.sleep(20)", ready=0.4)
        self.assertFalse(result["ok"])
        self.assertIn("SR_SMOKE unavailable", result["error"])
        self.assertIsNotNone(result["exit_code"])

    def test_wrong_scenario_cannot_pass(self):
        result = self.run_child("print('SMOKE READY level'); print('SMOKE PASS level')")
        self.assertFalse(result["ok"])

    def test_restore_editor_files(self):
        data = self.folder / "data/building"
        data.mkdir(parents=True)
        building = data / "test.sbb"
        building.write_bytes(b"building input")
        config = self.folder / "config.txt"
        config.write_bytes(b"test settings")
        city = self.folder / "data/city.sbc"
        settings = self.folder / "Sub Rosa"
        settings.mkdir()
        account = settings / "client.srk"
        with self.assertRaises(RuntimeError):
            with smoke.preserve_inputs(self.folder):
                building.write_bytes(b"saved building")
                config.write_bytes(b"saved settings")
                city.write_bytes(b"saved city")
                account.write_bytes(b"saved account")
                raise RuntimeError("test failure")
        self.assertEqual(building.read_bytes(), b"building input")
        self.assertEqual(config.read_bytes(), b"test settings")
        self.assertFalse(city.exists())
        self.assertFalse(account.exists())

    def test_crash_module_in_summary(self):
        source = "print('SMOKE READY menus'); print('exception module: ig8icd32.dll+0x123'); print('exception 0xc0000005, read of 00000024'); raise SystemExit(7)"
        result = self.run_child(source)
        self.assertFalse(result["ok"])
        self.assertIn("ig8icd32.dll", result["error"])
        self.assertIn("read of 00000024", result["error"])

    def test_internal_timeout_uses_failure_stage(self):
        result = self.run_child("print('SMOKE READY menus'); print('SMOKE TIMEOUT editor'); print('SMOKE FAIL menus: editor'); print('SMOKE STAGE quit')")
        self.assertFalse(result["ok"])
        self.assertTrue(result["timeout"])
        self.assertEqual(result["stage"], "editor")


if __name__ == "__main__":
    unittest.main()
