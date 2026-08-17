#!/usr/bin/env python3
import copy
import json
import pathlib
import re
import sys


HEX64 = re.compile(r"^[0-9a-f]{64}$")
RELATIVE_PATH = re.compile(r"^(?![A-Za-z]:)(?![/\\])(?!.*(?:^|[/\\])\.\.(?:[/\\]|$)).+$")


def require_exact(obj, required):
    if not isinstance(obj, dict) or set(obj) != set(required):
        raise ValueError("object fields do not match the frozen contract")


def validate_settings(value):
    fields = {
        "schema", "autoAfterSlice", "renderIntent", "transparentMode",
        "colorMode", "inputIcc", "outputIcc", "continueOnError",
        "deviceGrayBits", "timeoutSeconds", "outputDirectoryName",
        "existingOutputPolicy",
    }
    require_exact(value, fields)
    if value["schema"] != "slicesoft.rip.settings.1":
        raise ValueError("settings schema")
    if not isinstance(value["autoAfterSlice"], bool) or not isinstance(value["continueOnError"], bool):
        raise ValueError("settings booleans")
    if value["renderIntent"] not in range(4) or value["colorMode"] != 0:
        raise ValueError("settings numeric enum")
    if value["transparentMode"] not in {
        "follow_manifest", "explicit_transparent", "explicit_opaque"
    }:
        raise ValueError("transparent mode")
    if value["deviceGrayBits"] not in {1, 2}:
        raise ValueError("device gray bits")
    if not 1 <= value["timeoutSeconds"] <= 86400:
        raise ValueError("timeout")
    if value["outputDirectoryName"] != "rip" or value["existingOutputPolicy"] != "fail_closed":
        raise ValueError("output policy")
    for key in ("inputIcc", "outputIcc"):
        if not isinstance(value[key], str) or not RELATIVE_PATH.fullmatch(value[key]):
            raise ValueError(key)


def validate_module(value):
    required = {
        "schema", "moduleId", "version", "status", "architecture",
        "entrypoint", "library", "resourceDirectory", "files", "input",
        "output", "externalValidation",
    }
    require_exact(value, required)
    if value["schema"] != "slicesoft.rip.module.1" or value["moduleId"] != "slicesoft.external_rip":
        raise ValueError("module identity")
    if value["status"] != "LOCAL_ENGINEERING_ONLY" or value["architecture"] != "x86_64-windows":
        raise ValueError("module status")
    if value["externalValidation"] != "EXTERNAL_VALIDATION_DEFERRED":
        raise ValueError("external validation")
    if len(value["files"]) < 11:
        raise ValueError("module files")
    for item in value["files"]:
        require_exact(item, {"path", "size", "sha256"})
        if not RELATIVE_PATH.fullmatch(item["path"]) or item["size"] < 1 or not HEX64.fullmatch(item["sha256"]):
            raise ValueError("module file")
    if value["input"] != {
        "schema": "p0.rgbwsv.2", "bitDepth": 8, "samplesPerPixel": 6,
        "planar": "contiguous", "storage": "stripped",
    }:
        raise ValueError("module input")
    if value["output"] != {
        "samplesPerPixel": 7, "bitDepth": 8, "storage": "stripped",
        "rawPattern": "slice.N.tiff", "publishedPattern": "rip_%06d.tif",
    }:
        raise ValueError("module output")


def validate_result(value):
    require_exact(value, {
        "schema", "status", "externalValidation", "sourcePackage",
        "sourceManifestSha256", "module", "settings", "process", "output",
    })
    if value["schema"] != "slicesoft.rip.result.1" or value["status"] not in {"succeeded", "failed", "cancelled"}:
        raise ValueError("result identity")
    if value["externalValidation"] != "EXTERNAL_VALIDATION_DEFERRED" or not HEX64.fullmatch(value["sourceManifestSha256"]):
        raise ValueError("result external state")
    validate_settings(value["settings"])
    if value["output"]["directory"] != "rip" or value["output"]["filePattern"] != "rip_%06d.tif":
        raise ValueError("result output")


def expect_failure(validator, value, name):
    try:
        validator(value)
    except (KeyError, TypeError, ValueError):
        return
    raise AssertionError(f"negative case passed: {name}")


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    fixture_root = root / "tests" / "ripflow" / "contracts"
    settings = json.loads((fixture_root / "rip_settings_valid.json").read_text(encoding="utf-8"))
    module = json.loads((fixture_root / "rip_module_valid.json").read_text(encoding="utf-8"))
    result = json.loads((fixture_root / "rip_result_valid.json").read_text(encoding="utf-8"))
    validate_settings(settings)
    validate_module(module)
    validate_result(result)

    case = copy.deepcopy(settings)
    case["autoAfterSlice"] = True
    case["colorMode"] = 7
    expect_failure(validate_settings, case, "unsupported color mode")
    case = copy.deepcopy(settings)
    case["inputIcc"] = "../escape.icc"
    expect_failure(validate_settings, case, "path escape")
    case = copy.deepcopy(module)
    case["status"] = "PRODUCTION_READY"
    expect_failure(validate_module, case, "production claim")
    case = copy.deepcopy(result)
    case["externalValidation"] = "PASS"
    expect_failure(validate_result, case, "external pass claim")
    print("RIPFLOW_CONTRACTS_PASS positive=3 negative=4")


if __name__ == "__main__":
    main()
