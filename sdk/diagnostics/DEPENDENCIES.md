# Consumer Dependencies

This source SDK contains SliceSoft integration code, not vendored third-party
implementations or binaries. It resolves existing consumer CMake targets only.

| Dependency | Required target | License and distribution |
|---|---|---|
| spdlog | `spdlog::spdlog` | MIT. The repository reference notice is included in `licenses/spdlog.txt`. |
| fmt, when selected by spdlog | Through the consumer spdlog package | MIT with upstream optional binary-embedding exception; reference notice in `licenses/fmt.txt`. |
| nlohmann/json | `nlohmann_json::nlohmann_json` | MIT. No implementation is copied here; obtain the notice matching the consumer package before distributing compiled products. |
| Windows DbgHelp | System `dbghelp` import library, optional helper only | Supplied by the Windows SDK/OS; no system DLL is redistributed here. |

The included reference notices do not certify which versions or linkage a
consumer selects. The consumer owns its dependency lock, configuration/CRT
compatibility, actual runtime DLL deployment and corresponding notices.

No dependency installer, Qt, full slicing core, RIP runtime, model, PDB, dump,
private log or PrintApp source is included. This package does not introduce a
new license for first-party source or grant third-party rights beyond their
existing terms; retain project ownership and distribution rules.
