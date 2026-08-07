#!/usr/bin/env python3
"""Validate that the Stage 14E-01 host stays on the public C ABI."""

from __future__ import annotations

import argparse
from pathlib import Path


EXPORTS = (
    "pm_spi_version",
    "pm_module_info",
    "pm_create",
    "pm_destroy",
    "pm_submit",
    "pm_poll",
    "pm_cancel",
    "pm_result",
    "pm_release",
    "pm_self_test",
    "pm_last_error",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    arguments = parser.parse_args()
    root = arguments.repo_root.resolve()
    host = root / "apps" / "slicer_host_sim"
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    sources = sorted(host.glob("*"))
    if not sources or any(path.suffix not in {".c", ".h"} for path in sources):
        raise SystemExit("14E-01 host must contain only C sources and headers")
    combined = "\n".join(path.read_text(encoding="utf-8") for path in sources)
    forbidden = ("slicer_core/", "slicer_module/", "#include <Qt", "#include \"Qt")
    for token in forbidden:
        if token in combined:
            raise SystemExit(f"14E-01 host crossed a forbidden boundary: {token}")
    for export in EXPORTS:
        if f'"{export}"' not in combined:
            raise SystemExit(f"14E-01 host does not resolve {export}")
    start = cmake.find("add_executable(slicer_host_sim")
    end = cmake.find("add_executable(", start + 1)
    if start < 0:
        raise SystemExit("slicer_host_sim target is missing")
    target_block = cmake[start : end if end >= 0 else len(cmake)]
    for forbidden_target in ("slicer_core", "slicer_base", "slicer_engine", "slicer_module"):
        if f"PRIVATE {forbidden_target}" in target_block:
            raise SystemExit(f"slicer_host_sim links forbidden target {forbidden_target}")
    print("14E-01 pure C host boundary: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
