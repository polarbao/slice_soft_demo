# DOC_DECISION_P0FIX-R1 模块自述与部署清单能力集受控修订

## Status

**ACCEPTED / USER AUTHORIZED**

> 日期：2026-09-15
> 关联任务：P0FIX P0-02（卡 `docs/codex_task/current/TASKS_P0FIX_分析专项P0契约一致性与输入加固.md`）
> 上游：`DOC_DECISION_MATVOL_T_冻结契约file_contract_v1变更处置.md`（方案 A：rgbwsvt 为可选附加声明）
> 来源：`analysis/04_问题清单与改动空间.md` F-01 / F-02 / F-03
> 授权记录：用户于 2026-09-15 在「运行时 `pm_module_info` 输出违反自己的 schema，怎么处置」一问中明确选择**「契约追上现实」**。

## Context

### 发现经过

P0-02 原本被登记为「补两行部署清单」的 S 级改动。开工前核查 schema 时发现判断有误：**三份 schema 都用 `const` 把 Stage 14 的 15 能力集钉死了**。

| Schema | 被 `const` 钉死的字段 | 含 rgbwsvt |
| --- | --- | --- |
| `contracts/slicer_module_manifest.schema.json` | `provides`（15 项）、`produces`（1 项） | ❌ |
| `contracts/slicer_module_info.schema.json` | `provides`（15 项）、`produces`（1 项）、`capabilities.workerCapabilities`（3 项） | ❌ |
| `contracts/slicesoft_version_manifest.schema.json` | `compatibility.contracts`（3 项） | ❌ |

与之完全一致的数据面：`src/slicer_module/module.json.in`、`version-manifest.json`、`tests/stage14c_05/ValidateModuleInfoContracts.py` 的 `EXPECTED_PROVIDES` / `EXPECTED_SYNC` / `EXPECTED_WORKER`。

**而另一侧已经越过这条线**：`src/slicer_module/ModuleInfo.cpp:41` 的运行时自述字符串输出 `slice.rgbwsvt`、`p0.rgbwsvt.1` 与 `workerCapabilities` 里的 `slice.rgbwsvt`；`tests/stage14c_05/ModuleInfoTests.cpp:118-125` **正面断言**它必须输出这三项；`file_contract_v1` 已到 minor 1；`CapabilityCarrierRouter`、Worker dispatcher、宿主 `HostSliceProtocolRoute` 全部按支持 rgbwsvt 实现。

### 由此得出的事实

```
真实 DLL 的 pm_module_info 返回值，违反它自己的 slicer_module_info.schema.json
  provides const 含 slice.rgbwsvt : False（schema 只允许 15 项）
  produces const 含 p0.rgbwsvt.1  : False
  workerCaps const 含 rgbwsvt     : False
  而 ModuleInfo.cpp 三项全部输出了
```

这不是「某个人忘了同步」。这是**两个各自内部自洽的阵营**：声明面（三份 schema + 两份数据 + 一组 Python 常量）停在 Stage 14 冻结集，实现面（C++ 运行时串 + 它的 C++ 断言 + 私有契约 + 路由 + 宿主）走到了 rgbwsvt。

### 为什么一直没被发现

`ValidateModuleInfoContracts.py` 里的 `ExpectValid(infoValidator, moduleInfo, ...)` 本来就会抓到这条违规。但 `CMakeLists.txt` 注册该测试时**没有传 `--debug-probe` / `--release-probe`**，脚本第 270 行因此退化为 `moduleInfo = expectedInfo`——它校验的是 Python 常量，不是真实产物。

**所以 F-02 不只是「一条不测漂移的测试」，它是一条正在掩盖实时 schema 违规的测试。** 这比 `analysis/04_` 原文的定性严重一级，已在该文回写更正。

## Decision

**让声明面追上实现面。** SPI major 保持 1，11 个导出保持不变，通道语义、包字节、采样策略、ABI 一律不动；仅把三份 schema 的 `const` 与对应数据面放开到**实现面早已在用**的集合。

### 目标集合

```
provides            15 项 → 16 项（在 slice.rgbwsv 之后插入 slice.rgbwsvt）
produces             1 项 →  2 项（增 {"contract":"p0.rgbwsvt.1","kind":"package"}）
workerCapabilities   3 项 →  4 项（增 slice.rgbwsvt）
compatibility.contracts  3 项 → 4 项（增 p0.rgbwsvt.1）
syncCapabilities    13 项，不变
```

### 变更清单

| 文件 | 变更 |
| --- | --- |
| `contracts/slicer_module_info.schema.json` | `provides` / `produces` / `capabilities.workerCapabilities` 三处 `const` 放开 |
| `contracts/slicer_module_manifest.schema.json` | `provides` / `produces` 两处 `const` 放开 |
| `contracts/slicesoft_version_manifest.schema.json` | `compatibility.contracts` 的 `const` 放开 |
| `src/slicer_module/module.json.in` | `provides` 补 `slice.rgbwsvt`；`produces` 补 `p0.rgbwsvt.1` |
| `version-manifest.json` | `compatibility.contracts` 补 `p0.rgbwsvt.1` |
| `tests/stage14c_05/ValidateModuleInfoContracts.py` | `EXPECTED_PROVIDES` / `EXPECTED_WORKER` 与 `BuildModuleInfo` 的 `produces` 同步 |
| `src/slicer_module/ModuleInfo.cpp` | **不改**。它是本次修订要对齐的基准 |

## Boundaries

以下一律不在本次修订范围内，改动任何一项都需要另行裁定：

- SPI major、`PM_SPI_VERSION`、11 个 `pm_*` 导出、`slicer_module.def`
- 通道语义、`black_is_print` 极性、`p0.rgbwsv.2` / `p0.rgbwsvt.1` 的包字节
- `file_contract_v1` 的 major/minor 与协商规则
- ~~`slicer_capability_dtos.json`——F-05 另行修订，本次不动~~ **该边界已于 2026-09-15 撤销，见下方「实施中的两处更正」**
- `maxConcurrentJobs` 仍为 1（F-13 不在本次范围）
- `ModuleInfo.cpp` 的输出内容

## 实施中的三处更正（2026-09-15）

本节记录实施过程中推翻本文档原始判断的三件事。**不修饰、不事后合理化。**

### 更正一：变更清单漏了 CMake 硬断言，实际是七处不是六处

`cmake/SliceSoftVersion.cmake:126` 有一条独立于所有 schema 的硬断言：

```cmake
if(NOT compatibility_count EQUAL 3)
    message(FATAL_ERROR
        "version-manifest.json must declare exactly three frozen compatibility contracts")
endif()
_slicesoft_version_json_get(compatibility_spi     "${manifest}" compatibility contracts 0)
_slicesoft_version_json_get(compatibility_worker  "${manifest}" compatibility contracts 1)
_slicesoft_version_json_get(compatibility_package "${manifest}" compatibility contracts 2)
```

它不只计数，还**按下标位置**取出三项并逐一比对字面量。给 `version-manifest.json` 加第四项后，**CMake 配置阶段直接 FATAL**，构建根本起不来。原文档的「变更清单」六行表格因此是不完整的。

已改为 `EQUAL 4` 并增加 `compatibility_transfer_package` = `contracts[3]`，值断言同步加一条 `p0.rgbwsvt.1`。**保持同样的「冻结且精确」强度**，只是从三项变四项。

### 更正二：「F-05 本次不动」这条边界不成立，已撤销

原文档 Boundaries 一节写了「`slicer_capability_dtos.json` 是另一条受控修订，本次不动」。**这句话是错的。**

`tests/stage14c_05/ValidateModuleInfoContracts.py:155-159`：

```python
def ValidateFrozenSources(repo):
    dto = LoadJson(repo / "contracts/slicer_capability_dtos.json")
    dtoCapabilities = tuple(item["id"] for item in dto["capabilities"])
    if dtoCapabilities != EXPECTED_PROVIDES:
        raise AssertionError("15-capability DTO source drifted")
```

它拿 **DTO 的能力 id 序列直接比对 `EXPECTED_PROVIDES`**。只要 `EXPECTED_PROVIDES` 变成 16 项而 DTO 停在 15 项，这条断言必红。**F-01 与 F-05 是硬耦合的，不存在只改一个的选项。**

用户 2026-09-15 裁定扩大范围至含 DTO。实际变更：

```
contracts/slicer_capability_dtos.json   contractVersion 1.14 -> 1.15
                                        capabilities 15 -> 16（slice.rgbwsvt 插在第 11 位）
                                        protocolInvariants.workerOnly 2 -> 3 项
contracts/slicer_capability_dtos.md     合同版本与「15 项能力」表述同步
tests/contracts/ValidateCapabilityDtos.py  版本号、两处 15 计数、workerOnly 集合同步
```

`slice.rgbwsvt` 条目由 `slice.rgbwsv` 条目**文本级复制**后只改三处常量（`id`、`capability` const、`output.contract` const）生成，因此请求/响应/错误字段与 rgbwsv **逐项相等**（12/20/7），不存在手写遗漏。

`protocolInvariants` 的 `packageSchema` 与 `channels` **保持不变**：核查确认该块只有一个消费者（`ValidateCapabilityDtos.py`），且 rgbwsvt 的包契约已由其条目内的 `output.contract` const 承载，无需再造新的不变量字段。

### 更正三：错误码数量也被 CMake 硬钉，且我的消费者扫描方法本身有缺陷

F-21（补 `PM-SLICER-VIEWDATA-SIMPLIFICATION`）原判定为「安全，因为读 `slicer_error_codes.json` 的两个校验器都是子集检查」。**这个判定错了**，全量回归报出新增失败：

```
tests/contracts/VerifySlicerErrorCodes.cmake:13
    Expected 19 registered error codes, found 20
```

错误码**数量**也被一条 CMake 脚本硬钉死。

**为什么之前的扫描漏了它**：我按文件名 `slicer_error_codes.json` grep 消费者，只命中两个 Python 校验器。而这条脚本通过 `add_test` 的 `-DERROR_CODE_FILE=...` **以参数接收路径**，脚本正文里根本不出现该文件名。

> **方法论教训：按文件名 grep 会漏掉「以参数接收路径」的消费者。** 契约类文件的消费者扫描必须同时查 `add_test`／脚本调用处传入的变量，不能只查文件名字面量。

**处置**：`VerifySlicerErrorCodes.cmake` 的 `EQUAL 19` 改 `EQUAL 20`，状态消息同步；并把 `PM-SLICER-VIEWDATA-SIMPLIFICATION` **加进该脚本的必需码列表**——只放宽计数而不加保护，等于把新码降格成不受约束的一条。`contractVersion` 保持 `1.0`（该字段在此处是 schema 形状版本，不随码表增减而变）。

随后做了一次**系统性**的数量断言扫描（覆盖 `.py` / `.cmake` / `.cpp` / `.ps1`，按上下文过滤与能力集/错误码/契约集相关者），确认无其它遗漏。顺带确认 `tests/hostflow/HostProfilePanelTests.cpp:103` 早已断言 `modulecapabilities.size() == 16`——**运行时侧一直是 16，再次印证落后的是声明面而不是实现面**。

### 这两处更正说明的问题

本文档原始版本把这次修订估成「六个文件」，实际做下来是**十个文件**，且其中三处是撞到红灯（CMake FATAL、DTO 交叉断言、错误码计数）才发现的，不是事先查出来的。这正是 `analysis/05_` §1 实测的「加一个能力要手工同步 17 处」在本次的具体兑现，也再次印证 R-10「能力注册表单一真源」才是根治方向——**本次修订仍然是止血**。

## Consequences

**正面**

- 部署清单、运行时自述、版本清单三者与实现一致，按 `module.json` 做能力发现的消费者（打包流程、未来的 PrintApp、第三方审计）不再被误导为「本模块不支持 RGBWSVT」。
- P0-04 接上探针后，`ExpectValid` 与漂移断言才可能真正通过；否则接上就是红灯。

**需要正视的代价**

- 本次修订**把一条已存在的违规合法化**，而不是修复一个新引入的缺陷。RGBWSVT 自 minor 1 起就已在生产路径上运行，声明面滞后期间对外的描述是错的。这一点必须记录，不能事后描述成「一直是一致的」。
- 放开 `const` 后，这四组集合失去「只能是这个值」的强约束，退化为「必须包含这些值」。**这是刻意的取舍**：`const` 的强度在这里没有换来保护，只换来了「实现面绕过声明面」。真正的根治是 `analysis/05_` §5 与 `06_` R-10 的**能力注册表单一真源**，把这些集合从手写改为生成。本次修订是止血，不是根治。

## Verification

1. 六个文件改完后，`ctest -R stage14c05`、`-R version`、`-R capability_dtos` 全绿。
2. `scripts/TestSlicerModulePackage.ps1` 打包验证通过。
3. **关键验证在 P0-04**：接上 `--debug-probe` / `--release-probe` 后，`ValidateModuleInfoContracts.py` 必须用**真实 DLL 输出**跑通 `ExpectValid` 与 `moduleInfo == expectedInfo`。在此之前，本次修订只能算「声明面自洽」，不能算「已被真实产物验证」。
4. 全量回归与 P0FIX 的 P0-00 基线（246 项 7 既有失败）逐条对照，失败集零新增。

## Related

- 卡：`docs/codex_task/current/TASKS_P0FIX_分析专项P0契约一致性与输入加固.md`
- 分析：`analysis/04_问题清单与改动空间.md` F-01 / F-02 / F-03 / F-05
- 根治方向：`analysis/06_改进路线图与验证方案.md` R-10 能力注册表单一真源
