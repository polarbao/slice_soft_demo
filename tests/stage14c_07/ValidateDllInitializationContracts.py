#!/usr/bin/env python3

import argparse
import re
from pathlib import Path


EXPECTED_EXPORTS = [
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
]

FORBIDDEN_DEPENDENCIES = [
    b"printsdk",
    b"qt5",
    b"qt6",
    b"slicer_engine",
    b"openvdb",
    b"libtiff",
]


def Read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def ParseDefExports(content: str) -> list[str]:
    lines = [line.strip() for line in content.splitlines()]
    exportIndex = lines.index("EXPORTS")
    return [line.split()[0] for line in lines[exportIndex + 1 :] if line]


def Require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def ValidateSource(repoRoot: Path) -> None:
    dllMain = Read(repoRoot / "src/slicer_module/DllMain.cpp")
    initialization = Read(repoRoot / "src/slicer_module/ModuleInitialization.h")
    initialization += Read(repoRoot / "src/slicer_module/ModuleInitialization.cpp")
    exports = Read(repoRoot / "src/slicer_module/Exports.cpp")
    moduleDef = Read(repoRoot / "src/slicer_module/slicer_module.def")
    cmake = Read(repoRoot / "CMakeLists.txt")

    Require(dllMain.count("return TRUE;") == 1, "DllMain must only return TRUE")
    for forbidden in (
        "DisableThreadLibraryCalls",
        "std::call_once",
        "CreateThread",
        "LoadLibrary",
        "CreateFile",
        "getenv",
        "fstream",
        "filesystem",
    ):
        Require(forbidden not in dllMain, f"DllMain contains forbidden token {forbidden}")

    Require("std::once_flag" in initialization, "ModuleInitialization lacks once_flag")
    Require("std::call_once" in initialization, "ModuleInitialization lacks call_once")
    for forbidden in (
        "Qt",
        "PrintSDK",
        "WorkerClient",
        "slicer_engine",
        "OpenVDB",
        "fstream",
        "filesystem",
        "LoadLibrary",
        "CreateThread",
    ):
        Require(
            forbidden not in initialization,
            f"ModuleInitialization contains forbidden token {forbidden}",
        )

    createStart = exports.index("pm_create")
    createEnd = exports.index("pm_destroy", createStart)
    createBody = exports[createStart:createEnd]
    Require(
        exports.count("EnsureProcessModuleInitialized()") == 1,
        "process initialization must only be requested by pm_create",
    )
    Require(
        "EnsureProcessModuleInitialized()" in createBody,
        "pm_create does not invoke process initialization",
    )
    Require(
        ParseDefExports(moduleDef) == EXPECTED_EXPORTS,
        "the frozen 11-symbol export surface changed",
    )

    linkBlock = re.search(
        r"target_link_libraries\(slicer_module\s+PRIVATE([\s\S]*?)\)", cmake
    )
    Require(linkBlock is not None, "slicer_module link block is missing")
    Require("slicer_base" in linkBlock.group(1), "slicer_module lost slicer_base")
    for forbidden in ("Qt", "PrintSDK", "slicer_engine", "OpenVDB", "TIFF"):
        Require(
            forbidden not in linkBlock.group(1),
            f"slicer_module links forbidden dependency {forbidden}",
        )
    Require("/DELAYLOAD" not in linkBlock.group(1), "forbidden delay-load detected")


def ValidateBinary(library: Path) -> None:
    Require(library.is_file(), f"module binary not found: {library}")
    binary = library.read_bytes().lower()
    for dependency in FORBIDDEN_DEPENDENCIES:
        Require(
            dependency not in binary,
            f"module binary contains forbidden dependency token {dependency.decode()}",
        )


def Main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--library", type=Path, required=True)
    arguments = parser.parse_args()
    ValidateSource(arguments.repo.resolve())
    ValidateBinary(arguments.library.resolve())
    print("Stage 14C-07 DLL initialization contracts: PASS")


if __name__ == "__main__":
    Main()
