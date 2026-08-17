import argparse
import os
import pathlib
import subprocess
import sys
import tempfile


def Fail(message):
    print(f"14E-02 missing-module test: FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    arguments = parser.parse_args()

    hostPath = pathlib.Path(arguments.host).resolve()
    if not hostPath.is_file():
        Fail(f"host binary is missing: {hostPath}")

    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "offscreen"
    with tempfile.TemporaryDirectory(prefix="slicesoft-14e02-") as tempDirectory:
        missingPath = pathlib.Path(tempDirectory) / "missing_slicer_module.dll"
        result = subprocess.run(
            [
                str(hostPath),
                "--self-test",
                "--module",
                str(missingPath),
            ],
            capture_output=True,
            check=False,
            encoding="utf-8",
            errors="replace",
            env=environment,
            timeout=20,
        )

    combinedOutput = result.stdout + result.stderr
    if result.returncode != 3:
        Fail(f"expected exit code 3, got {result.returncode}: {combinedOutput}")
    if "MODULE_LOAD_FAILED" not in combinedOutput:
        Fail(f"stable fail-closed marker is missing: {combinedOutput}")

    uiResult = subprocess.run(
        [
            str(hostPath),
            "--hostflow-import-ui-self-test",
            "--module",
            str(missingPath),
        ],
        capture_output=True,
        check=False,
        encoding="utf-8",
        errors="replace",
        env=environment,
        timeout=20,
    )
    uiOutput = uiResult.stdout + uiResult.stderr
    if uiResult.returncode != 0 or "VERSION_UI_UNAVAILABLE_PASS" not in uiOutput:
        Fail(f"software/library unavailable UI state is missing: {uiOutput}")
    print("14E-02 missing-module test: PASS")


if __name__ == "__main__":
    Main()
