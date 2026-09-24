import importlib.util
import tempfile
import unittest
import json
import struct
import zlib
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

    def test_all_runtime_examples_pack(self):
        sources = [ROOT / "examples" / "hello_yap" / "main.lua"]
        sources.extend(
            sorted((ROOT / "examples" / "yap_runtime_tests").glob("*.lua"))
        )
        with tempfile.TemporaryDirectory() as temporary:
            for source in sources:
                package = Path(temporary) / f"{source.stem}.yap"
                YAP.pack(
                    ROOT / "examples" / "hello_yap" / "manifest.json",
                    source,
                    package,
                )
                YAP.inspect(package, quiet=True)

    def test_api_minor_and_resource_paths(self):
        YAP.build_manifest({'id':'test','name':'Test','api_minor':3},1)
        with self.assertRaises(ValueError):
            YAP.build_manifest({'id':'test','name':'Test','api_minor':255},1)
        for name in ('../escape','/absolute','a//b','a/./b','bad\\name','a.','a ','x'*64):
            with self.subTest(name=name), self.assertRaises(ValueError):
                YAP.resource_name(name)
        YAP.resource_name('папка/ресурс.txt')

    def test_resources_and_new_examples(self):
        with tempfile.TemporaryDirectory() as temporary:
            for example in ('file_roundtrip_yap','document_info_yap','calculator_yap',
                            'canvas_probe_yap'):
                target=Path(temporary)/f'{example}.yap'
                YAP.pack(ROOT/'examples'/example/'manifest.json',ROOT/'examples'/example/'main.lua',target)
                YAP.inspect(target,quiet=True)

    def test_resource_map_type_is_checked(self):
        with tempfile.TemporaryDirectory() as temporary:
            config=Path(temporary)/'manifest.json'
            config.write_text('{"id":"test","name":"Test","resources":[]}',encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'resources must be'):
                YAP.pack(config,ROOT/'examples/hello_yap/main.lua',Path(temporary)/'bad.yap')

    def test_repaired_crc_does_not_bypass_structure_checks(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=self.make_package(Path(temporary))
            original=path.read_bytes()
            for case in ('overlap','length','unknown','api','padding'):
                data=bytearray(original)
                if case=='overlap': struct.pack_into('<I',data,56,struct.unpack_from('<I',data,36)[0])
                if case=='length': struct.pack_into('<I',data,40,0xffffffff)
                if case=='unknown': data[52:56]=b'WHAT'
                if case in ('api','padding'):
                    offset,length=struct.unpack_from('<II',data,36)
                    if case=='api': data[offset+9]=255
                    else: data[offset+55]=1
                    struct.pack_into('<I',data,44,zlib.crc32(data[offset:offset+length]))
                data[20:24]=bytes(4); struct.pack_into('<I',data,20,zlib.crc32(data))
                path.write_bytes(data)
                with self.subTest(case=case),self.assertRaises(ValueError): YAP.inspect(path,quiet=True)


if __name__ == "__main__":
    unittest.main()
