import argparse
import pathlib
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
    forbidden = (
        '#include "slicer_core/',
        "#include <slicer_core/",
        "slicer_core",
        "slicer_base",
        "slicer_engine",
        "target_link_libraries(slicer_ui_host_sim PRIVATE Qt5::Widgets slicer_module",
    )
    for token in forbidden:
        if token in sourceText:
            Fail(f"forbidden internal dependency found: {token}")

    for required in ("LoadLibraryW", "GetProcAddress", "Qt5::Widgets"):
        if required not in sourceText:
            Fail(f"runtime host marker missing: {required}")
    for exportName in EXPORTS:
        if f'"{exportName}"' not in sourceText:
            Fail(f"frozen export is not resolved: {exportName}")

    for path in sourcePaths:
        if path.suffix.lower() in {".cpp", ".h"}:
            lineCount = len(path.read_text(encoding="utf-8").splitlines())
            if lineCount > 500:
                Fail(f"new host source exceeds 500 lines: {path} ({lineCount})")


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
