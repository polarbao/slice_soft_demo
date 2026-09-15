# P0FIX 分析专项 P0 契约一致性与输入加固

日期：2026-09-14。授权：用户 2026-09-14 指示「可新建一个 feature 分支进行后续的软件优化处理」「可根据优先级，先进行 P0 阶段处理，开始进入处理阶段之前，先完成准备工作处理」。分支 `codex/feature-p0fix-contract-robustness`（自 `product/packaged-slicer` 尖端 `d28b6451`）。

来源：`analysis/` 分析专项（2026-09-06 首轮、09-08 增补、09-14 基线校正，共 10 份文档 2553 行）登记的 36 条发现 F-01..F-36 与 13 条路线图 R-01..R-13。本卡把其中 **P0 层**转成可开工的原子任务并承接状态真源；`analysis/` 保持为分析产物，不承担状态真源职责。

## Implementation Plan

### Problem Type

契约声明一致性缺陷 + 输入解析健壮性缺口 + 一条空转的对拍测试。**不是**几何、工艺、通道语义或采样策略问题。全部为加闸与声明补齐，不改变任何生产语义。

### Layer(s) Involved

`contracts/`（声明面）、`src/slicer_core/json_value.cpp`（解析器）、`src/slicer_module/`（Worker 协议与作业服务）、`apps/slicer_ui_host_sim/`（能力判定）、`tests/stage14c_05/`（对拍测试）、`CMakeLists.txt`（测试注册）。Qt 保持在宿主，核心不引入 Qt。

### Official Documents

`contracts/print_module_spi.h` 的 11 函数 ABI 与不变量 4（异常不逃逸）；`contracts/file_contract_v1.md` §2/§5（协商与稳定错误码）；`contracts/slicer_error_codes.json`（错误码真源）；`contracts/slicer_module_manifest.schema.json`（部署清单）。本卡**兑现既有合同**，不改 schema、不改 ABI、不改通道语义、不新增导出。

### Historical Documents

`analysis/04_问题清单与改动空间.md`（F-01..F-21）、`analysis/07_..09_`（F-22..F-36）、`analysis/06_改进路线图与验证方案.md`（R-01..R-13 与逐条验证 Gate）。这些是分析结论，不是状态真源。

### AI Workspace Evidence

分支自 `d28b6451` 切出，切出时工作树干净（仅未跟踪目录 `analysis/`、`cache/`、`docs/team-collaboration/`、两个模型目录），无切片作业在跑。`analysis/` 当前**未入库**，本卡引用它时须注意该前提。

### Current Code Reality

九条 P0 候选于 2026-09-14 在 `d28b6451` 逐条复验，**无一被修复**：

- `json_value.cpp` 递归下降解析器 `parse_value ↔ parse_object/parse_array` 无深度计数；全文件 6 处 `depth` 全在 `dump_impl` 输出侧。
- `module.json.in` 的 `provides`/`produces` 缺 `slice.rgbwsvt` / `p0.rgbwsvt.1`，而 `ModuleInfo.cpp:41` 有，且 `ModuleInfoTests.cpp:118-125` 正面断言其存在。
- `version-manifest.json` `compatibility.contracts` 仍三项。
- `slicer_error_codes.json` 未声明 `PM-SLICER-VIEWDATA-SIMPLIFICATION`，该码在 `MeshSimplifier.cpp:156,166,304,319` 与 `SceneViewMeshBuilder.cpp:621` 对外返回。
- `CMakeLists.txt:2545` 注册 `ValidateModuleInfoContracts.py` 时只传 `--repo`，脚本第 270 行退化为 `moduleInfo = expectedInfo`，漂移断言恒假。
- `HostSliceProtocolRoute.cpp:27` 用 `moduleInfo.contains("slice.rgbwsvt")` 对整份 JSON 做子串匹配。
- `WorkerClient.h:95-98` 四个容器无上限。
- `WorkerJobService.cpp:470` 每作业无条件重跑 `--contract-info`。
- `config.h:448` `slicing_mode` 默认 `closed_mesh_scanline`，该分支走未经 MEMFLOW 有界化的 `sample_model_masks` 全层物化。

### Current State

P0-00 **已完成**：feature 分支已建、任务卡已立、两项前置排查已验、Debug 构建 PASS、全量回归基线已固化（246/7 失败/22.3 分钟）。P0-01..P0-08 全部 PROPOSED，**准备工作完毕，可开工**。

### Target State

- 畸形/深嵌套 JSON 在 DLL 内变成稳定错误码而非进程消失。
- 部署清单、版本清单、错误码表三处声明与运行时实际一致。
- 「运行时自述是否漂移」这条断言**真的会红**，且已用故意改名证明过。
- 宿主按 `provides` 数组精确判定能力，不再依赖子串。
- Worker 日志与进度有明确上界与溢出计数。
- 协商结果按 Worker 身份缓存（契约 §2 本就预期）。
- `slicing_mode` 省略时不再静默落到未根治的 Dense 路径。

### Historical State

既有 golden 包、通道语义、采样策略、`p0.rgbwsv.2` / `p0.rgbwsvt.1` 字节输出**全部不变**。本卡不触碰 `slicer.cpp` 的算法块，不动 `config.cpp` 的既有取值语义（P0-07 只加必填校验，不改默认行为的语义含义）。

### Pending Confirmation

- ~~P0-07 口径~~ **已裁定**（2026-09-14，用户选「改成必填」）：省略 `slicingMode` 即校验失败，不改默认值语义、不改既有行为。
- ~~`analysis/` 入库~~ **已裁定**（2026-09-14，用户选「随本分支入库」）：随 `codex/feature-p0fix-contract-robustness` 提交。
- 当前**无待确认项**。

### Risk Points

- **基线未固化就开工**：改完分不清新增失败与既有失败。故 P0-00 为硬前置，未完成不得进入 P0-01。
- **P0-04 修成「绿但仍空转」**：必须走「先证明它会红 → 补常量转绿 → 故意改名再红」三步，任一步不符合预期即不算完成。
- **P0-02 触发打包门禁**：已核实 `PackageSlicerModule.ps1:260-269` 只校验 schema/spi/dll/subprocess/buildConfig/version 六字段，不校验 `provides`/`produces`，补齐安全。
- **P0-03 与既有用例冲突**：需先确认无用例依赖「全部日志行都在」。
- **P0-01 阈值误伤**：需确认既有 fixture 无超深嵌套配置。
- **并行会话共享工作树**：本分支与其他会话共用同一工作树，开工前后各查 `git status`，不覆盖他人未提交改动。

### Files To Change

`src/slicer_core/json_value.cpp`（深度计数）、`src/slicer_module/module.json.in`、`version-manifest.json`、`contracts/slicer_error_codes.json`、`src/slicer_module/WorkerProtocol.{h,cpp}` 与 `WorkerClient.h`、`src/slicer_module/WorkerJobService.cpp`、`apps/slicer_ui_host_sim/HostSliceProtocolRoute.cpp`、`tests/stage14c_05/ValidateModuleInfoContracts.py`、`CMakeLists.txt`（探针目标与测试注册）、`src/slicer_core/config.cpp`（P0-07，待裁定）。新增少量负例用例。**不做全仓机械替换，不引入新依赖。**

### Verification Plan

构建目录固定 `build-slicesoft/main`（唯一产出过基线的目录；仓库根 `build/` 已知陈旧，禁止使用）。**构建与回归分开判定退出码**：先确认 `cmake --build` 退出码为 0，再相信任何 ctest 结果。每个任务独立验证，证据落 `output/p0fix/`（E 盘、已 gitignore）。收口时全量回归与 P0-00 基线逐条对照，**失败集不得新增**。提交前 `git status --short` 与 `git diff --check`。

### 开工前置排查结果（2026-09-14，静态实测）

两项在 Risk Points 里列出的风险已于开工前验掉，结论直接约束实现：

**排查 1 ── P0-01 深度阈值是否误伤既有 fixture：不会。**

扫描 `contracts/` + `samples/` + `tests/` 全部 JSON（括号计数法，跳过字符串内与转义字符）：

```
全仓 JSON 最大嵌套深度 : 9
出现位置               : contracts/slicesoft_build_manifest.schema.json
阈值 64 的安全余量     : 7.1 倍
```

**结论：阈值取 64 安全。** 不需要为既有配置放宽，也不需要做例外名单。

**排查 2 ── P0-03 是否有用例依赖容器内容：有，且约束了实现形状。**

| 依赖类型 | 位置 |
| --- | --- |
| `progressEvents.back().percent == 100` | `EngineConformanceGate.cpp:209`、`EngineConformanceSupport.cpp:445`、`tests/.../Main.cpp:629-630` |
| `stdoutLogLines` 整体相等 / 按下标取值 | `WorkerClientTests.cpp:193, 211-212` |
| 扫描 stdout 行提取子进程 PID | `WorkerClientTests.cpp:310, 349` |
| 容器 size 精确断言 | `WorkerClientTests.cpp:191-192`、`WorkerContractTests.cpp:155-157` |

**结论：环形缓冲必须【永远保留最新条目】。** 「保留最早 N 条、丢弃其余」的实现会同时打断三处 `.back()` 断言，并违反 `file_contract_v1` §4 的「terminal success emits percent=100 before process exit」不变量。

因此 P0-03 的实现形状被钉死为：**首 N 条 + 最新 M 条 + 中间丢弃计数**，且 `.back()` 恒为最新一条。上述用例全部是短作业（2 条进度、1–2 条日志），在 M=256 / 512 KB 的上限下不受影响，无需改动既有断言。


### P0-00 回归基线（2026-09-14，`d28b6451`，构建目录 `build-slicesoft/main`）

```
246 项注册 ｜ 239 通过 ｜ 7 失败（97%） ｜ 总耗时 1335.95 s（22.3 分钟）
```

**注册数 246** —— `AGENTS.md` 记的 234 与静态 `add_test` 计数 242 **均已过期**，以本次实测为准。

**既有失败集（7 项，冻结为对照基线，P0-08 收口时不得新增）**

| # | 测试 | 失败原因（实测输出） |
| --- | --- | --- |
| 14 | `slicer_stage14b_layering_feasibility_test` | `model.cpp acquired a project dependency outside the frozen base parser boundary`：`model/AssetReferencePath.h`、`ObjFaceParser.h`、`MtlMaterialParser.h`、`FrameGeometry.h` |
| 22 | `slicer_stage14c04_sync_capability_safety_test` | `scene.get_viewdata first poll was succeeded, expected failed` |
| 57 | `stage14f03_single_model_s1_gate` | 单模型 S1 门禁失败 |
| 119 | `stage16c06_bounded_support_shape_unit_tests` | `mask-only ConsumeLayer path performs no allocation` 断言失败 |
| 164 | `scene_layer_adapters_unit_tests` | `translation preserves local layer bytes and dimensions` 失败（UNIPATH 卡已记为既有失败） |
| 208 | `slicer_stage14e02_qt_host_boundary_test` | `HostMainWindow.cpp (516)` 超 500 行且不在债务台账 |
| 241 | `slicer_stage14e04d_dual_view_contract_test` | `missing texture silently became a gray model` |

**最慢 8 项**（决定回归周期，P0-08 若只需定向验证可据此裁剪）

```
538.5s hostflow_hd02_real_asset_matrix      112.8s stage14d08_r2_slice_executor_tests
 76.6s hostux_preset_after_import            74.8s hostux_restored_preset_after_import
 65.8s frame_production_tests                56.6s matvol_production_wiring_tests
 46.2s stage16_contact_leveling_diagnostic   38.3s hostflow_hd04_scene_refresh
```

#### 基线对 P0 的三条直接约束

1. **#208 钉死了一条红线**：`HostMainWindow.cpp` 已 516 行、超 G1 且未入台账。P0-05 改的是 `HostSliceProtocolRoute.cpp`，**绝不可往 `HostMainWindow.cpp` 加任何行**，否则把一条既有红灯推得更深。
2. **#22 与 P0 改动面相邻**：`scene.get_viewdata` 的同步能力安全语义已经在漂。P0-01（JSON 深度）与 P0-05（能力判定）都在这条链路附近，**改完必须确认 #22 的失败原因逐字未变**，不能变成另一条失败而被误判为「既有」。
3. **#14 是分层门禁红灯**：`model.cpp` 已越出冻结 base parser 边界。P0 不碰 `model.cpp`，但 P0-08 对照时须确认这条原因逐字未变。

#### 两条对既有分析结论的更正（已回写 `analysis/`）

- `analysis/04_` **F-06** 原写「分层名单没有自检」。**不准确**：`slicer_stage14b_layering_feasibility_test` 就是分层门禁，且**当前是红的**（#14）。准确说法是：自检存在但检的是「base parser 冻结依赖边界」，不检「`baseExactStems` 名单完整性」；且这条已红的门禁没人处理。
- `analysis/04_` **F-11** 原写行数门禁「只在 CTest 注册 `--self-test`，是静默债不是红灯」。**不准确**：Qt 宿主侧的行数门禁 `slicer_stage14e02_qt_host_boundary_test` **已注册进 CTest 且当前是红的**（#208）。准确说法是：仓库全扫描未进 CTest（静默），但宿主侧已进且已红。

## 任务清单

| 任务 | 状态 | 完成日期 | 实际验证 |
| --- | --- | --- | --- |
| P0-00 基线固化：Debug 构建 + 全量 ctest，记录失败集与耗时 | COMPLETE | 2026-09-14 | 构建退出码 0、0 error、205 目标；回归 246 项 / 7 既有失败 / 1335.95s，失败集与原因已逐条固化（见上）。证据 `output/p0fix/baseline-build.log`、`baseline-ctest.log` |
| P0-01 JSON 解析递归深度上限（F-33 🔴） | COMPLETE | 2026-09-15 | RAII `DepthGuard` 上限 64，27 插入 0 删除；新增 `json_parse_depth_unit_tests` 六组断言 PASS；**证伪通过**（上限临时改 8 → 恰好四条「应通过」用例转红，恢复 64 全绿）；全量回归 247 项 7 失败，与基线**逐条一致、零新增零消失** |
| P0-02 能力集受控修订（F-01/F-03/F-05/F-21） | COMPLETE | 2026-09-15 | **实际改动 10 个文件**（原估 6 个）。定向 6/6 全绿；全量回归 247 项 7 失败，与基线**逐条一致、零新增零消失**。⚠ 打包门禁未跑（`PackageSlicerModule.ps1` 的 `Config` 有 `ValidateSet("Release")`，Debug 构建跑不了）。⚠ 本项完成只等于**声明面自洽**，真实产物验证在 P0-04 |
| P0-03 Worker 日志与进度上界 + 溢出计数（F-12） | COMPLETE | 2026-09-15 | `AppendBounded` 四容器受限，`m_result` 上无裸 `push_back`；新增 `worker_protocol_bounds_unit_tests` 5 组断言 PASS；**证伪精准**（改成「丢最新」后恰好红在四条 `.back()` 断言）；全量回归 248 项，1 条 hostux 经定向复跑 117.21s PASS 证明为负载假失败，剔除后与基线一致 |
| P0-04 模块自述对拍测试接探针（F-02 🔴） | COMPLETE | 2026-09-15 | 三步验证完成：接真实探针绿 → 故意改能力名**红且诊断精确到 provides 列表** → 恢复绿且源文件与 HEAD 逐字节一致。静默降级分支已**删除**而非补参数。全量回归 247 项，剔除 2 条负载超时假失败后与基线一致 |
| P0-05 宿主改结构化能力判定 + 负例（F-04） | COMPLETE | 2026-09-15 | 新增 `ModuleProvidesCapability` 解析 `provides` 数组；新建 `hostflow_slice_protocol_route_tests` 6 组断言 PASS；**证伪精准**（退回裸子串后恰好红在三条结构化断言）；全量回归 249 项 7 失败，与基线**逐条一致、零新增零消失** |
| P0-06 合同协商按 Worker 身份缓存（F-20） | COMPLETE | 2026-09-15 | 按「路径 + mtime + size + **能力**」缓存于 `Implementation` 实例；Worker 链路 12/12 PASS；随 P0-05 同一轮全量回归零新增 |
| P0-07 `slicing_mode` 改为必填（F-22 🔴） | COMPLETE | 2026-09-15 | 解析层必填；**爆炸半径分三轮才探清**（34 份 sample 配置 + 9 处源码内字面量 + 1 处 schema v1 位置更正）；新增门禁 `slicing_mode_declared_contract_test` 按 schema 分档并四向证伪；全量回归 250 项 7 失败 1561.51s，与基线**逐条一致、零新增零消失** |
| P0-08 收口：全量回归对照 P0-00 基线 + 文档与卡状态同步 | COMPLETE | 2026-09-15 | 最终回归 250 项 / 7 失败 / 1561.51s，失败集与 P0-00 基线**逐条一致、零新增零消失**；`analysis/04` 已标注 10 条已修复并附 commit；`analysis/README` 增 §1.7 收口段 |

### P0-07 已裁定口径：改为必填

用户 2026-09-14 选定**方案 ②「改成必填」**，不选改默认值、不选内存预算准入。

实现边界（**不得越界**）：

- 只在 `config.cpp` 的配置校验处加一条「`slicingMode` 必须显式声明」，缺失即失败并返回稳定错误码。
- **不改** `config.h:448` 的默认值字面量本身，**不改** `closed_mesh_scanline` 与 `relief_heightfield` 任一分支的算法行为。
- 生产面零影响：90 份样例配置里 89 份、10/10 部署工艺预设均已显式写 `relief_heightfield`。
- 预期副作用：少量内部手写精简配置与 test fixture 可能需补一行显式声明。**属预期内改动，不算回归**，但须在 P0-08 的失败集对照里逐条点名，不得混进「既有失败」。

选它而不选①的理由：改默认值会让所有省略该字段的既有配置**换一条算法路径**，属行为变更，须重跑全套 golden 并证明零漂移；必填只是让省略从「静默落到坏路径」变成「明确报错」，语义不动。

### P0-01 收口证据（2026-09-15）

**实现**：`src/slicer_core/json_value.cpp` 的 `Parser` 增加 RAII `DepthGuard` 与 `depth_` 成员，`parse_object()` / `parse_array()` 各加一行守卫。上限 `kMaxParseDepth{64}`。**27 行插入、0 行删除**，不改任何既有逻辑分支。

守卫在**检查通过后**才自增，因此构造抛出时不留残值；析构负责回退，异常路径同样正确。

**证伪验证**（这条是重点，不是走过场）：

```
上限临时改为 8 → 重建 → ctest 转红，且【恰好】红在这四条：
    FAIL depth 16 array parses
    FAIL depth 16 mixed object/array parses
    FAIL depth 64 is accepted (at the limit)
    FAIL depth 64 object is accepted
  而「应当被拒绝」的用例仍然通过
恢复 64 → 重建 → 全绿
```

这证明守卫确实被调用、边界就在常量所指处、测试不会空转。

**新增用例** `tests/unit/json_parse_depth/Main.cpp`（目标与 CTest 同名 `json_parse_depth_unit_tests`，避免「ctest 名 ≠ target 名」的坑）六组断言：浅层不受影响、16 层通过、64/65 边界、对象嵌套同样计数、20 万层深嵌套必须是**可捕获异常而非栈溢出**、以及一万个各自 3 层的兄弟节点必须全过（专门验 RAII **递减**正确，「只增不减」的实现会在此露馅）。

**回归对照**

```
基线   246 项 / 7 失败 / 1335.95 s
本轮   247 项 / 7 失败 / 1296.54 s   （+1 为新增用例）
新增失败：0    消失失败：0    失败集逐条一致
```

**过程中的一处自伤，已修**：用 PowerShell `Set-Content -Encoding utf8` 改常量时给源文件加了 UTF-8 BOM，且 `git show > file` 的自查因重定向重编码给出了错误结论，是 `git diff` 首行 `+﻿#include` 才暴露。已用按字节精确控制的方式去 BOM 并保持 LF 行尾。**教训：改源码不得用 PowerShell 文本重定向或 `Set-Content`。**

### P0-02 收口证据（2026-09-15）

**最终改动 10 个文件**，原估 6 个。多出的四处全部是**撞红灯才发现**的：

| 文件 | 变更 | 怎么发现的 |
| --- | --- | --- |
| `contracts/slicer_module_info.schema.json` | 三处 `const` 放开 | 计划内 |
| `contracts/slicer_module_manifest.schema.json` | 两处 `const` 放开 | 计划内 |
| `contracts/slicesoft_version_manifest.schema.json` | `contracts` const 放开 | 计划内 |
| `src/slicer_module/module.json.in` | provides/produces 补齐 | 计划内 |
| `version-manifest.json` | contracts 补齐 | 计划内 |
| `tests/stage14c_05/ValidateModuleInfoContracts.py` | 三处常量同步 | 计划内 |
| **`cmake/SliceSoftVersion.cmake`** | `EQUAL 3`→`4` + 增 `compatibility_transfer_package` | **CMake 配置阶段 FATAL** |
| **`contracts/slicer_capability_dtos.json`** | 1.14→1.15，15→16 能力，workerOnly 补一项 | **定向测试红**（F-01/F-05 硬耦合） |
| **`contracts/slicer_capability_dtos.md`** | 版本与「15 项能力」表述同步 | 随上 |
| **`tests/contracts/VerifySlicerErrorCodes.cmake`** | `EQUAL 19`→`20` + 新码入必需列表 | **全量回归红** |

**回归对照**

```
基线   246 项 / 7 失败 / 1335.95 s
本轮   247 项 / 7 失败 / 1765.52 s
新增失败：0    消失失败：0    失败集逐条一致
```

**未完成的验证（如实记录，不当作已通过）**

- 打包门禁 `scripts/TestSlicerModulePackage.ps1` / `PackageSlicerModule.ps1` **未跑**：该脚本 `Config` 参数带 `ValidateSet("Release")`，只接受 Release，本轮 Debug 构建无法执行。
  已用**代码走查**确认其冻结校验只比对 `schema` / `spi` / `dll` / `subprocess.exe` / `buildConfig` 五个字段，**不含 `provides` / `produces`**（`PackageSlicerModule.ps1:262-266`），故本次改动理论上不触发该门禁——但**这是走查结论，不是实跑证据**，须在 Release 轨道补验。
- **真实产物验证仍缺**：本项只让声明面互相自洽。`pm_module_info` 的真实输出是否符合放开后的 schema，要等 P0-04 接上探针才知道。在此之前不得声称「已验证一致」。

**方法论教训（已写入决策文档）**

按文件名 grep 消费者会漏掉「以参数接收路径」的脚本——`VerifySlicerErrorCodes.cmake` 通过 `-DERROR_CODE_FILE=` 收路径，正文不出现文件名。修复后已补做系统性数量断言扫描，确认无其它遗漏。

顺带印证：`tests/hostflow/HostProfilePanelTests.cpp:103` 早已断言 `modulecapabilities.size() == 16`——**运行时侧一直是 16**，落后的确实是声明面。

### P0-04 收口证据（2026-09-15）

**核心改动不是加参数，是删掉静默降级分支**：

```python
# 旧：可选参数缺失 → 退化为自我比对 → "运行时自述漂移"断言恒真
moduleInfo = RunProbe(probePath) if probePath else expectedInfo

# 新：--config / --probe / --build-manifest 全部 required
#     未构建的那个 config 明确打印 SKIP，不得用期望值冒充真实产物
```

CMake 注册补 `--config $<CONFIG>`、`--probe $<TARGET_FILE:stage14c05_module_info_tests>`、`--build-manifest`。探针是**既有目标自带的 `--print-json`**，无需新建。

**避开的一个假失败**：脚本原以 `version-manifest.json` 的 `0.2.0-dev` 为比对基准，而真实探针输出 `0.2.467-dev`（带构建计数）。改为从 `slicesoft_build_manifest.json` 取已构建版本，否则会在 version 字段上红，与能力集无关。

**三步验证**

```
① 接真实探针         → PASS   （P0-02 的声明面改动经得起真实 DLL 输出检验）
② 改一个能力名为假名 → FAIL   "$.provides: [...16 项...] was expected"，诊断精确
③ 恢复               → PASS   且 ModuleInfo.cpp 与 HEAD 逐字节一致
```

**至此 P0-02 才算真正验证完成**——在 P0-04 之前它只是"声明面互相自洽"。

**回归对照（含两次污染的如实记录）**

第一轮全量回归出现 `#45 stage14d03_worker_contract_unit_tests` 失败，查时间戳定责为**我自己的证伪操作污染**：改坏 `ModuleInfo.cpp` 后只重建探针目标，版本计数跳动，其它链接 `slicer_module` 的测试仍挂旧二进制（10:48 vs 13:31）。已停掉该轮、全量重建 206 目标、重跑。

> **教训：证伪操作必须以全量重建收尾。** 残留的部分构建会污染后续全量回归的归因，且这种失败看起来像真回归，极易误判。

第二轮干净回归 247 项 9 失败，比基线多 2 条：

```
hostux_preset_after_import          170.48s FAIL     基线 76.63s PASS
hostux_restored_preset_after_import 180.03s TIMEOUT  基线 74.82s PASS
本轮总耗时 2306s vs 基线 1336s（慢 73%）
```

两条的**功能断言全部走完**（`PRESET_AFTER_IMPORT ready=1 button=1`、`PRE_SLICE ready=1`、`MULTILAYER_WORKSPACE_RESTORE_PASS`），只是没跑完就到点。定向复跑**两条均 PASS**（142.00s / 108.67s）。同时段机器上 `verysync` CPU=9960、两个 `ChatGPT`、`Feishu` 在抢资源。

**结论：剔除这 2 条负载超时假失败后，失败集与基线 7 项一致，零新增。** 但这不是"7 失败"，如实记为"9 失败，其中 2 条经定向复跑证明为负载假失败"。

> **教训：跑回归基准前必须先查机器负载。** 本轮我没查，导致多花一轮排查。

### P0-03 收口证据（2026-09-15）

**实现**：`WorkerProtocol.cpp` 增加 `AppendBounded` 模板，四个诊断容器全部受限，`WorkerRunResult` 增四个溢出计数字段。上界取 `progress 8+256` / `timing 4+64` / `log 32+2048`。

**形状由 P0-00 前置排查钉死，不是自由选择**：超限时删除的是 `headKept` 之后**最老**的一条，因此 `front()` 恒为最早、`back()` 恒为最新。理由是仓库中三处断言 `progressEvents.back().percent == 100`（`EngineConformanceGate.cpp:209`、`EngineConformanceSupport.cpp:445`、stage16 用例）外加 `file_contract_v1` §4 的终态不变量。

**证伪**：把 `AppendBounded` 改成"满了丢最新"后重建，测试**恰好**红在四条 `.back()` 断言：

```
FAIL back() is the newest event, not an old one
FAIL terminal percent=100 survives truncation (file_contract_v1 §4)
FAIL newest stdout line is retained
FAIL newest stderr line is retained
```

而上界与丢弃计数断言**仍然通过**——证明用例的鉴别力对准的正是"必须保留最新"这条唯一不能错的性质，而不是笼统地测"有没有上界"。恢复后按上轮教训**全量重建 207 目标 0 error**，`stage14d02` / `stage14d03` / 新用例全绿。

**回归对照**

```
基线   246 项 / 7 失败 / 1335.95 s
本轮   248 项 / 8 失败 / 1723.73 s（+2 为 P0-01 与 P0-03 的新用例）
新增 1 条：hostux_restored_preset_after_import（154.02 s FAIL，无失败信息）
定向复跑：117.21 s PASS  → 判为 F-41 负载假失败
剔除后失败集与基线 7 项一致，零新增
```

**残留未解**：`.tiff` 等单条日志行的**字节长度**未设上限，本次只按条数设界。引擎若发出超长单行仍可能占用可观内存。属已知残留，不在本项范围。

### P0-05 / P0-06 收口证据（2026-09-15，同一轮回归）

**批处理决策**：P0-05（宿主）、P0-06（模块）、P0-07（引擎）落在三个互不相交的层，故一次改完、各跑定向、合并成**一次**全量重建 + 回归，把三轮回归压成一轮。

**未开并行 agent**，依据是本轮实测的两条性质：F-42（部分构建产生伪装成真回归的版本红灯）与 F-41（负载已致假失败，本轮复现三次）。并行构建/回归只会让两类噪音叠加到无法定责。

#### P0-05

新增 `ModuleProvidesCapability`：解析 `pm_module_info` 的 `provides` 数组做精确元素匹配，替掉 `moduleInfo.contains("slice.rgbwsvt")` 的整份 JSON 子串匹配。

**证伪**：退回裸子串实现后，测试恰好红在三条结构化断言，而三条正例仍通过：

```
FAIL capability name outside provides must NOT count as support   ← F-04 的核心缺陷
FAIL malformed module info fails closed
FAIL non-array provides fails closed
```

**范围克制**：只修 F-04 指出的子串匹配。`p0.rgbwsv.2 → slice.rgbwsv` 这条路径**原本就不查 provides**，本次未给它新增检查——那属于行为变更，超出 F-04 范围。此不对称已知，留作后续。

#### P0-06

缓存键为「可执行文件路径 + mtime + size + **请求能力**」。

**键必须含能力**：`Negotiate` 返回的 `compatible` 是针对具体 `requirement` 算出来的，只按可执行文件缓存会让 `slice.rgbwsv` 的结果污染 `slice.rgbwsvt`。

**缓存挂在 `Implementation` 实例而非进程级静态**：`pm_create` 的 ABI 不变量要求同一进程内多个模块实例完全隔离，进程级缓存会破坏这条。因 `Run()` 是静态成员只拿得到 `execution`，给 `JobExecution` 加了 `owner` 回指针。

**协商失败不入缓存**，避免把一次偶发传输失败固化下来。

#### 回归对照

```
基线   246 项 / 7 失败 / 1335.95 s
本轮   249 项 / 7 失败 / 2232.80 s（+3 为 P0-01/03/05 的新用例）
新增失败：0    消失失败：0    失败集逐条一致
```

### P0-07 收口证据（2026-09-15）

`slicingMode` 从「省略即 `closed_mesh_scanline`」改为必填。改的是解析层
（`config.cpp`，`NormalizeConfigJson` 之后、取值之前），因为 `validate_slice_config`
只看得到结构体，此时默认值已填上、分不出「省略」与「显式声明」。

#### 为什么值得付这个代价

`validate_slice_config` 原本只兜住两种危险组合（`materialVolumePolicy` 与
`geometrySampling` 候选策略要求 relief）。但 `slicer.cpp` 另有四处按 `slicing_mode` 分叉
且**不在交叉校验覆盖内**：4454 支撑源层、4482 贴图、4493 材质角色映射、4504 列范围。
开了贴图却漏声明 `slicingMode` 的配置会静默走另一条路径，无任何报错——这正是 F-22 指的病。

#### 爆炸半径分四轮才探清

| 轮次 | 判据 | 怎么发现不够的 | 补齐 |
|---|---|---|---|
| 一 | 盘上 `.json`，要 `input.modelPath` **且** `output.packageDir` | 回归第 69 项 `experimental_config` 变红 | 34 份 |
| 二 | + 源码内字符串字面量 | 全量复核 124 份配置时发现漏网 | 9 处 / 7 文件 |
| 三 | 放宽为只看 `input.modelPath` | 全量回归点名 5 项 | 1 份（`openvdb/...disabled.json` 无 `output` 块） |
| 四 | **谁写出配置文件** | —（收敛） | 5 处程序化 `Json::object` |

前三轮错在同一处：盯着**谁加载**配置。但测试可以造好配置交给上层入口，由它内部加载——
按 `load_slice_config` 的调用方枚举必然漏掉这一类。**四轮全部靠红灯发现，无一靠事先枚举**。

第一轮的回归**作废重跑**：`output_resolution_config_unit_tests` 未捕获异常挂死
（CPU 0.078 秒不退出），带着已知坏 fixture 跑完的失败集没有归因价值，掐掉重来。
第四轮时做过一次静态枚举，放宽判据后得到 87 个可疑文件而实际只有 5 个真坏——
两头都不准。详见 F-43 / F-44 的方法论结论：**F-44 是 F-43 昂贵的原因**，
先修挂死，「跑一轮改一轮」就是最省的做法。

**未改的四面**（核实后确认本就合规，不是漏掉）：程序化 `Json::Object` 构造
（`WorkerPreflightExecutorTests::MakeProfile` 本就声明）、纯 C 宿主
`HostRequestBuilder.c`（规范与紧凑两份形式均已声明）、`apps/multi_model_scene_matrix`
（解析既有 scene 配置为 root，`slicingMode` 透传）、引擎侧 5 处（加载调用方传入的路径，
契约上由调用方负责）。

#### 一处自我更正：schema v1 的 slicingMode 不在顶层

给 `samples/configs/schema_v1/slicer_config_v1_basic.json` 补齐时误加在顶层。
`NormalizeSlicerConfig1` 只搬运白名单键，顶层 `slicingMode` 被静默丢弃——
**文件里看得见该键、门禁报绿，引擎仍判「未声明」并拒绝**。该文件本来就有
`pipeline.slicingMode`，原本就合规，是我多加的；已撤回。同样的错误也出现在
`material_closure_slicer_config_1_parses` 用例，已改为 `pipeline.slicingMode`。

由此立 **F-45**（v1 归一化静默丢弃白名单外全部键；`background` 与 `geometrySampling`
经 v1 无法表达且无提示）。

#### 另一处差点出事：门禁脚本会被 .gitignore 静默忽略

`ValidateSlicingModeDeclared.py` 写好、CTest 也注册了，提交前才发现
`.gitignore:18` 的 `tests/*` 把 `tests/contracts/` 整个挡在外面——**已跟踪文件不受影响，
所以现存 28 个契约测试照常在库，新增的却会被静默忽略**。若就这样提交，
我本地全绿、任何人拉下来 `slicing_mode_declared_contract_test` 必红，且红灯信息是「文件不存在」。

实测 `tests/` 下 35 个含已跟踪文件的目录里，**25 个有此陷阱**；`tests/contracts/`
已因此丢过两个脚本（`RunSceneFacade14B03Tests.ps1`、`ValidateSceneFacade14B03.py`，
属 PRESET 专项）。本次只解开 `tests/contracts/`，那两个孤儿因此从隐形变为可见的未跟踪状态，
**不代 PRESET 专项入库**。其余 24 个目录另立 **F-46**。

#### 新增门禁

必填是靠一次性补齐跟上的，没有门禁会慢慢烂掉。新增
`slicing_mode_declared_contract_test`（`tests/contracts/ValidateSlicingModeDeclared.py`）：

- 按 schema 分档指认正确位置（legacy 查顶层，`slicer.config.1` 查 `pipeline.slicingMode`）
- **反向**拒绝「v1 配置带被忽略的顶层 `slicingMode`」——即我上面犯的那个错
- 自带空转自检：发现数为 0 即判门禁失效（判据漂移时不会假绿）

**四向证伪通过**：基线绿（124 份）→ legacy 缺字段红 → v1 缺 `pipeline` 红 →
v1 多余顶层键红 → 非法取值红 → 还原绿。

#### 报错文案

这是对既有配置的破坏性变更，报错直接给出改法：

```
slicingMode must be declared explicitly: closed_mesh_scanline or relief_heightfield
(configs written before this became mandatory were slicing as closed_mesh_scanline,
so declare that to keep the previous behaviour)
```

#### 验证

- 定向 11/11 PASS（含新门禁与全部受影响用例）
- 产品侧双向：补齐配置正常推进到 `mask_sampling`；缺字段退出码 1 且信息明确
- 全量回归：全量回归 250 项 7 失败 1561.51s，与基线**逐条一致、零新增零消失**

### P0-08 收口证据（2026-09-15）

#### 最终回归对照

以 P0-00 基线（246 项 / 7 失败 / 1335.95s）为唯一归因参照，逐条比**集合**而非比数量：

```
基线失败集（7）                         最终失败集（7）
scene_layer_adapters_unit_tests          同
slicer_stage14b_layering_feasibility_test 同
slicer_stage14c04_sync_capability_safety_test 同
slicer_stage14e02_qt_host_boundary_test  同
slicer_stage14e04d_dual_view_contract_test 同
stage14f03_single_model_s1_gate          同
stage16c06_bounded_support_shape_unit_tests 同

新增失败：0    消失失败：0
最终规模：250 项 / 7 失败 / 1561.51s（+4 项为本专项新用例与新门禁）
```

**比集合而非比数量救过一次**：P0-07 倒数第二轮也是「7 条基线 + 5 条新增 = 12」，
若只看数量会以为差 5 条；但其中 `x_origin_padding_tests` 我一度凭印象当成基线项，
按集合比对才暴露它其实是新增的。

#### 本专项修掉的 10 条

| 任务 | 问题 | commit |
|---|---|---|
| P0-01 | F-33 JSON 解析无深度上限 | `554847f3` |
| P0-02 | F-01 / F-03 / F-05 / F-21 能力集与错误码契约 | `768a0de6` |
| P0-03 | F-12 Worker 日志无界累积 | `6299f0d0` |
| P0-04 | F-02 模块自述对拍测试整条空转 | `4596a771` |
| P0-05 | F-04 宿主裸子串判定能力 | `0a261fbd` |
| P0-06 | F-20 合同协商每作业重跑 | `b918e0ba` |
| P0-07 | F-22 `slicing_mode` 默认值可静默落入 | `bccec8c7` |

含 `analysis/04` 当初列为「最该先做的三条」的全部三条（F-33 / F-22 / F-02）。

#### 新立 6 条（F-41~F-46）

专项期间新立 6 条，**净增不等于变差**——这些问题本来就在，只是此前无人撞到。
其中优先级最高的是 **F-44**（单测未捕获异常挂死）：它是 F-43（配置来源面分散）
代价高昂的直接原因，修掉它之后「跑一轮回归、按它点名的修」重新成为最省的做法。

#### 本专项未动的

引擎内部的债一条未碰：F-09（`slicer.cpp` 5888 行）、F-10（单一 SliceConfig 39 结构体）、
F-07（RIP 未走 SPI）、F-16（无 CI）、F-17（无测试框架）。P0FIX 做的全是契约与工装一侧。
F-41 / F-42 的处置（抬 TIMEOUT、把「验证前必须全量重建」写进 AGENTS.md）仍待裁定。

## 修订记录

- 2026-09-15：**P0-08 收口完成，P0FIX 专项九项任务全部 COMPLETE**。最终回归失败集与 P0-00 基线逐条一致。

- 2026-09-15：**P0-07 完成**。爆炸半径分三轮探清，含一处 schema v1 的自我更正；新增按 schema 分档的门禁；由此立 F-43 / F-44 / F-45。

- 2026-09-15：**P0-05 与 P0-06 完成**，同一轮全量回归验证，零新增。采用批处理而非并行 agent，依据见上。

- 2026-09-15：**P0-03 完成**。证据见上。实现形状由 P0-00 的前置排查决定而非自由设计；证伪证明用例鉴别的是「保留最新」而非「有上界」。回归新增 1 条经定向复跑定责为 F-41 负载假失败。

- 2026-09-15：**P0-04 完成**。证据见上。P0-02 至此获得真实产物验证。过程中自造两类假失败（证伪残留、负载超时），均已定责并记录教训。

- 2026-09-15：**P0-02 完成**（含 F-05，范围经用户裁定扩大）。证据见上。原估 6 个文件实际 10 个，其中三处靠红灯发现，已在 `DOC_DECISION_P0FIX_R1` 的「实施中的三处更正」逐条记账。打包门禁因只接受 Release 未跑，如实留作待补验项。

- 2026-09-15：**P0-01 完成**。证据见上。同时把 P0-02 由「三处声明补齐」重新定性为「能力集受控修订」——开工前核查 schema 发现三份 schema 用 `const` 钉死 15 能力集，原「补两行」的判断是错的，详见 `docs/slice/DOC/DOC_DECISION_P0FIX_R1_模块自述与部署清单能力集受控修订.md`。

- 2026-09-14：**P0-00 完成，准备工作收口**。全量回归 246 项 7 失败（97%）、1335.95s。失败集与逐条原因已冻结为对照基线。基线顺带产出三条对 P0 的硬约束与两条对 `analysis/` 既有结论的更正（均见上）。注册数实测 246，`AGENTS.md` 的 234 已过期。

- 2026-09-14：P0-00 构建段完成。`build-slicesoft/main` Debug 构建退出码 0、0 error、205 个目标链接成功。附带确认一处易误判：宿主可执行文件已按 `279ec248` 改名为 `slice_soft_test.exe`（日志 `Preserving the legacy host executable entry point`），`slicer_ui_host_sim.exe` 不存在**不是**构建失败。全量回归随后启动，结果与失败集另记。
- 2026-09-14：两项待确认已裁定 —— P0-07 取「改为必填」；`analysis/` 随本分支入库。卡内 Pending Confirmation 清空。
- 2026-09-14：开工前置排查两项完成（见上）。排查 1 证明深度阈值 64 对既有 fixture 有 7.1 倍余量；排查 2 发现三处 `progressEvents.back()` 断言与 `file_contract_v1` §4 终态不变量，**把 P0-03 的环形缓冲形状钉死为「必须永远保留最新条目」**——这条约束是排查出来的，不是设计偏好。
- 2026-09-14：建卡。来源为 `analysis/` 专项 36 条发现中的 P0 层。本卡承接状态真源职责，`analysis/` 保持分析产物定位。P0-07 因需产品口径裁定列 INPUT_OPEN，不自行选定。`analysis/` 当前未入库，是否入库待用户裁定。分支 `codex/feature-p0fix-contract-robustness` 自 `d28b6451` 切出，切出时工作树干净、无作业在跑。
