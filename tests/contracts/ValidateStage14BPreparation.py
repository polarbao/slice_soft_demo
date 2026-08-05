#!/usr/bin/env python3

from pathlib import Path


def RequireText(path: Path, requiredTexts: tuple[str, ...]) -> None:
    if not path.exists():
        raise AssertionError(f"required Stage 14B document is missing: {path}")
    content = path.read_text(encoding="utf-8")
    for requiredText in requiredTexts:
        if requiredText not in content:
            raise AssertionError(f"{path.name} misses required text: {requiredText}")


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    prepPath = (
        repoRoot
        / "docs"
        / "slice"
        / "DOC"
        / "DOC_PREP_14B_核心Facade与BaseEngine分层实施准备.md"
    )
    taskPath = (
        repoRoot
        / "docs"
        / "codex_task"
        / "current"
        / "TASKS_14_切片能力包封装与打印软件集成任务清单.md"
    )
    reportPath = (
        repoRoot
        / "docs"
        / "slice"
        / "REPORT"
        / "REPORT_14_切片能力包封装与打印软件集成准备状态.md"
    )

    requiredFiles = (
        repoRoot / "contracts" / "print_module_spi.h",
        repoRoot / "contracts" / "slicer_capability_dtos.json",
        repoRoot / "contracts" / "slicer_three_lane_contract.json",
        repoRoot / "contracts" / "slicer_cancel_contract.json",
        repoRoot / "contracts" / "slicer_ui_view_spec.json",
        repoRoot / "docs" / "slice" / "PRD" / "PRD_14_切片能力包封装与打印软件集成.md",
        repoRoot / "docs" / "slice" / "DEV" / "DEV_14_切片能力包封装与打印软件集成.md",
        repoRoot / "docs" / "slice" / "DEMO" / "DEMO_14_切片能力包封装与打印软件集成验收方案.md",
    )
    missingFiles = [str(path) for path in requiredFiles if not path.exists()]
    if missingFiles:
        raise AssertionError(f"Stage 14B prerequisites are missing: {missingFiles}")

    RequireText(
        prepPath,
        (
            "14B_PREPARATION_GATE = PASS",
            "FIRST_TASK           = 14B-00",
            "slicer_engine -> slicer_base",
            "TexturedSceneViewDataProvider",
            "14B-06",
            "不改变 `p0.rgbwsv.2`",
        ),
    )
    RequireText(
        taskPath,
        tuple(
            f"14B-{taskId}"
            for taskId in ("00", "01", "01A", "02", "03", "03A", "04", "05", "06")
        ),
    )
    RequireText(
        reportPath,
        (
            "14B_PREPARATION_GATE",
            "= PASS          （Facade/Base-Engine 实施准备已冻结）",
            "CURRENT_NEXT_TASK      = 14B-00 / 14B-06",
        ),
    )

    print("Stage 14B preparation contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
