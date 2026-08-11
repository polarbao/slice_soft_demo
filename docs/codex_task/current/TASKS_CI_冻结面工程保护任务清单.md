# TASKS_CI 冻结面工程保护任务清单

> 文档状态：⏸ **DEFERRED — 用户 2026-08-10 裁决暂缓建设**
> 版本：v1.2 ｜ 日期：2026-08-11

> ⏸ **2026-08-10 用户裁决：暂缓 CI 构建，先做其他任务。**
> 本清单**保持有效但不开工**，`C-A-01`（修绝对路径）除外 —— 它是独立缺陷，
> 换机器/容器即失败，与 CI 是否建设无关，已并入 `TASKS_TIFF` 的开工前置。
>
> ⚠️ **暂缓期间新增一条风险登记**：R-B-02 已把 `meshoptimizer` 链入 `slicer_base`
> → **进入了已冻结的 `slicer_module.dll`**。冻结面（11 导出 / 15 能力 / 12 份合同哈希）
> 在无 CI 的情况下**继续裸奔**，且现在多了一个第三方依赖。
> 这不构成"必须立刻做 CI"，但**恢复 CI 建设时 `C-B-01` 的优先级应更高**。
> 定位：独立补充专项，不占阶段编号
> 起因：打印侧 `CLD_42` §4.2 T-07 —— 接口已冻结但冻结面无自动化保护
> 证据等级：A=已核实事实，B=目标设计，P=判断

---

## 0. 为什么必须做（A 级）

**仓库当前零 CI 配置。** 已核实不存在：

```text
.github/workflows/     ✗      .gitlab-ci.yml   ✗
azure-pipelines.yml    ✗      Jenkinsfile      ✗      .gitea/     ✗
```

而 Stage 14 已经冻结了：

```text
PM_SPI_VERSION = 1        11 个 pm_* 导出符号        15 项能力
12 份合同哈希              p0.rgbwsv.2 协议           46 文件移植清单
```

**冻结的意义在于「变了能被发现」。零 CI 意味着这些冻结只是文档承诺，不是工程约束。**
当前 201 个 ctest 用例 + 约 30 个 PowerShell 脚本全靠人工在本地跑 —— 谁忘了跑，谁就静默破坏。

## 1. 任务卡

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **C-A-01** | **修阻塞 CI 的绝对路径**：`samples/configs/material_process/obj_mtl_texture_rgb_varnish.json:22` | 无 | 该配置的模型路径改为仓库相对路径；换机器/容器不再依赖历史绝对路径 | **COMPLETE（2026-08-11，并入 T-A-01）** |
| **C-A-02** | **盘点可自动化范围**：201 个 ctest 用例逐个标注「可无人值守 / 需 GUI / 需真实设备 / 需外部 RIP」四类；约 30 个 PS 脚本同样分类 | 无 | 输出机器可读清单；**明确哪些永远进不了 CI**（实物打样、目标 RIP、干净机）| **PROPOSED** |
| **C-A-03** | **最小流水线落地**：Windows runner + MSVC + Debug/Release 两配置，跑 C-A-02 判定为「可无人值守」的全部用例 | C-A-01、C-A-02 | 一次 push 触发即出结果；失败可定位到具体用例 | **PROPOSED** |
| **C-B-01** | 🔴 **冻结面守卫**（本专项的真正目的）：断言 `PM_SPI_VERSION=1`、`slicer_module.def` 恰好 11 个符号、`module.json` 恰好 15 项能力、12 份合同哈希不变 | C-A-03 | 人为改动其中任一项，CI **必须红**；守卫自身有反向测试证明它会红 | **PROPOSED** |
| **C-B-02** | **既有门禁脚本接入**：`ValidateCapabilityDtos.py`、`ValidateThreeLaneContract.py`、`ValidateQtHostBoundary.py`、`ValidateSourceSizeGuard.py`、`ValidateHostflowMigrationInventory.py` | C-A-03 | 全部在 CI 内执行；宿主边界与源码行数门禁生效 | **PROPOSED** |
| **C-B-03** | **golden 漂移守卫**：生产 TIFF / Package 的 SHA-256 基线校验 | C-A-03 | 产物字节变化必须显式失败并给出 diff 摘要；⚠️ 与 `TASKS_TIFF` 的 T-A-03 重固化协同 | **PROPOSED** |
| **C-C-01** | **构建矩阵扩展**：`SLICESOFT_TIFF_BACKEND` 两值 × Debug/Release；`meshoptimizer` 静态链接可复现性 | C-A-03 | 四组组合均能构建并通过对应用例 | **PROPOSED** |

## 2. 🔴 三条实施纪律

### 2.1 不得为了让 CI 变绿而放松门禁

**这是本专项最大的失败模式。** 若某用例在 CI 环境失败，正确处理是**修环境或标注为不可自动化**，
不是降低断言。特别注意 `DOC_DECISION_14F` §48-52 的既有纪律：
**禁止把 `EXTERNAL_VALIDATION_DEFERRED` 记作 PASS。**

CI 里必须把「未运行」与「通过」显式区分，不得用 skip 伪装成绿。

### 2.2 先做守卫，再做覆盖率

C-B-01 的冻结面守卫**比把 201 个用例全接进去更重要**。
理由（P）：201 个用例大多测的是功能正确性，本地跑得挺勤；而**冻结面破坏是静默的** ——
改了 `.def` 少一个符号，功能测试可能全绿，打印侧装载时才炸。
所以顺序是 `C-A-03 最小可跑` → `C-B-01 守卫` → 再逐步扩覆盖。

### 2.3 CI 不是拆 `slicer.cpp` 的许可证

`CLD_42` §5 判断「等 CI 就位后再拆 `slicer.cpp`」，方向对但**不要理解成 CI 一绿就能动**。
拆单体还需要：Stage 14 外部验收通过 + `slicer.cpp` 所在的 `slicer_engine` 有独立的
行为等价性测试。**CI 是必要条件，不是充分条件。**

## 3. 一个需要用户决定的前置

**CI 跑在哪里？** 这决定 C-A-03 的形态，我无法替你判断：

```text
甲 · GitHub Actions（windows-latest）
     免费额度够用；但需要仓库能推到 GitHub，且 vcpkg 依赖拉取受网络影响
乙 · 自托管 runner（你自己的 Windows 机器）
     环境可控、有 MSVC 与 vcpkg 缓存、可跑 GUI self-test；但要维护机器
丙 · 本地 pre-push hook + 脚本
     零基础设施，立刻可用；但【不是 CI】—— 绕过 hook 就没了，只能算过渡
```

**建议乙**，理由（P）：本项目有 GUI self-test、Qt 依赖、vcpkg 静态链接、
以及需要真实模型资产（`model/obj/**` 36 个 OBJ）的门禁 —— 托管 runner 每次拉环境会很慢且脆。
自托管一次配好，后续跑得快也稳。⚠️ 若你希望打印侧也能看到 CI 结果，则甲更合适。

## 4. 边界

```text
✅ 本专项【只加自动化】，不改任何生产代码逻辑（C-A-01 的路径修正除外）
✅ 不放宽任何既有门禁阈值
⛔ 不把实物打样 / 目标 RIP / 干净机验证接进 CI —— 它们物理上不可自动化，
   必须在 C-A-02 中显式标注为「永久人工」
```

## 5. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-11 | v1.2 | 回填 C-A-01 已随 T-A-01 完成：`obj_mtl_texture_rgb_varnish.json` 改用仓库相对模型路径。CI 主体仍按用户裁决保持 DEFERRED，不据此启动 C-A-02 及后续任务。 |
| 2026-08-10 | v1.0 | 首版。核实仓库零 CI 配置；立 C-A（基础流水线，3 卡）、C-B（冻结面守卫，3 卡）、C-C（构建矩阵，1 卡）；写入三条实施纪律（不得为变绿放松门禁 / 先守卫后覆盖 / CI 不等于拆单体的许可）；提出 runner 形态待用户决定并建议自托管 |
