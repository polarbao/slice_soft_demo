#!/usr/bin/env python3

from pathlib import Path


def RequireTokens(path: Path, tokens: tuple[str, ...]) -> None:
    content = path.read_text(encoding="utf-8")
    for token in tokens:
        if token not in content:
            raise AssertionError(f"{path.name} misses capability DTO field: {token}")


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    apiRoot = repoRoot / "src" / "slicer_core" / "api"

    RequireTokens(
        apiRoot / "PackageDtos.h",
        (
            "package_identity",
            "PackageGrid grid",
            "channels",
            "bit_depth",
            "polarity",
            "per_instance",
            "profile_echo",
            "empty_pixels",
            "storage_mode",
            "PackageValidationError",
            "per_layer_checksum",
            "PackageReport",
            "report_schema",
            "source_path",
        ),
    )
    RequireTokens(
        apiRoot / "PackageQueryFacade.h",
        ("ApiResult<PackageReport> ReadReport",),
    )
    RequireTokens(
        apiRoot / "ModelDtos.h",
        ("has_normals",),
    )

    print("Stage 14B facade DTO v1.2 alignment: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
