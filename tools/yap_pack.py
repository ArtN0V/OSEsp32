#!/usr/bin/env python3
"""Pack and inspect deterministic OSEsp32 YAP1 application files."""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

HEADER_SIZE = 32
SECTION_SIZE = 20
MANIFEST_SIZE = 160
MAX_PACKAGE_SIZE = 8 * 1024 * 1024

CAPABILITIES = {
    "storage.private.read": 1 << 0,
    "storage.private.write": 1 << 1,
    "documents.open": 1 << 2,
    "documents.create": 1 << 3,
    "documents.replace": 1 << 4,
}
MODES = {"windowed": 0, "fullscreen": 1, "exclusive": 2}


def fixed(value: str, width: int, *, ascii_only: bool = False) -> bytes:
    encoded = value.encode("ascii" if ascii_only else "utf-8")
    if not encoded or len(encoded) >= width or any(byte < 0x20 for byte in encoded):
        raise ValueError(f"value {value!r} does not fit a {width}-byte field")
    return encoded + bytes(width - len(encoded))


def align4(value: int) -> int:
    return (value + 3) & ~3


def validate_id(value: str) -> None:
    allowed = set("abcdefghijklmnopqrstuvwxyz0123456789._-")
    if (not value or not value[0].isalnum() or value[0] not in allowed
            or value[-1] == "." or any(char not in allowed for char in value)):
        raise ValueError("id must be a lowercase YAP application identifier")


def validate_identifier(value: str) -> None:
    if (not value or not (value[0].isalpha() or value[0] == "_")
            or not value.isascii()
            or any(not (char.isalnum() or char == "_") for char in value[1:])):
        raise ValueError("entry must be an ASCII Lua identifier")


def build_manifest(config: dict, code_index: int) -> bytes:
    app_id = str(config["id"])
    name = str(config["name"])
    entry = str(config.get("entry", "main"))
    validate_id(app_id)
    validate_identifier(entry)
    mode_name = str(config.get("mode", "windowed"))
    if mode_name not in MODES:
        raise ValueError(f"unsupported mode: {mode_name}")
    memory = int(config.get("memory", 32768))
    if not 16 * 1024 <= memory <= 96 * 1024:
        raise ValueError("memory must be between 16384 and 98304 bytes")
    capability_mask = 0
    for capability in config.get("capabilities", []):
        if capability not in CAPABILITIES:
            raise ValueError(f"unsupported capability: {capability}")
        capability_mask |= CAPABILITIES[capability]
    associations = list(config.get("associations", []))
    if len(associations) > 4:
        raise ValueError("at most four file associations are allowed")
    if associations and not capability_mask & (
        CAPABILITIES["documents.open"] | CAPABILITIES["documents.create"]
    ):
        raise ValueError("associations require documents.open or documents.create")
    association_bytes = bytearray(32)
    for index, extension in enumerate(associations):
        extension = str(extension)
        if (not extension or not extension.isascii() or not extension.isalnum()
                or extension.lower() != extension or len(extension) > 7):
            raise ValueError(f"invalid extension: {extension!r}")
        association_bytes[index * 8:(index + 1) * 8] = fixed(
            extension, 8, ascii_only=True
        )
    manifest = bytearray(MANIFEST_SIZE)
    struct.pack_into(
        "<4sHBBBBBBIIHH", manifest, 0, b"MNF1", MANIFEST_SIZE, 1,
        MODES[mode_name], 1, int(config.get("api_minor", 0)),
        len(associations), 0, memory, capability_mask, code_index, 0xFFFF,
    )
    manifest[24:56] = fixed(app_id, 32, ascii_only=True)
    manifest[56:104] = fixed(name, 48)
    manifest[104:128] = fixed(entry, 24, ascii_only=True)
    manifest[128:160] = association_bytes
    return bytes(manifest)


def pack(manifest_path: Path, lua_path: Path, output_path: Path) -> None:
    config = json.loads(manifest_path.read_text(encoding="utf-8"))
    lua_source = lua_path.read_bytes()
    if not lua_source:
        raise ValueError("Lua source is empty")
    lua_source.decode("utf-8")
    manifest = build_manifest(config, code_index=1)
    sections = [(b"MANF", manifest), (b"LUAS", lua_source)]
    table_end = HEADER_SIZE + len(sections) * SECTION_SIZE
    cursor = align4(table_end)
    entries: list[tuple[bytes, int, bytes]] = []
    for section_type, payload in sections:
        cursor = align4(cursor)
        entries.append((section_type, cursor, payload))
        cursor += len(payload)
    if cursor > MAX_PACKAGE_SIZE:
        raise ValueError("package exceeds the YAP1 size limit")
    package = bytearray(cursor)
    struct.pack_into(
        "<4sHHIIHHIII", package, 0, b"YAP1", 1, HEADER_SIZE, cursor,
        HEADER_SIZE, len(entries), 0, 0, 0, 0,
    )
    for index, (section_type, offset, payload) in enumerate(entries):
        struct.pack_into(
            "<4sIIII", package, HEADER_SIZE + index * SECTION_SIZE,
            section_type, offset, len(payload), zlib.crc32(payload), 0,
        )
        package[offset:offset + len(payload)] = payload
    struct.pack_into("<I", package, 20, zlib.crc32(package))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(package)
    inspect(output_path, quiet=True)
    print(f"packed {output_path} ({len(package)} bytes)")


def c_string(field: bytes) -> str:
    if b"\0" not in field:
        raise ValueError("unterminated fixed string")
    return field.split(b"\0", 1)[0].decode("utf-8")


def inspect(path: Path, *, quiet: bool = False) -> None:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise ValueError("truncated header")
    (magic, version, header_size, declared_size, table_offset, section_count,
     manifest_index, expected_crc, flags, reserved) = struct.unpack_from(
        "<4sHHIIHHIII", data, 0
    )
    if (magic != b"YAP1" or version != 1 or header_size != HEADER_SIZE
            or declared_size != len(data) or table_offset != HEADER_SIZE
            or not 2 <= section_count <= 16 or manifest_index >= section_count
            or flags or reserved):
        raise ValueError("invalid YAP1 header")
    crc_data = bytearray(data)
    crc_data[20:24] = bytes(4)
    if zlib.crc32(crc_data) != expected_crc:
        raise ValueError("package CRC mismatch")
    sections = []
    for index in range(section_count):
        entry = struct.unpack_from("<4sIIII", data, table_offset + index * SECTION_SIZE)
        section_type, offset, length, expected_section_crc, section_flags = entry
        payload = data[offset:offset + length]
        if (section_flags or offset % 4 or len(payload) != length
                or zlib.crc32(payload) != expected_section_crc):
            raise ValueError(f"invalid section {index}")
        sections.append((section_type, payload))
    section_type, manifest = sections[manifest_index]
    if section_type != b"MANF" or len(manifest) != MANIFEST_SIZE:
        raise ValueError("invalid manifest section")
    if not quiet:
        print(f"name: {c_string(manifest[56:104])}")
        print(f"id: {c_string(manifest[24:56])}")
        print(f"sections: {section_count}; bytes: {len(data)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    pack_parser = subparsers.add_parser("pack")
    pack_parser.add_argument("manifest", type=Path)
    pack_parser.add_argument("lua", type=Path)
    pack_parser.add_argument("-o", "--output", required=True, type=Path)
    inspect_parser = subparsers.add_parser("inspect")
    inspect_parser.add_argument("package", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.command == "pack":
            pack(arguments.manifest, arguments.lua, arguments.output)
        else:
            inspect(arguments.package)
    except (OSError, UnicodeError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
