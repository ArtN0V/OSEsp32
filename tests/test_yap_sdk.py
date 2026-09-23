import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "tools" / "yap.py"


class YapSdkTests(unittest.TestCase):
    def run_sdk(self, *arguments: str, ok: bool = True) -> subprocess.CompletedProcess:
        environment = dict(os.environ)
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        result = subprocess.run(
            [sys.executable, str(SDK), *map(str, arguments)],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        if ok and result.returncode:
            self.fail(f"SDK failed: {result.stderr}\n{result.stdout}")
        if not ok and not result.returncode:
            self.fail("SDK unexpectedly succeeded")
        return result

    def test_new_check_build_inspect_is_deterministic(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "my_app"
            created = self.run_sdk(
                "new", project, "--id", "org.example.myapp",
                "--name", "My App", "--mode", "fullscreen",
            )
            self.assertIn("created", created.stdout)
            config = (project / "manifest.json").read_text(encoding="utf-8")
            self.assertIn('"id": "org.example.myapp"', config)
            self.assertIn('"mode": "fullscreen"', config)
            checked = self.run_sdk("check", project)
            self.assertIn("check: ok", checked.stdout)
            self.assertIn("capabilities: none", checked.stdout)
            self.run_sdk("build", project)
            package = project / "build" / "my_app.yap"
            first = package.read_bytes()
            self.assertTrue(first.startswith(b"YAP1"))
            self.run_sdk("build", project)
            self.assertEqual(first, package.read_bytes())
            inspected = self.run_sdk("inspect", package)
            self.assertIn("name: My App", inspected.stdout)

    def test_new_never_overwrites_and_invalid_id_creates_nothing(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "existing"
            target.mkdir()
            marker = target / "keep.txt"
            marker.write_text("keep", encoding="utf-8")
            result = self.run_sdk(
                "new", target, "--id", "org.example.existing", ok=False
            )
            self.assertIn("nothing overwritten", result.stderr)
            self.assertEqual(marker.read_text(encoding="utf-8"), "keep")

            invalid = Path(temporary) / "invalid"
            self.run_sdk("new", invalid, "--id", "../escape", ok=False)
            self.assertFalse(invalid.exists())

    def test_build_accepts_explicit_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "app"
            output = Path(temporary) / "result" / "custom.yap"
            self.run_sdk("new", project, "--id", "org.example.output")
            self.run_sdk("build", project, "-o", output)
            self.assertTrue(output.is_file())


if __name__ == "__main__":
    unittest.main()
