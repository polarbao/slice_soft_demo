#!/usr/bin/env python3

from pathlib import Path


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    apiRoot = repoRoot / "src" / "slicer_core" / "api"
    headers = sorted(apiRoot.glob("*.h"))
    if not headers:
        raise AssertionError("Stage 14B facade headers are missing")

    combined = ""
    for header in headers:
        content = header.read_text(encoding="utf-8")
        combined += content
        lineCount = len(content.splitlines())
        if lineCount > 200:
            raise AssertionError(f"G3 facade header exceeds 200 lines: {header}={lineCount}")
        for forbidden in ("<QObject>", "<QString>", "print_module_spi.h", "PM_API", "PM_CALL"):
            if forbidden in content:
                raise AssertionError(f"facade header contains forbidden ABI/Qt token: {forbidden}")

    for required in (
        "class ICancelToken",
        "class ModelFacade",
        "class PackageQueryFacade",
        "class SceneFacade",
        "class SliceFacade",
        "class PreflightFacade",
        "class PreflightFullFacade",
        "class RepairFacade",
        "struct SceneViewData",
        "struct ViewAppearance",
    ):
        if required not in combined:
            raise AssertionError(f"facade contract misses required declaration: {required}")

    print(f"Stage 14B facade header contract: PASS (headers={len(headers)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
