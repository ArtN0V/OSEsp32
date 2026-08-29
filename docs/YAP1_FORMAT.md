# YAP1 package format

Status: frozen for the Stage 4 parser/packer vertical slice. All integers are
unsigned little-endian. Offsets are absolute from the start of the file. A
`.yap` file is an application package, not native ESP32 executable code.

## Limits

- Maximum package size: 8 MiB.
- Section count: 2–16.
- Every payload offset is four-byte aligned.
- Exactly one `MANF` and one `LUAS` section are required.
- At most one `ICON` section is allowed; `RSRC` may repeat.
- Unknown section types or non-zero flags are rejected in version 1.
- CRC32 is the IEEE/zlib variant.

## Header — 32 bytes

| Offset | Size | Field | Version 1 value |
|---:|---:|---|---|
| 0 | 4 | Magic | `YAP1` |
| 4 | 2 | Format version | `1` |
| 6 | 2 | Header size | `32` |
| 8 | 4 | Exact file size | bytes |
| 12 | 4 | Section-table offset | `32` |
| 16 | 2 | Section count | 2–16 |
| 18 | 2 | Manifest section index | valid `MANF` index |
| 20 | 4 | Package CRC32 | whole file with bytes 20–23 treated as zero |
| 24 | 4 | Flags | `0` |
| 28 | 4 | Reserved | `0` |

## Section entry — 20 bytes

Each entry contains `type`, `offset`, `length`, `CRC32` and `flags`, all
32-bit. `flags` is zero in version 1. Payloads cannot overlap the header,
section table or another payload.

Known FourCC types:

- `MANF` — fixed manifest below;
- `LUAS` — UTF-8 Lua source (bytecode is deliberately not accepted);
- `ICON` — optional package icon, decoding contract comes with installation;
- `RSRC` — opaque read-only resource; the named resource index is added before
  the `app:/` API is enabled.

## MANF payload — 160 bytes

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | `MNF1` |
| 4 | 2 | Structure size, `160` |
| 6 | 1 | Manifest version, `1` |
| 7 | 1 | Requested mode: 0 windowed, 1 fullscreen, 2 exclusive |
| 8 | 1 | API major, currently `1` |
| 9 | 1 | API minor |
| 10 | 1 | File-association count, 0–4 |
| 11 | 1 | Reserved, zero |
| 12 | 4 | Requested Lua heap, 16–96 KiB |
| 16 | 4 | Capability bit mask |
| 20 | 2 | `LUAS` section index |
| 22 | 2 | `ICON` index or `0xFFFF` |
| 24 | 32 | NUL-terminated application ID |
| 56 | 48 | NUL-terminated UTF-8 display name |
| 104 | 24 | NUL-terminated Lua entry function |
| 128 | 32 | Four 8-byte NUL-terminated extensions without a dot |

Application IDs use lowercase ASCII letters, digits, `.`, `_` and `-`, begin
with an alphanumeric character and do not end with a dot. Entry points use a
Lua-compatible ASCII identifier. Extensions use lowercase ASCII letters and
digits. Strings must terminate inside their field; controls, malformed UTF-8,
overlong UTF-8, surrogate values and out-of-range code points are rejected.

Capability bits:

| Bit | Name |
|---:|---|
| 0 | `storage.private.read` |
| 1 | `storage.private.write` |
| 2 | `documents.open` |
| 3 | `documents.create` |
| 4 | `documents.replace` |

Capabilities are requests only. A manifest never grants ambient SD access.
Associations require `documents.open` or `documents.create`; they are merely
candidates for a later system-owned **Open with** choice.

## Validation order

The OS validates path/size, header, table bounds, package CRC, every section's
bounds/overlap/type/CRC, and finally the manifest. Lua source is not read or
executed when any earlier check fails. Current firmware stops after validation
and displays package information; the quota-controlled Lua runtime is the next
Stage 4 package.
