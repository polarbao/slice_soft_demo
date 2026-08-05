#!/usr/bin/env python3

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


def ReadText(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def ParseDefExports(content: str) -> list[str]:
    lines = [line.strip() for line in content.splitlines()]
    try:
        exportIndex = lines.index("EXPORTS")
    except ValueError as error:
        raise AssertionError("slicer_module.def lacks EXPORTS") from error
    return [line.split()[0] for line in lines[exportIndex + 1 :] if line]


def Validate() -> None:
    repoRoot = Path(__file__).resolve().parents[2]
    source = ReadText(repoRoot / "src/slicer_module/Exports.cpp")
    moduleDef = ReadText(repoRoot / "src/slicer_module/slicer_module.def")
    cmake = ReadText(repoRoot / "CMakeLists.txt")

    exported = ParseDefExports(moduleDef)
    assert exported == EXPECTED_EXPORTS, (
        f"expected exact ordered exports {EXPECTED_EXPORTS}, got {exported}"
    )

    definitions = re.findall(
        r'extern\s+"C"\s+PM_API[\s\S]*?PM_CALL\s+(pm_[a-z_]+)\s*\(',
        source,
    )
    assert definitions == EXPECTED_EXPORTS, (
        f"expected exact ordered definitions {EXPECTED_EXPORTS}, got {definitions}"
    )
    assert "PM_MODULE_BUILD_SHARED" in cmake
    assert re.search(r"add_library\(slicer_module\s+SHARED", cmake)

    targetBlock = re.search(
        r"target_link_libraries\(\s*slicer_module[\s\S]*?\)",
        cmake,
    )
    assert targetBlock is not None
    assert "slicer_base" in targetBlock.group(0)
    assert "slicer_engine" not in targetBlock.group(0)
    assert "slicer_core" not in targetBlock.group(0)


if __name__ == "__main__":
    Validate()
    print("Stage 14C-01 module shell contract: PASS")
