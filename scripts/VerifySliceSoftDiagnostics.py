"""Same-binary diagnostics off/info comparison; writes evidence, never a production package."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


class MemoryCounters(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("faults", wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            "peakWorkingSet", "workingSet", "pagedPeak", "paged", "nonpagedPeak",
            "nonpaged", "pagefile", "peakPagefile")]


def run_process(command, cwd, env, evidence, timeout=120):
    get_memory = ctypes.WinDLL("psapi").GetProcessMemoryInfo
    get_memory.argtypes = [wintypes.HANDLE, ctypes.POINTER(MemoryCounters), wintypes.DWORD]
    peak = 0
    start = time.perf_counter()
    with evidence.open("wb") as stream:
        process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
        try:
            while True:
                memory = MemoryCounters()
                memory.cb = ctypes.sizeof(memory)
                if get_memory(wintypes.HANDLE(int(process._handle)), ctypes.byref(memory), memory.cb):
                    peak = max(peak, memory.peakWorkingSet)
                if process.poll() is not None:
                    break
                if time.perf_counter() - start > timeout:
                    raise TimeoutError(command)
                time.sleep(0.01)
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
    if process.returncode:
        raise RuntimeError(f"Exit {process.returncode}: {command}; see {evidence}")
    return {"wallMs": (time.perf_counter() - start) * 1000, "mainPeakWorkingSetBytes": peak}


def hashes(package):
    paths = sorted((package / "layers").glob("*.tiff"))
    if not paths:
        raise RuntimeError(f"No layer TIFFs: {package}")
    return {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--source-config", type=Path, default=Path("samples/configs/golden/material_process_top2_fixture.json"))
    parser.add_argument("--model", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    build = args.build.resolve()
    binary = build / args.config
    evidence = build / "diagnostic-evidence" / f"ab_{args.config}_{time.time_ns()}"
    evidence.mkdir(parents=True)
    source_path = (repo / args.source_config).resolve()
    source = json.loads(source_path.read_text(encoding="utf-8"))
    source["input"]["modelPath"] = str((source_path.parent / source["input"]["modelPath"]).resolve())
    if args.model:
        source["input"]["modelPath"] = str((repo / args.model).resolve())
    samples = []
    expected = None
    for iteration in range(args.repetitions + 1):
        for enabled in (False, True):
            name = f"{'info' if enabled else 'off'}_{iteration}"
            package = evidence / name / "package"
            source["output"]["packageDir"] = str(package)
            configuration = evidence / f"{name}.json"
            configuration.write_text(json.dumps(source), encoding="utf-8")
            env = os.environ.copy()
            env["SLICESOFT_DIAGNOSTICS_ENABLED"] = "1" if enabled else "0"
            env["SLICESOFT_LOG_LEVEL"] = "info"
            env["SLICESOFT_DUMP_ENABLED"] = "1"
            env["SLICESOFT_DIAGNOSTICS_DIR"] = str(evidence / "logs" / name)
            measured = run_process([str(binary / "slicer_cli.exe"), "--config", str(configuration)],
                                   repo, env, evidence / f"{name}.txt")
            measured.update(mode="info" if enabled else "off", warmup=iteration == 0)
            current = hashes(package)
            if expected is None:
                expected = current
            if current != expected:
                raise AssertionError(f"Layer bytes changed: {name}")
            run_process([str(binary / "rip_reader_test.exe"), "--package", str(package), "--quiet"],
                        repo, env, evidence / f"{name}_reader.txt")
            sessions = list((evidence / "logs" / name).glob("*/app.log"))
            if bool(sessions) != enabled:
                raise AssertionError(f"Unexpected diagnostics persistence: {name}")
            if enabled:
                events = [json.loads(line) for path in sessions for line in path.read_text(encoding="utf-8").splitlines()]
                if not any(event.get("action") == "crash_reporter" and event.get("phase") == "ready" for event in events):
                    raise AssertionError("CLI crash reporter did not start")
                if not all(event["sourceProcessRole"] == "cli" for event in events):
                    raise AssertionError("CLI source role lost")
            samples.append(measured)
    summary = {"schema": "slicesoft.diagnostics.ab.v1", "result": "PASS", "samples": samples,
               "layerCount": len(expected), "layerSha256": expected,
               "sourceConfig": str(source_path), "model": source["input"]["modelPath"],
               "limits": ["Same binary, explicit local asset, no external RIP/printing",
                          "Peak working set is CLI only; helper is excluded", "Iteration zero is warmup"]}
    for mode in ("off", "info"):
        rows = [row for row in samples if row["mode"] == mode and not row["warmup"]]
        summary[mode] = {key: statistics.median(row[key] for row in rows)
                         for key in ("wallMs", "mainPeakWorkingSetBytes")}
    (evidence / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps({"result": "PASS", "evidence": str(evidence), "off": summary["off"], "info": summary["info"]}))


if __name__ == "__main__":
    main()
