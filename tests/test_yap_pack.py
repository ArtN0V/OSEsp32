import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "yap_pack", ROOT / "tools" / "yap_pack.py"
)
YAP = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(YAP)


class YapPackTests(unittest.TestCase):
    def make_package(self, directory: Path) -> Path:
        output = directory / "hello.yap"
        YAP.pack(
            ROOT / "examples" / "hello_yap" / "manifest.json",
            ROOT / "examples" / "hello_yap" / "main.lua",
            output,
        )
        return output

    def test_round_trip(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = self.make_package(Path(temporary))
            YAP.inspect(package, quiet=True)

    def test_corrupted_package_crc_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = self.make_package(Path(temporary))
            data = bytearray(package.read_bytes())
            data[-1] ^= 0x55
            package.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "package CRC"):
                YAP.inspect(package, quiet=True)

    def test_bad_magic_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = self.make_package(Path(temporary))
            data = bytearray(package.read_bytes())
            data[0:4] = b"NOPE"
            package.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "header"):
                YAP.inspect(package, quiet=True)

    def test_invalid_manifest_is_rejected_by_packer(self):
        with tempfile.TemporaryDirectory() as temporary:
            config = Path(temporary) / "manifest.json"
            config.write_text(
                '{"id":"../escape","name":"Bad","memory":32768}',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "identifier"):
                YAP.pack(
                    config,
                    ROOT / "examples" / "hello_yap" / "main.lua",
                    Path(temporary) / "bad.yap",
                )


if __name__ == "__main__":
    unittest.main()
