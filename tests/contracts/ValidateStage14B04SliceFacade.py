#!/usr/bin/env python3

from pathlib import Path


def Require(text: str, token: str, message: str) -> None:
    if token not in text:
        raise AssertionError(message)


def Forbid(text: str, token: str, message: str) -> None:
    if token in text:
        raise AssertionError(message)


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    adapterPath = (
        repoRoot / "src" / "slicer_core" / "engine" / "SliceFacadeAdapter.cpp"
    )
    factoryPath = (
        repoRoot
        / "src"
        / "slicer_core"
        / "engine"
        / "ProductionSliceFacadeFactory.cpp"
    )
    adapter = adapterPath.read_text(encoding="utf-8")
    factory = factoryPath.read_text(encoding="utf-8")
    implementation = adapter + "\n" + factory

    Require(
        factory,
        "RunMultiModelProductionService(request)",
        "SliceFacade must reuse the existing production scene entry",
    )
    Require(
        factory,
        "ReadSceneEffectiveConfig(effectiveConfigPath)",
        "submission must resolve the committed effective config",
    )
    Require(
        factory,
        "options.transfer_scene_production_opt_in = true;",
        "RGBWSVT Scene entry must carry the explicit production opt-in",
    )
    Forbid(
        factory,
        "transfer_scene_candidate",
        "obsolete RGBWSVT candidate flag must not remain in the production factory",
    )
    Require(
        adapter,
        "PM-SLICER-CANCELLED-0070",
        "cooperative cancellation must use the frozen PM code",
    )
    Require(
        adapter,
        "event.stage != \"completed\"",
        "cancellation must be checked at existing progress boundaries",
    )
    Require(
        adapter,
        "PM-SLICER-LAYOUT-0022",
        "stale committed sceneHash must fail closed",
    )

    for forbiddenToken in (
        "WriteRgbwsv",
        "write_rgbwsv",
        "RgbwsvPackageWriter",
        "OpenVDB",
        "openvdb",
        "p0.rgbwsv.1",
        "p0.rgbwsv.2",
        "black_is_print",
        "uint8",
    ):
        Forbid(
            implementation,
            forbiddenToken,
            f"14B-04 must not own or mutate production policy: {forbiddenToken}",
        )

    print("Stage 14B-04 SliceFacade architecture contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
