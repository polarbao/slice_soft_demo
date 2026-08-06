from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]


def Read(relativePath: str) -> str:
    return (ROOT / relativePath).read_text(encoding="utf-8")


def Require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise AssertionError(f"{label}: missing {fragment!r}")


def Main() -> int:
    facade = Read("src/slicer_core/engine/SliceFacadeAdapter.cpp")
    factory = Read("src/slicer_core/engine/ProductionSliceFacadeFactory.cpp")
    productionHeader = Read(
        "src/slicer_core/pipeline/MultiModelProductionService.h"
    )
    adapterHeader = Read(
        "src/slicer_core/pipeline/LegacySceneLayerAdapter.h"
    )
    orchestratorHeader = Read(
        "src/slicer_core/pipeline/MultiModelSliceOrchestrator.h"
    )
    rasterHeader = Read("src/slicer_core/pipeline/SceneRasterTypes.h")
    composer = Read("src/slicer_core/pipeline/SceneLayerComposer.cpp")
    writerHeader = Read(
        "src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
    )
    writer = Read("src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.cpp")

    Require(facade, "m_productionRunner(", "facade runner")
    Require(facade, "cancelToken,", "facade token forwarding")
    Require(factory, "request.canceltoken = &cancelToken", "factory")
    Require(
        productionHeader,
        "const api::ICancelToken* canceltoken{nullptr}",
        "production request",
    )
    Require(adapterHeader, "const api::ICancelToken* canceltoken{nullptr}", "legacy adapter")
    Require(orchestratorHeader, "const api::ICancelToken* canceltoken{nullptr}", "orchestrator")
    Require(rasterHeader, "Cancelled,", "raster cancellation code")
    Require(composer, "kCancellationCheckStride", "composer long-loop checks")
    Require(writerHeader, "const api::ICancelToken* canceltoken{nullptr}", "writer request")
    Require(writer, '"before_layer_tiff"', "writer pre-TIFF boundary")
    Require(writer, '"after_layer_tiff"', "writer post-TIFF boundary")
    Require(writer, '"before_package_publish"', "writer publish boundary")
    Require(factory, '"PM-SLICER-CANCELLED-0070"', "stable facade code")

    print("Stage 14D-04A cancellation propagation contract passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(Main())
    except AssertionError as error:
        print(f"FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
