#!/usr/bin/env python3
"""Generate a portable absolute geometry.repair request for Worker debugging."""

from __future__ import annotations

import argparse
import hashlib
import json
import uuid
from pathlib import Path
from typing import Any


def DumpLikeSlicer(value: Any) -> str:
    if isinstance(value, dict):
        items = [
            f"{json.dumps(key, ensure_ascii=False)}: {DumpLikeSlicer(value[key])}"
            for key in sorted(value)
        ]
        return "{" + ("\n" + ",\n".join(items) + "\n" if items else "") + "}"
    if isinstance(value, list):
        items = [DumpLikeSlicer(item) for item in value]
        return "[" + ("\n" + ",\n".join(items) + "\n" if items else "") + "]"
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "null"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return str(int(value)) if value.is_integer() else format(value, ".15g")
    raise TypeError(f"unsupported JSON value: {type(value).__name__}")


def CanonicalHash(document: dict[str, Any]) -> str:
    payload = DumpLikeSlicer(document).encode("utf-8")
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()

    repoRoot = arguments.repo_root.resolve()
    requestPath = arguments.output.resolve()
    try:
        requestPath.relative_to(repoRoot)
    except ValueError as error:
        raise ValueError("debug request output must stay inside the repository") from error
    jobRoot = requestPath.parent
    modelPath = (jobRoot / "source/tetra.obj").resolve()
    attemptIdentity = uuid.uuid4().hex[:12]
    repairPath = (
        jobRoot / f"repair/tetra-{attemptIdentity}.repaired.obj"
    ).resolve()
    modelPath.parent.mkdir(parents=True, exist_ok=True)
    modelPath.write_text(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n",
        encoding="ascii",
    )

    profile: dict[str, Any] = {
        "profileVersion": "1.0",
        "slicingMode": "closed_mesh_scanline",
        "autoOrient": {"enabled": False, "maxHeightMm": 9},
        "slicePipeline": {"mode": "legacy"},
        "input": {"modelPath": modelPath.as_posix(), "format": "obj"},
        "output": {
            "packageDir": (jobRoot / "unused-package").resolve().as_posix(),
            "dpiX": 127,
            "dpiY": 127,
            "layerThicknessMm": 0.2,
            "channelOrder": ["R", "G", "B", "W", "S", "V"],
            "bitDepth": 8,
            "planarConfig": "contiguous",
            "storageMode": "stripped",
            "rowsPerStrip": 64,
        },
        "preview": {"enabled": False},
        "background": {"value": 255},
        "modelMaterial": {
            "materialChannel": "RGB",
            "applyMode": "solid_volume",
            "rgb": [0, 0, 0],
            "whiteValue": 255,
            "varnishValue": 255,
        },
        "materialProcessProfile": {
            "enabled": True,
            "name": "profile-stage14d08-debug",
            "target": "stage14d08-debug-fixture",
        },
    }
    profileHash = CanonicalHash(profile)
    profile["profileHash"] = profileHash
    request = {
        "contract": "file_contract",
        "major": 1,
        "minor": 0,
        "jobId": "stage14d08-debug-repair",
        "correlationId": f"stage14d08-debug-repair-{attemptIdentity}",
        "capability": "geometry.repair",
        "timeoutMs": 30000,
        "profile": profile,
        "input": {
            "modelId": "stage14d08-debug-cube",
            "modelPath": modelPath.as_posix(),
            "modelFormat": "obj",
            "outputPath": repairPath.as_posix(),
            "profileHash": profileHash,
            "sourceResourceScope": {"rootPath": modelPath.parent.as_posix()},
            "repairOutputFormat": "obj",
            "policy": "conservative",
            "requireStrictPass": True,
        },
    }
    jobRoot.mkdir(parents=True, exist_ok=True)
    requestPath.write_text(
        json.dumps(request, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(requestPath)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(Main())
    except (OSError, ValueError) as error:
        print(f"Stage 14D-08 debug request generation failed: {error}")
        raise SystemExit(1) from error
