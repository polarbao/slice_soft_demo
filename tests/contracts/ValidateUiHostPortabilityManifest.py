#!/usr/bin/env python3
"""Validate the Stage 14E reference-host portability inventory."""

import json
import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "contracts" / "slicer_ui_host_portability_manifest.json"
HOST_ROOT = ROOT / "apps" / "slicer_ui_host_sim"
SHARED_HOST_FILES = {
    "apps/slicer_host_sim/HostRequestBuilder.c",
    "apps/slicer_host_sim/HostRequestBuilder.h",
    "apps/slicer_host_sim/JsonText.c",
    "apps/slicer_host_sim/JsonText.h",
}
VALID_DISPOSITIONS = {"direct_copy", "adapt_required"}


def fail(message: str) -> int:
    print(f"Stage 14E-06 portability manifest: FAIL: {message}", file=sys.stderr)
    return 1


def main() -> int:
    with MANIFEST_PATH.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)

    if manifest.get("contractVersion") != "1.0":
        return fail("contractVersion must remain 1.0")

    entries = manifest.get("sourceInventory")
    if not isinstance(entries, list) or not entries:
        return fail("sourceInventory must be a non-empty array")

    inventory_paths: set[str] = set()
    for entry in entries:
        path = entry.get("path")
        disposition = entry.get("disposition")
        layer = entry.get("layer")
        if not isinstance(path, str) or not path:
            return fail("every sourceInventory entry needs a path")
        if path in inventory_paths:
            return fail(f"duplicate sourceInventory path: {path}")
        if disposition not in VALID_DISPOSITIONS:
            return fail(f"invalid disposition for {path}: {disposition}")
        if not isinstance(layer, str) or not layer:
            return fail(f"missing layer for {path}")
        if not (ROOT / path).is_file():
            return fail(f"listed source does not exist: {path}")
        inventory_paths.add(path)

    host_paths = {
        path.relative_to(ROOT).as_posix()
        for path in HOST_ROOT.rglob("*")
        if path.is_file()
    }
    expected_paths = host_paths | SHARED_HOST_FILES
    missing = sorted(expected_paths - inventory_paths)
    extra = sorted(inventory_paths - expected_paths)
    if missing:
        return fail("unclassified host files: " + ", ".join(missing))
    if extra:
        return fail("inventory contains non-host files: " + ", ".join(extra))

    contract_paths: set[str] = set()
    for entry in manifest.get("contractInputs", []):
        path = entry.get("path")
        if not isinstance(path, str) or not (ROOT / path).is_file():
            return fail(f"missing contract input: {path}")
        if path in contract_paths:
            return fail(f"duplicate contract input: {path}")
        contract_paths.add(path)

    required_contracts = {
        "contracts/print_module_spi.h",
        "contracts/slicer_capability_dtos.json",
        "contracts/slicer_three_lane_contract.json",
        "contracts/slicer_cancel_contract.json",
        "contracts/slicer_error_codes.json",
        "contracts/slicer_ui_view_spec.json",
    }
    if not required_contracts.issubset(contract_paths):
        return fail("required ABI and UI contracts are incomplete")

    forbidden_prefixes = tuple(manifest.get("forbiddenSourceDependencies", []))
    if not forbidden_prefixes:
        return fail("forbiddenSourceDependencies must not be empty")
    for path in inventory_paths:
        if path.startswith(forbidden_prefixes):
            return fail(f"internal slicer source leaked into inventory: {path}")

    direct_count = sum(
        entry["disposition"] == "direct_copy" for entry in entries
    )
    adapt_count = len(entries) - direct_count
    print(
        "Stage 14E-06 portability manifest: PASS "
        f"(sources={len(entries)}, direct={direct_count}, adapt={adapt_count}, "
        f"contracts={len(contract_paths)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
