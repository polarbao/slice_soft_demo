# Slice Code Standards

## General

- Language: C++20.
- Prefer RAII and value semantics.
- Avoid manual `new/delete`; use stack objects, `std::unique_ptr`, or `std::shared_ptr` when ownership requires it.
- Single-argument constructors should be `explicit`.
- Prefer `constexpr`, `enum class`, `std::optional`, `std::span`, `std::string_view` where appropriate.
- Avoid broad `using namespace` in headers and source files.

## Qt boundary

- Qt types are allowed in `apps/slicer_debug_ui`.
- Qt types are not allowed in slicer_core public/domain APIs.
- Long-running work must not block the Qt main thread.
- Prefer function-pointer connect syntax, not `SIGNAL/SLOT` macros.

## Naming

Current code uses mixed demo-era naming. During R1, do not rename everything at once.

For new formal modules prefer:

```text
Types: PascalCase
Functions: camelCase or existing local convention, keep consistent per module
Members: trailing underscore or existing module convention; do not mix within a class
Files: PascalCase for new formal classes when practical
```

If integrating with existing files, preserve local style to reduce churn.

## Comments

- Public APIs that become stable module boundaries should have brief Doxygen comments.
- Avoid redundant comments that restate code.
- Document non-obvious protocol decisions and error behavior.

## Error handling

- Do not silently fail.
- Low-level parsing/importer APIs should return status/result objects or throw only at documented boundaries.
- UI must surface command errors, exit codes, warnings, and `E_*` error codes.

## Performance

- Reserve large vectors when size is known.
- Avoid unnecessary per-pixel heap allocation.
- Avoid repeated disk I/O inside inner loops.
- Do not optimize before profiling, but do not introduce obviously quadratic hot paths.

## Floating point and units

- Default length unit is mm.
- Use explicit conversion for inch/mm/pixel/layer.
- Avoid direct `==` for floating point comparisons in geometry-sensitive code.
