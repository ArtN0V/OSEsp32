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
    if not encoded or len(encoded) >= width or any(byte < 0x20 or byte==127 for byte in encoded):
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
    if int(config.get("api_minor",0)) not in (0,1,2):
        raise ValueError("unsupported API minor")
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


def resource_name(name: str) -> bytes:
    encoded = fixed(name,64)
    if any(char in '\\:*?"<>|' for char in name):
        raise ValueError("invalid resource path")
    for part in name.split('/'):
        if not part or len(part.encode('utf-8'))>48 or part[-1] in '. ':
            raise ValueError("invalid resource path")
    return encoded


def pack(manifest_path: Path, lua_path: Path, output_path: Path,
         *, quiet: bool = False) -> None:
    config = json.loads(manifest_path.read_text(encoding="utf-8"))
    lua_source = lua_path.read_bytes()
    if not lua_source or len(lua_source)>65536:
        raise ValueError("Lua source must contain 1..65536 bytes")
    lua_source.decode("utf-8")
    manifest = build_manifest(config, code_index=1)
    sections = [(b"MANF", manifest), (b"LUAS", lua_source)]
    resources=config.get("resources",{})
    if not isinstance(resources,dict):
        raise ValueError("resources must be an object mapping package names to files")
    names=set()
    for name, filename in resources.items():
        if not isinstance(name,str) or not isinstance(filename,str):
            raise ValueError("resource names and source files must be strings")
        if name.casefold() in names:
            raise ValueError("duplicate resource name")
        names.add(name.casefold())
        sections.append((b"RSRC",resource_name(name)+(manifest_path.parent / filename).read_bytes()))
    if len(sections)>16:
        raise ValueError("at most 14 resources are allowed")
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
    if not quiet:
        print(f"packed {output_path} ({len(package)} bytes)")


def c_string(field: bytes) -> str:
    if b"\0" not in field:
        raise ValueError("unterminated fixed string")
    value,padding=field.split(b"\0",1)
    if any(padding):
        raise ValueError("non-zero fixed string padding")
    return value.decode("utf-8")


def inspect(path: Path, *, quiet: bool = False) -> None:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE or len(data)>MAX_PACKAGE_SIZE:
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
    bounds=[]
    table_end=table_offset+section_count*SECTION_SIZE
    if table_end>len(data):
        raise ValueError("truncated section table")
    resource_names=set()
    for index in range(section_count):
        entry = struct.unpack_from("<4sIIII", data, table_offset + index * SECTION_SIZE)
        section_type, offset, length, expected_section_crc, section_flags = entry
        payload = data[offset:offset + length]
        if (section_flags or offset % 4 or not length or offset<table_end or len(payload) != length
                or zlib.crc32(payload) != expected_section_crc):
            raise ValueError(f"invalid section {index}")
        if section_type not in (b'MANF',b'LUAS',b'ICON',b'RSRC'):
            raise ValueError("unknown section")
        if any(offset<end and start<offset+length for start,end in bounds):
            raise ValueError("overlapping sections")
        bounds.append((offset,offset+length))
        if section_type==b'RSRC':
            if length<64:
                raise ValueError("short resource header")
            name=c_string(payload[:64]); resource_name(name)
            if name.casefold() in resource_names:
                raise ValueError("duplicate resource")
            resource_names.add(name.casefold())
        sections.append((section_type, payload))
    types=[kind for kind,_ in sections]
    if types.count(b'MANF')!=1 or types.count(b'LUAS')!=1 or types.count(b'ICON')>1:
        raise ValueError("duplicate/missing mandatory section")
    section_type, manifest = sections[manifest_index]
    if section_type != b"MANF" or len(manifest) != MANIFEST_SIZE:
        raise ValueError("invalid manifest section")
    magic,size,runtime,mode,major,minor,count,reserved,memory,caps,code,icon=struct.unpack_from('<4sHBBBBBBIIHH',manifest)
    if (magic!=b'MNF1' or size!=160 or runtime!=1 or mode>2 or major!=1 or minor>2 or count>4 or reserved
            or caps & ~31 or code>=section_count or types[code]!=b'LUAS'
            or (icon!=65535 and (icon>=section_count or types[icon]!=b'ICON'))):
        raise ValueError("invalid manifest fields")
    config={"id":c_string(manifest[24:56]),"name":c_string(manifest[56:104]),
            "entry":c_string(manifest[104:128]),"memory":memory,"mode":list(MODES)[mode],"api_minor":minor,
            "capabilities":[name for name,bit in CAPABILITIES.items() if caps&bit],
            "associations":[c_string(manifest[128+i*8:136+i*8]) for i in range(count)]}
    build_manifest(config,code)
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
