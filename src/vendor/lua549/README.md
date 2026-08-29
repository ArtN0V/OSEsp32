# Generated Lua dependency

The pinned sources are included in the project. To reproduce or refresh them,
run:

```text
python3 tools/install_lua.py
```

On Windows PowerShell, `py -3 tools\install_lua.py` is usually the equivalent.
The installer downloads official Lua 5.4.9, verifies its pinned SHA-256 and
copies only the core plus the explicitly allowed standard-library sources into
this directory. It excludes `io`, `os`, `package`, `debug`, dynamic loading,
`lua`, `luac` and `linit`.
