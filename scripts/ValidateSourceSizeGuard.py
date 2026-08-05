#!/usr/bin/env python3

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Change:
    path: str
    baseLines: int | None
    currentLines: int


def ParseArguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Enforce SliceSoft source-size gates G1 through G5."
    )
    parser.add_argument("--base-ref", help="Git base used for incremental checks.")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def RunGit(repoRoot: Path, arguments: list[str], check: bool = True) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=repoRoot,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if check and result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "git command failed")
    return result.stdout


def NormalizePath(path: str) -> str:
    return path.replace("\\", "/")


def CountLines(content: str) -> int:
    return len(content.splitlines())


def IsSource(path: str) -> bool:
    return Path(path).suffix.lower() in {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}


def IsHeader(path: str) -> bool:
    return Path(path).suffix.lower() in {".h", ".hh", ".hpp"}


def IsExcluded(path: str, config: dict) -> bool:
    return any(path.startswith(prefix) for prefix in config["excludedPrefixes"])


def ReadConfig(repoRoot: Path) -> dict:
    configPath = repoRoot / "scripts" / "SourceSizeGuardConfig.json"
    config = json.loads(configPath.read_text(encoding="utf-8"))
    if config.get("schemaVersion") != 1:
        raise AssertionError("source-size guard schemaVersion must be 1")
    protectedPrefixes = tuple(config["protectedPrefixes"])
    for entry in config["allowlist"]:
        if not entry.get("reason") or not entry.get("expiresWhen"):
            raise AssertionError(f"allowlist entry lacks reason/expiry: {entry}")
        if not (repoRoot / entry["path"]).is_file():
            raise AssertionError(f"allowlist path does not exist: {entry['path']}")
        if any(entry["path"].startswith(prefix) for prefix in protectedPrefixes):
            raise AssertionError(
                f"protected Stage 14 path cannot be allowlisted: {entry['path']}"
            )
    return config


def ResolveBaseRef(repoRoot: Path, requestedRef: str | None) -> str:
    candidate = requestedRef or os.environ.get("SLICESOFT_LINE_GUARD_BASE")
    if candidate:
        RunGit(repoRoot, ["rev-parse", "--verify", candidate])
        return candidate
    parentResult = subprocess.run(
        ["git", "rev-parse", "--verify", "HEAD^"],
        cwd=repoRoot,
        check=False,
        capture_output=True,
        text=True,
    )
    return "HEAD^" if parentResult.returncode == 0 else "HEAD"


def ReadBaseContent(repoRoot: Path, baseRef: str, path: str) -> str | None:
    result = subprocess.run(
        ["git", "show", f"{baseRef}:{path}"],
        cwd=repoRoot,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return result.stdout if result.returncode == 0 else None


def CollectChanges(repoRoot: Path, baseRef: str, config: dict) -> list[Change]:
    changedPaths = set()
    diffOutput = RunGit(repoRoot, ["diff", "--name-only", baseRef, "--"])
    changedPaths.update(NormalizePath(path) for path in diffOutput.splitlines())
    otherOutput = RunGit(repoRoot, ["ls-files", "--others", "--exclude-standard"])
    changedPaths.update(NormalizePath(path) for path in otherOutput.splitlines())

    changes = []
    for path in sorted(changedPaths):
        currentPath = repoRoot / path
        if not IsSource(path) or IsExcluded(path, config) or not currentPath.is_file():
            continue
        currentContent = currentPath.read_text(encoding="utf-8", errors="replace")
        baseContent = ReadBaseContent(repoRoot, baseRef, path)
        changes.append(
            Change(
                path=path,
                baseLines=None if baseContent is None else CountLines(baseContent),
                currentLines=CountLines(currentContent),
            )
        )
    return changes


def IsAllowed(path: str, rule: str, config: dict) -> bool:
    return any(
        entry["path"] == path and rule in entry["rules"]
        for entry in config["allowlist"]
    )


def EvaluateChanges(changes: list[Change], config: dict) -> list[str]:
    thresholds = config["thresholds"]
    failures = []
    for change in changes:
        if change.baseLines is None:
            if (
                change.currentLines > thresholds["newSourceMaxLines"]
                and not IsAllowed(change.path, "G1", config)
            ):
                failures.append(
                    f"G1 {change.path}: new source has {change.currentLines} lines "
                    f"> {thresholds['newSourceMaxLines']}"
                )
            if (
                IsHeader(change.path)
                and change.currentLines > thresholds["newHeaderMaxLines"]
                and not IsAllowed(change.path, "G3", config)
            ):
                failures.append(
                    f"G3 {change.path}: new header has {change.currentLines} lines "
                    f"> {thresholds['newHeaderMaxLines']}"
                )
        elif (
            change.baseLines > thresholds["legacyGrowthGuardLines"]
            and change.currentLines > change.baseLines
            and not IsAllowed(change.path, "G2", config)
        ):
            failures.append(
                f"G2 {change.path}: legacy file grew from {change.baseLines} "
                f"to {change.currentLines} lines"
            )
    return failures


def CollectWarnings(repoRoot: Path, config: dict) -> list[str]:
    thresholds = config["thresholds"]
    tracked = RunGit(repoRoot, ["ls-files"]).splitlines()
    warnings = []
    now = int(time.time())
    for rawPath in tracked:
        path = NormalizePath(rawPath)
        sourcePath = repoRoot / path
        if not IsSource(path) or IsExcluded(path, config) or not sourcePath.is_file():
            continue
        lineCount = CountLines(sourcePath.read_text(encoding="utf-8", errors="replace"))
        if Path(path).suffix.lower() in {".cc", ".cpp", ".cxx"}:
            headerPath = str(Path(path).with_suffix(".h")).replace("\\", "/")
            header = repoRoot / headerPath
            headerLines = (
                CountLines(header.read_text(encoding="utf-8", errors="replace"))
                if header.is_file()
                else 0
            )
            if (
                lineCount > thresholds["implementationRatioMinLines"]
                and headerLines <= thresholds["implementationRatioHeaderMaxLines"]
                and not IsAllowed(path, "G4", config)
            ):
                warnings.append(
                    f"G4 {path}: implementation={lineCount}, header={headerLines}"
                )
        if lineCount <= thresholds["emptyShellMaxLines"]:
            timestampText = RunGit(repoRoot, ["log", "-1", "--format=%ct", "--", path]).strip()
            if timestampText:
                ageDays = (now - int(timestampText)) // 86400
                if ageDays >= thresholds["emptyShellAgeDays"]:
                    warnings.append(f"G5 {path}: {lineCount} lines, unchanged for {ageDays} days")
    return warnings


def RunSelfTest(config: dict) -> int:
    cases = [
        (Change("src/new.cpp", None, 501), "G1"),
        (Change("src/NewHeader.h", None, 201), "G3"),
        (Change("src/Legacy.cpp", 1001, 1002), "G2"),
    ]
    for change, expectedRule in cases:
        failures = EvaluateChanges([change], config)
        if not any(failure.startswith(expectedRule) for failure in failures):
            raise AssertionError(f"self-test did not trigger {expectedRule}")
    if EvaluateChanges([Change("src/Legacy.cpp", 1001, 1000)], config):
        raise AssertionError("G2 must allow legacy files to shrink")
    print("SliceSoft source-size guard self-test: PASS")
    return 0


def Main() -> int:
    arguments = ParseArguments()
    repoRoot = Path(__file__).resolve().parents[1]
    config = ReadConfig(repoRoot)
    if arguments.self_test:
        return RunSelfTest(config)

    baseRef = ResolveBaseRef(repoRoot, arguments.base_ref)
    failures = EvaluateChanges(CollectChanges(repoRoot, baseRef, config), config)
    warnings = CollectWarnings(repoRoot, config)
    for warning in warnings:
        print(f"WARNING: {warning}")
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}", file=sys.stderr)
        return 1
    print(
        f"SliceSoft source-size guard: PASS (base={baseRef}, "
        f"warnings={len(warnings)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
