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
    required = {
        "schema", "autoAfterSlice", "renderIntent", "transparentMode",
        "colorMode", "inputIcc", "outputIcc", "continueOnError",
        "deviceGrayBits", "timeoutSeconds", "outputDirectoryName",
        "existingOutputPolicy",
    }
    allowed = required | {"outputValidationMode"}
    if not isinstance(value, dict) or not required.issubset(value) or not set(value).issubset(allowed):
        raise ValueError("settings fields")
    if value["schema"] != "slicesoft.rip.settings.2":
        raise ValueError("settings schema")
    if not isinstance(value["autoAfterSlice"], bool) or not isinstance(value["continueOnError"], bool):
        raise ValueError("settings booleans")
    if (value["renderIntent"] not in range(4)
            or value["transparentMode"] not in range(5)
            or value["colorMode"] != 0):
        raise ValueError("settings numeric enum")
    if value["deviceGrayBits"] not in {1, 2}:
        raise ValueError("device gray bits")
    if value.get("outputValidationMode", "strict_s2") not in {
        "strict_s2", "diagnostic_unvalidated"
    }:
        raise ValueError("output validation mode")
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
    if value["version"] != "1.1.0":
        raise ValueError("module version")
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
    if value["schema"] != "slicesoft.rip.result.2" or value["status"] not in {"succeeded", "failed", "cancelled"}:
        raise ValueError("result identity")
    if value["externalValidation"] != "EXTERNAL_VALIDATION_DEFERRED" or not HEX64.fullmatch(value["sourceManifestSha256"]):
        raise ValueError("result external state")
    validate_settings(value["settings"])
    if value["settings"].get("outputValidationMode", "strict_s2") != "strict_s2":
        raise ValueError("strict result settings mode")
    if value["output"]["directory"] != "rip" or value["output"]["filePattern"] != "rip_%06d.tif":
        raise ValueError("result output")


def validate_diagnostic(value):
    require_exact(value, {
        "schema", "status", "externalValidation", "sourcePackage",
        "sourceManifestSha256", "module", "settings", "process", "output",
    })
    if value["schema"] != "slicesoft.rip.diagnostic.2" or value["status"] != "diagnostic_unvalidated":
        raise ValueError("diagnostic identity")
    if value["externalValidation"] != "EXTERNAL_VALIDATION_DEFERRED" or not HEX64.fullmatch(value["sourceManifestSha256"]):
        raise ValueError("diagnostic external state")
    validate_settings(value["settings"])
    if value["settings"].get("outputValidationMode") != "diagnostic_unvalidated":
        raise ValueError("diagnostic settings mode")
    if value["process"].get("exitCode") != 0:
        raise ValueError("diagnostic process")
    output = value["output"]
    required_output = {
        "directory", "layerCount", "filePattern", "minimum", "maximum",
        "s2DropLimitsPassed", "s2PublicationEligible", "referenceDropLimit",
        "samplesExceedingLimit",
    }
    if not required_output.issubset(output) or not set(output).issubset(required_output | {"firstExceedance"}):
        raise ValueError("diagnostic output fields")
    if output["directory"] != "rip_diagnostic" or output["filePattern"] != "rip_%06d.tif":
        raise ValueError("diagnostic output identity")
    if output["s2PublicationEligible"] is not False:
        raise ValueError("diagnostic publication claim")
    for field in ("minimum", "maximum", "referenceDropLimit", "samplesExceedingLimit"):
        require_exact(output[field], {"W", "S", "V"})
    exceeded = sum(output["samplesExceedingLimit"].values())
    if output["s2DropLimitsPassed"] != (exceeded == 0):
        raise ValueError("diagnostic S2 evidence consistency")
    if (exceeded > 0) != ("firstExceedance" in output):
        raise ValueError("diagnostic first exceedance consistency")


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
    diagnostic = json.loads((fixture_root / "rip_diagnostic_valid.json").read_text(encoding="utf-8"))
    validate_settings(settings)
    validate_module(module)
    validate_result(result)
    validate_diagnostic(diagnostic)

    case = copy.deepcopy(settings)
    case["transparentMode"] = 5
    expect_failure(validate_settings, case, "unsupported transparent color mode")
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
    case = copy.deepcopy(diagnostic)
    case["output"]["s2PublicationEligible"] = True
    expect_failure(validate_diagnostic, case, "diagnostic print claim")
    print("RIPFLOW_CONTRACTS_PASS positive=4 negative=6")


if __name__ == "__main__":
    main()
