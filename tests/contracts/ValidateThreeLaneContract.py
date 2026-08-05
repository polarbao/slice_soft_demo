#!/usr/bin/env python3

import json
from pathlib import Path
from typing import Any


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    contracts = repoRoot / "contracts"
    contract = LoadJson(contracts / "slicer_three_lane_contract.json")
    capabilities = LoadJson(contracts / "slicer_capability_dtos.json")
    byId = {capability["id"]: capability for capability in capabilities["capabilities"]}

    transient = contract["lanes"]["transient"]
    if transient["owner"] != "host" or transient["crossModuleCalls"] != 0:
        raise AssertionError("transient lane must remain host-local")
    if transient["persistentMutation"] or transient["sceneRevisionChange"]:
        raise AssertionError("transient lane must not mutate authoritative state")

    commit = contract["lanes"]["commit"]
    if commit["capability"] != "scene.apply_operation" or not commit["atomic"]:
        raise AssertionError("commit lane boundary drifted")
    requiredCommitFields = {
        "operationId",
        "currentSceneRevision",
        "expectedSceneRevision",
        "operations",
    }
    if set(commit["requiredRequestFields"]) != requiredCommitFields:
        raise AssertionError("commit request contract is incomplete")
    dtoPaths = {
        field["path"]
        for field in byId["scene.apply_operation"]["requestFields"]
    }
    if not requiredCommitFields <= dtoPaths:
        raise AssertionError("three-lane contract and capability DTO disagree")
    idempotency = commit["idempotency"]
    if idempotency["sameCanonicalRequest"] != (
        "replay_exact_result_without_revision_increment"
    ):
        raise AssertionError("operation replay must be idempotent")
    if idempotency["differentCanonicalRequest"] != "reject_without_mutation":
        raise AssertionError("operation ID collision must fail closed")

    revision = commit["revision"]
    if revision["staleCode"] != "PM-SLICER-LAYOUT-0022":
        raise AssertionError("stale revision error drifted")
    if revision["staleMutation"] or revision["partialApply"]:
        raise AssertionError("stale/failed commits must be atomic")

    production = contract["lanes"]["production"]
    if production["capability"] != "slice.rgbwsv":
        raise AssertionError("production lane must use slice.rgbwsv")
    if production["acceptedSceneState"] != "committed_scene_hash_only":
        raise AssertionError("production accepted uncommitted state")
    if production["authoritativePreflight"] != "full_in_worker":
        raise AssertionError("full preflight must remain authoritative in Worker")
    if production["silentFallback"]:
        raise AssertionError("production must not silently fall back")

    expectedRollback = [
        "discard_transient_state",
        "call_scene.get_snapshot",
        "replace_host_snapshot_and_revision",
        "rebuild_local_view_from_authoritative_snapshot",
        "create_new_operation_id_before_retry",
    ]
    if contract["staleRollback"]["steps"] != expectedRollback:
        raise AssertionError("stale rollback sequence drifted")
    if contract["invariants"]["commitRevisionIncrement"] != 1:
        raise AssertionError("successful commit must increment revision exactly once")

    print("Transient/Commit/Production interaction contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
