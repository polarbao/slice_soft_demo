#!/usr/bin/env python3
"""Validate the Stage 14D-06 Worker-only heavy capability boundary."""

from pathlib import Path
import sys


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    cmake = (repo / "CMakeLists.txt").read_text(encoding="utf-8")
    exports = (repo / "src/slicer_module/Exports.cpp").read_text(encoding="utf-8")
    sync = (repo / "src/slicer_module/SyncCapabilityAdapter.cpp").read_text(
        encoding="utf-8"
    )
    router = (repo / "src/slicer_module/CapabilityCarrierRouter.cpp").read_text(
        encoding="utf-8"
    )
    service = (repo / "src/slicer_module/WorkerJobService.cpp").read_text(
        encoding="utf-8"
    )

    module_block_start = cmake.find("add_library(slicer_module SHARED")
    module_block_end = cmake.find("set_target_properties(slicer_module", module_block_start)
    if module_block_start < 0 or module_block_end < 0:
        fail("slicer_module CMake target was not found")
    module_block = cmake[module_block_start:module_block_end]
    if "target_link_libraries(slicer_module PRIVATE slicer_base)" not in module_block:
        fail("slicer_module must link only slicer_base")
    if "slicer_engine" in module_block:
        fail("slicer_module must not link slicer_engine")

    for capability in (
        'route.workerCapability = std::string{WorkerPreflightCapability}',
        'route.workerCapability = std::string{RepairCapability}',
        'route.workerCapability = std::string{SliceCapability}',
    ):
        if capability not in router:
            fail(f"Worker route is missing: {capability}")

    if 'options.at("backend").as_string() != "worker"' not in router:
        fail("slice backend is not frozen to exact worker")
    if "WorkerJobService::Instance().Submit" not in exports:
        fail("pm_submit does not dispatch heavy work to WorkerJobService")
    lifecycle_markers = (
        "workerJobs.Poll",
        "workerJobs.RequestCancel",
        "workerJobs.Result",
        "WorkerJobService::Instance().ReleaseJob",
        "WorkerJobService::Instance().RemoveModule",
    )
    for marker in lifecycle_markers:
        if marker not in exports:
            fail(f"public SPI does not route Worker lifecycle marker {marker}")
    if "workerJobs.Poll" not in exports or "workerJobs.Result" not in exports:
        fail("public SPI poll/result carrier dispatch is incomplete")

    if "WorkerContractNegotiator" not in service or "--spi-request" not in service:
        fail("Worker service does not negotiate and launch file_contract_v1")
    if "CreateProduction" in service or "slicer_engine" in service:
        fail("Worker service contains an in-process engine dependency")
    if "std::thread" not in service:
        fail("heavy Worker submission is not asynchronous")

    heavy_execute_markers = (
        'capability == "geometry.repair"',
        'capability == "slice.rgbwsv"',
    )
    if not all(marker in sync for marker in heavy_execute_markers):
        fail("synchronous adapter heavy capability rejection boundary changed")
    if "context->" in sync[sync.find('capability == "geometry.repair"') : sync.find(
        'capability == "slice.rgbwsv"'
    )]:
        fail("synchronous adapter appears to execute a heavy capability")

    print("Stage 14D-06 Worker-only heavy routing validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
