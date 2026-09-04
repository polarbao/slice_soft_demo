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
        and "EffectiveOutputDirectoryName" in panel
        and 'QStringLiteral("rip_diagnostic")' in controller,
        "strict and diagnostic outputs are not shown as sibling Package paths",
    )
    require(
        "DiagnosticUnvalidated" in controller
        and 'QStringLiteral("RIP_DIAGNOSTIC_SAVED")' in controller
        and 'QStringLiteral("s2PublicationEligible"), false' in controller,
        "diagnostic output is not isolated from strict S2 publication",
    )
    require(
        all(
            f'QStringLiteral("{label}")' in panel
            for label in ("透明", "不透", "肤色", "白色 30", "白色 50")
        )
        and "coreSettings.transparent_mode = m_settings.transparentmode" in controller
        and "manifestWhite" not in controller,
        "RIP --transparent 0..4 modes are not wired losslessly",
    )
    print(
        "RIPFLOW_WIRING_PASS strict_hook=1 shell_free=1 sibling_paths=1 "
        "diagnostic_isolated=1 transparent_modes=5"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
