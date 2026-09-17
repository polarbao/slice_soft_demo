#!/usr/bin/env python3

import argparse
import datetime
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
    parser.add_argument(
        "--census",
        action="store_true",
        help="全仓清点超限源文件并与 censusBaseline 比对（不依赖 git 历史）。",
    )
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
        # 这里历史上写的是 "lacks reason/expiry"，但 expiresWhen 的值是散文
        # （「版本重构时统一清理…」），本函数只检查它【是否存在】，IsAllowed 从不读它——
        # 结果是所有豁免实际上永久有效，且在日常输出里完全不可见（F-11）。
        # 措辞改为不再暗示时间到期；可判定的那一半由 expiresOn / CollectAllowlistNotices 承担。
        if not entry.get("reason") or not entry.get("expiresWhen"):
            raise AssertionError(
                f"allowlist entry lacks reason/expiresWhen: {entry}")
        if not (repoRoot / entry["path"]).is_file():
            raise AssertionError(f"allowlist path does not exist: {entry['path']}")
        if any(entry["path"].startswith(prefix) for prefix in protectedPrefixes):
            raise AssertionError(
                f"protected Stage 14 path cannot be allowlisted: {entry['path']}"
            )
        expiresOn = entry.get("expiresOn")
        if expiresOn is not None:
            try:
                datetime.date.fromisoformat(expiresOn)
            except (TypeError, ValueError):
                raise AssertionError(
                    f"allowlist expiresOn must be an ISO date (YYYY-MM-DD): {entry}")
    return config


def CollectAllowlistNotices(config: dict) -> list[str]:
    """把每条豁免连同其解除条件报出来，并对已过期的 expiresOn 判失败（F-11）。

    原状态是：豁免一旦写进配置就永久生效，且**在日常输出里完全不可见**——
    没有任何一行提醒它还在，更没有任何机制让它失效。本函数解决可见性；
    `expiresOn`（可选的 ISO 日期）解决可判定性：一旦过期即为 ERROR，必须复核后
    延期或删除，不能默默继续豁免。

    刻意**不**给现有三条补日期：它们的解除条件是「版本重构时统一清理」，
    即 F-09 完成，不是某个时间点；硬套日期只会制造一个到期就被顺手延后的形式。
    那三条的真正处置见 `docs/slice/DOC/DOC_PREP_F09_slicer_cpp拆解立项方案.md` §6。
    """
    notices = []
    expired = []
    today = datetime.date.today()
    for entry in config["allowlist"]:
        rules = ",".join(entry.get("rules", []))
        expiresOn = entry.get("expiresOn")
        if expiresOn is not None and datetime.date.fromisoformat(expiresOn) < today:
            expired.append(
                f"ALLOWLIST {entry['path']} [{rules}]: expiresOn {expiresOn} "
                f"已过期，请复核后延期或删除")
            continue
        suffix = f"，expiresOn={expiresOn}" if expiresOn else "，无可判定到期条件"
        notices.append(
            f"ALLOWLIST {entry['path']} [{rules}] 仍在豁免："
            f"{entry['expiresWhen']}{suffix}")
    return notices, expired


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


def CollectCensus(repoRoot: Path, config: dict) -> tuple[int, list[str]]:
    """全仓清点：有多少【已跟踪】源文件超过「新文件」阈值。

    G1/G2/G3 是【变更】判据，且 base 默认是 HEAD^——两次提交之前引入的东西永远看不见。
    本次专项就撞上了：F-09 新增的三个文件超过 G1 阈值，一次都没被拦过
    （门禁注册的 ctest 当时只跑 --self-test，实质检查从不自动运行）。

    清点是【状态】判据，不依赖 git 历史，因此可以安全注册为 ctest。
    """
    thresholds = config["thresholds"]
    over: list[str] = []
    for rawPath in RunGit(repoRoot, ["ls-files"]).splitlines():
        path = NormalizePath(rawPath)
        sourcePath = repoRoot / path
        if not IsSource(path) or IsExcluded(path, config) or not sourcePath.is_file():
            continue
        lineCount = CountLines(sourcePath.read_text(encoding="utf-8", errors="replace"))
        limit = (
            thresholds["newHeaderMaxLines"] if IsHeader(path)
            else thresholds["newSourceMaxLines"]
        )
        if lineCount > limit:
            over.append(f"{path}: {lineCount} > {limit}")
    over.sort(key=lambda text: -int(text.rsplit(": ", 1)[1].split(" ")[0]))
    return len(over), over


def EvaluateCensus(repoRoot: Path, config: dict, verbose: bool) -> int:
    """棘轮：只记基线【数量】，不冻结清单。

    为什么不冻结清单：实测超限文件有 132 个。F-43 已论证过——冻结近百文件的清单
    会常年变红、没人维护，比没有门禁更糟。而且逐条定责在 132 条的规模上不可行
    （R-09 当初估的是 12 条，实际差 11 倍）。

    棘轮只回答一个问题：【有没有新增】。它不替任何专项承接既有的债，
    也不需要先定责就能立刻生效。代价是「加一个又删一个」会互相抵消，
    它是防回涨的棘轮，不是完备证明。
    """
    baseline = config.get("censusBaseline")
    count, over = CollectCensus(repoRoot, config)
    if baseline is None:
        print(f"NOTICE: CENSUS 未设基线，当前超限文件 {count} 个")
        return 0
    print(f"NOTICE: CENSUS 超限文件 {count} 个（基线 {baseline}）")
    if verbose:
        for item in over[:15]:
            print(f"NOTICE:   {item}")
        if len(over) > 15:
            print(f"NOTICE:   （余 {len(over) - 15} 个未列出）")
    if count > baseline:
        print(
            f"ERROR: CENSUS 超限文件由 {baseline} 增至 {count}——"
            f"新增了超过阈值的源文件。请拆分它，或在确有理由时更新 censusBaseline "
            f"并在提交信息里说明是哪一个文件、为什么不能拆。",
            file=sys.stderr,
        )
        return 1
    if count < baseline:
        print(
            f"NOTICE: CENSUS 已降到 {count}，低于基线 {baseline}——"
            f"可把 censusBaseline 收紧到 {count} 以锁住这份收益"
        )
    return 0


def Main() -> int:
    arguments = ParseArguments()
    repoRoot = Path(__file__).resolve().parents[1]
    config = ReadConfig(repoRoot)
    if arguments.self_test:
        return RunSelfTest(config)
    if arguments.census:
        return EvaluateCensus(repoRoot, config, verbose=True)

    baseRef = ResolveBaseRef(repoRoot, arguments.base_ref)
    failures = EvaluateChanges(CollectChanges(repoRoot, baseRef, config), config)
    warnings = CollectWarnings(repoRoot, config)
    notices, expired = CollectAllowlistNotices(config)
    # 豁免通告排在最前：它们此前完全不可见，写在末尾等于继续被 74 条 G4/G5 淹没。
    for notice in notices:
        print(f"NOTICE: {notice}")
    for warning in warnings:
        print(f"WARNING: {warning}")
    failures = list(failures) + expired
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}", file=sys.stderr)
        return 1
    print(
        f"SliceSoft source-size guard: PASS (base={baseRef}, "
        f"allowlist={len(notices)}, warnings={len(warnings)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
