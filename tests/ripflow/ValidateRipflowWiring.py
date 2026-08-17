#!/usr/bin/env python3
from pathlib import Path
import sys


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"RIPFLOW_WIRING_FAILED: {message}")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    result = (root / "apps/slicer_ui_host_sim/HostMainWindowResult.cpp").read_text(
        encoding="utf-8"
    )
    job = (root / "apps/slicer_ui_host_sim/HostMainWindowJob.cpp").read_text(
        encoding="utf-8"
    )
    controller = (
        root / "apps/slicer_ui_host_sim/HostRipJobController.cpp"
    ).read_text(encoding="utf-8")
    panel = (root / "apps/slicer_ui_host_sim/HostRipSettingsPanel.cpp").read_text(
        encoding="utf-8"
    )

    failed_branch = result.index("if (!loaded)")
    auto_hook = result.index("StartRipForPackage(packageDirectory, true)")
    require(failed_branch < auto_hook, "automatic hook precedes strict load failure branch")
    require(
        "m_ripSettingsPanel->Settings().autoafterslice" in result,
        "automatic hook is not guarded by the persisted switch",
    )
    require(
        "StartRipForPackage" not in job,
        "RIP is connected directly to Worker completion instead of strict package load",
    )
    require(
        "setProgram(" in controller and "setArguments(" in controller,
        "QProcess program and argv are not separated",
    )
    require(
        "cmd.exe" not in controller.lower() and "powershell" not in controller.lower(),
        "product execution path invokes a shell",
    )
    require(
        'QStringLiteral("/layers")' in panel
        and 'QStringLiteral("/rip")' in panel,
        "layers and rip are not shown as sibling Package paths",
    )
    print("RIPFLOW_WIRING_PASS strict_hook=1 shell_free=1 sibling_paths=1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
