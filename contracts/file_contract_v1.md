# file_contract_v1

> Status: slicer-side frozen candidate; print-side written confirmation pending
> Version: 1.0
> Transport: UTF-8 JSON files, stdout protocol lines, process exit code
> Platform baseline: Windows x64

## 1. Boundary

`file_contract_v1` is the private contract between `slicer_module.dll` and
`slicer_worker.exe`. It does not change the public 11-function C ABI and does
not change the `p0.rgbwsv.2` production package.

The worker carries only heavy operations:

```text
slice.rgbwsv
geometry.preflight.full
geometry.repair
```

In-process slicing is not part of this contract.

## 2. Version negotiation

The module must execute the following command before submitting the first job
to a worker identity:

```text
slicer_worker.exe --contract-info
```

The worker writes exactly one UTF-8 JSON object to stdout. It must validate
against `file_contract_v1.contract_info.schema.json`.

Compatibility rules:

```text
major differs                 reject, PM-SLICER-INTERNAL-0099
worker minor >= module minor  accept; ignore unknown optional fields
worker minor < module minor   accept only features declared by that minor
missing p0.rgbwsv.2           reject, PM-SLICER-CONTRACT-0060
missing requested capability  reject before process launch
```

## 3. Job files and invocation

The module owns a private job directory:

```text
<tempRoot>/<jobId>/
  request.json
  result.json                 written atomically through result.json.tmp
  cancel.requested            optional zero-byte cancellation marker
```

Submission command:

```text
slicer_worker.exe --spi-request <absolute-request-json-path>
```

`request.json` must validate against
`file_contract_v1.request.schema.json`. `result.json`, when present, must
validate against `file_contract_v1.result.schema.json`.

The module must reject relative request paths, job IDs containing path
separators, and a result whose `jobId`, `correlationId`, or `capability` does
not match the request.

## 4. Progress and timing lines

Contract lines use ASCII keys and values without spaces. Unknown ordinary log
lines may be retained as diagnostics, but a line beginning with a reserved
prefix and failing the grammar is a contract error.

```text
SLICE_PROGRESS phase=<token> current=<uint> total=<uint> percent=<0..100> elapsedMs=<fixed-3>
SLICE_TIMING engine=<token> <key>=<value> ... totalMs=<fixed-3> workingSetBytes=<uint> peakWorkingSetBytes=<uint>
```

Required progress invariants:

```text
0 <= current <= total
0 <= percent <= 100
elapsedMs is monotonic for one job
terminal success emits percent=100 before process exit
```

Required timing keys are `engine`, `totalMs`, `workingSetBytes`, and
`peakWorkingSetBytes`. Additional timing keys are optional minor-version
extensions and must not contain whitespace.

## 5. Result and exit codes

Normal success and handled domain failures must write `result.json` before
exit. A crash, forced termination, or startup failure may have no result file;
the module then derives the stable error from the process state and exit-code
table in `file_contract_v1.exit_codes.json`.

The result `code` is the authoritative stable `PM-SLICER-*` code. The process
exit code only carries its category.

## 6. Timeout and process-tree ownership

Every request carries a finite `timeoutMs`; zero and an omitted timeout are
invalid. The module starts the worker in a Windows Job Object configured with
`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, and the worker process must not escape
that job.

```text
contract-info timeout  5000 ms
job timeout            request.timeoutMs
cancel grace period    2000 ms maximum
```

On timeout the module enters `Cancelling`, creates `cancel.requested`
atomically, waits at most 2000 ms, then terminates the Job Object if the worker
has not exited. The final public state becomes `Cancelled` only after process
exit and staging cleanup.

## 7. Cancellation and staging cleanup

The worker checks `cancel.requested` at step boundaries and inside per-layer
loops. Cancellation maps to exit code 8 and
`PM-SLICER-CANCELLED-0070` after cleanup.

For a target package `<packageDir>`, the existing writer creates sibling
temporary directories named `<packageDir>.staging.*` and
`<packageDir>.backup.*`.

Cleanup order is fixed:

```text
worker startup    remove stale temporary siblings owned by the same job target
normal success    staging -> self-check -> atomic publish -> remove backup
handled failure   remove current staging; preserve last successful package
cancel/timeout    worker cleanup first; module cleanup after worker exit
module release    close Job Object; repeat idempotent cleanup
```

Cleanup must be path-normalized and limited to sibling names derived from the
request's exact `packageDir`. Recursive deletion outside that parent directory
is forbidden. A remaining staging directory is a failed cancellation even if
the worker process has exited.

## 8. Encoding and security

```text
JSON encoding        UTF-8 without BOM
path encoding        JSON Unicode strings; absolute normalized paths
stdout encoding      UTF-8; reserved protocol keys remain ASCII
unknown JSON fields  ignored only within the same major version
secrets              forbidden in request/result/logs
```

## 9. Related contracts

```text
Public ABI                 contracts/print_module_spi.h
Error codes                contracts/slicer_error_codes.json
Production package         contracts/p0.rgbwsv.2.schema.json
Request schema             contracts/file_contract_v1.request.schema.json
Result schema              contracts/file_contract_v1.result.schema.json
Negotiation schema         contracts/file_contract_v1.contract_info.schema.json
Exit-code table            contracts/file_contract_v1.exit_codes.json
```
