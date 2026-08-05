#!/usr/bin/env python3

from pathlib import Path


def Require(text: str, token: str, message: str) -> None:
    if token not in text:
        raise AssertionError(message)


def Forbid(text: str, token: str, message: str) -> None:
    if token in text:
        raise AssertionError(message)


def Extract(text: str, begin: str, end: str) -> str:
    start = text.find(begin)
    finish = text.find(end, start + len(begin))
    if start < 0 or finish < 0:
        raise AssertionError(f"cannot isolate CLI route: {begin}")
    return text[start:finish]


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    mainPath = repoRoot / "apps" / "slicer_cli" / "main.cpp"
    source = mainPath.read_text(encoding="utf-8")
    route = Extract(
        source,
        "int RunMultiModelSceneProduction(const CliOptions& options)",
        "int PrintTiffBackendInfoJson()",
    )
    dispatcher = Extract(source, "int main(int argc, char** argv)", "catch (")

    Require(
        route,
        "CreateProductionSliceFacade()",
        "--scene-config must create the production SliceFacade",
    )
    Require(
        route,
        "facade->Run(",
        "--scene-config must execute through SliceFacade::Run",
    )
    Require(
        route,
        "LegacySceneErrorCode(*error)",
        "Facade failures must preserve stable legacy scene error names",
    )
    Require(
        dispatcher,
        "return RunMultiModelSceneProduction(options);",
        "the --scene-config dispatcher must enter the Facade route",
    )
    Forbid(
        route,
        "RunMultiModelProductionService(",
        "CLI must not bypass SliceFacade for --scene-config",
    )
    Forbid(
        route,
        "RunSlicePipeline(",
        "scene production must not fall back to the single-model pipeline",
    )

    print("Stage 14B-05 CLI Facade route contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
