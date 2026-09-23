#!/usr/bin/env python3
"""Create, validate, build and inspect OSEsp32 YAP projects."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import tempfile
from pathlib import Path

import yap_pack


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "templates" / "yap_app"


def project_files(project: Path) -> tuple[Path, Path]:
    project = project.resolve()
    if not project.is_dir():
        raise ValueError(f"project directory does not exist: {project}")
    manifest = project / "manifest.json"
    source = project / "main.lua"
    if not manifest.is_file():
        raise ValueError(f"missing project manifest: {manifest}")
    if not source.is_file():
        raise ValueError(f"missing project source: {source}")
    return manifest, source


def load_config(manifest: Path) -> dict:
    config = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(config, dict):
        raise ValueError("manifest root must be a JSON object")
    return config


def print_summary(config: dict, source: Path, package: Path | None = None) -> None:
    capabilities = config.get("capabilities", [])
    associations = config.get("associations", [])
    resources = config.get("resources", {})
    print(f"id: {config.get('id', '<missing>')}")
    print(f"name: {config.get('name', '<missing>')}")
    print(f"api: 1.{config.get('api_minor', 0)}")
    print(f"mode: {config.get('mode', 'windowed')}")
    print(f"lua quota: {config.get('memory', 32768)} bytes")
    print("capabilities: " + (", ".join(capabilities) if capabilities else "none"))
    print("associations: " + (", ".join(associations) if associations else "none"))
    print(f"resources: {len(resources) if isinstance(resources, dict) else 'invalid'}")
    print(f"source: {source.stat().st_size} bytes")
    if package is not None:
        print(f"package: {package} ({package.stat().st_size} bytes)")


def new_project(target: Path, app_id: str, name: str | None,
                mode: str, memory: int) -> None:
    # Validate identity and main manifest constraints before creating anything.
    display_name = name or target.name.replace("_", " ").replace("-", " ").strip()
    if not display_name:
        raise ValueError("display name cannot be empty")
    config = {
        "id": app_id,
        "name": display_name,
        "api_minor": 1,
        "entry": "main",
        "mode": mode,
        "memory": memory,
        "capabilities": [],
        "associations": [],
        "resources": {"welcome.txt": "assets/welcome.txt"},
    }
    yap_pack.build_manifest(config, 1)
    if target.exists():
        raise FileExistsError(f"target already exists; nothing overwritten: {target}")
    if not TEMPLATE.is_dir():
        raise ValueError(f"SDK template is missing: {TEMPLATE}")
    shutil.copytree(TEMPLATE, target)
    (target / "manifest.json").write_text(
        json.dumps(config, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    readme = target / "README.md"
    readme.write_text(
        readme.read_text(encoding="utf-8").replace("__APP_NAME__", display_name),
        encoding="utf-8",
    )
    print(f"created {target}")
    print(f"next: python tools/yap.py check {target}")


def check_project(project: Path) -> None:
    manifest, source = project_files(project)
    config = load_config(manifest)
    with tempfile.TemporaryDirectory(prefix="osesp32-yap-check-") as temporary:
        package = Path(temporary) / "check.yap"
        yap_pack.pack(manifest, source, package, quiet=True)
        yap_pack.inspect(package, quiet=True)
        print_summary(config, source, package)
    print("check: ok")


def build_project(project: Path, output: Path | None) -> None:
    manifest, source = project_files(project)
    config = load_config(manifest)
    destination = output or project.resolve() / "build" / f"{project.name}.yap"
    yap_pack.pack(manifest, source, destination, quiet=True)
    print_summary(config, source, destination)
    print("build: ok")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    create = commands.add_parser("new", help="create a project from the SDK template")
    create.add_argument("target", type=Path, help="new project directory")
    create.add_argument("--id", required=True, dest="app_id",
                        help="stable lowercase application ID, e.g. org.example.calc")
    create.add_argument("--name", help="display name; defaults to directory name")
    create.add_argument("--mode", choices=yap_pack.MODES, default="windowed")
    create.add_argument("--memory", type=int, default=32768,
                        help="Lua quota in bytes (16384..98304)")

    check = commands.add_parser("check", help="validate a project without keeping a package")
    check.add_argument("project", type=Path)

    build = commands.add_parser("build", help="validate and create a .yap package")
    build.add_argument("project", type=Path)
    build.add_argument("-o", "--output", type=Path,
                       help="output path; default is PROJECT/build/PROJECT.yap")

    inspect = commands.add_parser("inspect", help="validate and describe a .yap package")
    inspect.add_argument("package", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "new":
            new_project(args.target, args.app_id, args.name, args.mode, args.memory)
        elif args.command == "check":
            check_project(args.project)
        elif args.command == "build":
            build_project(args.project, args.output)
        else:
            yap_pack.inspect(args.package)
    except (OSError, UnicodeError, ValueError, KeyError, TypeError,
            json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
