import argparse
import pathlib
import json
import re
import struct
import sys


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


def Fail(message):
    print(f"14E-02 Qt host boundary: FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def ReadSources(appDirectory):
    paths = sorted(appDirectory.rglob("*"))
    sourcePaths = [
        path for path in paths if path.suffix.lower() in {".cpp", ".h", ".txt"}
        or path.name == "CMakeLists.txt"
    ]
    return sourcePaths, "\n".join(
        path.read_text(encoding="utf-8") for path in sourcePaths
    )


def ValidateSources(repoRoot):
    appDirectory = repoRoot / "apps" / "slicer_ui_host_sim"
    if not appDirectory.is_dir():
        Fail("apps/slicer_ui_host_sim is missing")

    sourcePaths, sourceText = ReadSources(appDirectory)
    implementationText = "\n".join(
        path.read_text(encoding="utf-8")
        for path in sourcePaths
        if path.name != "CMakeLists.txt"
    )
    forbidden = (
        '#include "slicer_core/',
        "#include <slicer_core/",
        "slicer_core",
        "slicer_base",
        "slicer_engine",
    )
    for token in forbidden:
        if token in implementationText:
            Fail(f"forbidden internal dependency found: {token}")

    cmakeText = (appDirectory / "CMakeLists.txt").read_text(encoding="utf-8")
    productionLinks = re.search(
        r"target_link_libraries\(slicer_ui_host_sim\s+PRIVATE(?P<body>.*?)\)",
        cmakeText,
        re.DOTALL,
    )
    if productionLinks is None:
        Fail("production host target_link_libraries block is missing")
    for token in ("slicer_module", "slicer_core", "slicer_base", "slicer_engine"):
        if re.search(rf"\b{token}\b", productionLinks.group("body")):
            Fail(f"production host links a forbidden dependency: {token}")

    for required in ("LoadLibraryW", "GetProcAddress", "Qt5::Widgets"):
        if required not in sourceText:
            Fail(f"runtime host marker missing: {required}")
    for exportName in EXPORTS:
        if f'"{exportName}"' not in sourceText:
            Fail(f"frozen export is not resolved: {exportName}")

    ledgerPath = pathlib.Path(__file__).with_name("HostSourceSizeDebtLedger.json")
    ledger = json.loads(ledgerPath.read_text(encoding="utf-8"))
    maxNewLines = int(ledger["maxLinesForNewSource"])
    knownOversized = ledger["knownOversized"]
    for path in sourcePaths:
        if path.suffix.lower() not in {".cpp", ".h"}:
            continue
        lineCount = len(path.read_text(encoding="utf-8").splitlines())
        recorded = knownOversized.get(path.name)
        if recorded is None:
            if lineCount > maxNewLines:
                Fail(
                    "host source exceeds "
                    f"{maxNewLines} lines and is not in the debt ledger: "
                    f"{path} ({lineCount})"
                )
            continue
        # 台账内的既有超限文件只允许缩减，不允许再增长。
        if lineCount > int(recorded):
            Fail(
                "host source in the debt ledger grew: "
                f"{path} ({lineCount} > recorded {recorded}); "
                "shrink it or update the ledger with a justification"
            )


def ReadCString(data, offset):
    end = data.find(b"\0", offset)
    if end < 0:
        Fail("unterminated PE import name")
    return data[offset:end].decode("ascii", errors="replace").lower()


def ParsePeImports(binaryPath):
    data = binaryPath.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        Fail("host binary is not a PE image")

    peOffset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[peOffset:peOffset + 4] != b"PE\0\0":
        Fail("host binary has an invalid PE signature")

    fileHeaderOffset = peOffset + 4
    sectionCount = struct.unpack_from("<H", data, fileHeaderOffset + 2)[0]
    optionalSize = struct.unpack_from("<H", data, fileHeaderOffset + 16)[0]
    optionalOffset = fileHeaderOffset + 20
    magic = struct.unpack_from("<H", data, optionalOffset)[0]
    if magic == 0x20B:
        dataDirectoryOffset = optionalOffset + 112
    elif magic == 0x10B:
        dataDirectoryOffset = optionalOffset + 96
    else:
        Fail("host binary has an unsupported PE optional header")

    importRva, _ = struct.unpack_from(
        "<II", data, dataDirectoryOffset + 8
    )
    sectionOffset = optionalOffset + optionalSize
    sections = []
    for index in range(sectionCount):
        headerOffset = sectionOffset + index * 40
        virtualSize, virtualAddress, rawSize, rawOffset = struct.unpack_from(
            "<IIII", data, headerOffset + 8
        )
        sections.append(
            (virtualAddress, max(virtualSize, rawSize), rawOffset)
        )

    def RvaToOffset(rva):
        for virtualAddress, span, rawOffset in sections:
            if virtualAddress <= rva < virtualAddress + span:
                return rawOffset + (rva - virtualAddress)
        Fail(f"PE RVA is outside all sections: {rva}")

    imports = []
    descriptorOffset = RvaToOffset(importRva)
    while True:
        descriptor = struct.unpack_from("<IIIII", data, descriptorOffset)
        if descriptor == (0, 0, 0, 0, 0):
            break
        imports.append(ReadCString(data, RvaToOffset(descriptor[3])))
        descriptorOffset += 20
    return imports


def ValidateBinary(binaryPath):
    if not binaryPath.is_file():
        Fail(f"host binary is missing: {binaryPath}")
    imports = ParsePeImports(binaryPath)
    forbiddenImports = [
        name for name in imports
        if "slicer_module" in name
        or "slicer_core" in name
        or "slicer_engine" in name
        or "slicer_base" in name
    ]
    if forbiddenImports:
        Fail(f"internal/module DLL leaked into import table: {forbiddenImports}")


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--binary")
    arguments = parser.parse_args()

    repoRoot = pathlib.Path(arguments.repo_root).resolve()
    ValidateSources(repoRoot)
    if arguments.binary:
        ValidateBinary(pathlib.Path(arguments.binary).resolve())
    print("14E-02 Qt host boundary: PASS")


if __name__ == "__main__":
    Main()
