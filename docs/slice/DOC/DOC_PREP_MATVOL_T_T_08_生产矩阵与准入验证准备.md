# DOC_PREP MATVOL-T T-08 生产矩阵与准入验证准备

> 状态：COMPLETE  
> 日期：2026-08-26  
> 分支：`codex/matvol-t-channel-protocol`

## 1. Problem Type

T-08 是生产协议验证与回归准入任务，不改变材质识别业务定义。目标是在同一 Release 构建中闭合
03 正例、08/09 预期拒绝、无 T、坏 T、Package、内部 strict RIP、取消清理及资源观测矩阵。

## 2. Layers

```text
配置/真实资产 -> Legacy RGBWSVT 切片 -> Package 严格 Reader
              -> rip_reader_test 双协议入口 -> Release 证据脚本
```

T-08 允许补齐 Reader CLI、测试和证据脚本；不修改 08/09 资产，不改变旧 `p0.rgbwsv.2` Reader，
不在本卡授予生产准入。

## 3. Official Docs

- `DOC_DECISION_MATVOL_T_RGBWSVT协议与缩裹材料通道.md`
- `DEV_MATVOL_T_可配置缩裹识别与RGBWSVT双协议设计.md`
- `contracts/p0.rgbwsvt.1.schema.json`
- `.agents/docs/build-and-test.md`

## 4. Historical Docs

- `REPORT_MATVOL_T_RGBWSVT缩裹材料通道当前状态.md`
- `TASKS_MATVOL_T_RGBWSVT缩裹材料通道任务清单.md` 中 T-00..T-07 实际结果

历史候选结果只用于选择回归入口，不替代本卡 Release 实测。

## 5. AI Workspace Evidence

多 Agent 只读审计用于核对现有覆盖和 T-09 后续准入边界；最终实现、命令退出码和状态更新由当前
工作树统一复核。Agent 结论不得直接写成 PASS。

## 6. Current Code Reality

- 03 已有真实资产 T 排他合成、三类甲片工艺和逐层 TIFF 字节回读覆盖。
- 08/09 已作为 `TopologyInvalid` 负例，且用户明确不修复。
- 七通道 stripped/tiled 与 none/packbits 已有 Writer/Reader 单测。
- Package Query 已使用 `ValidateRgbwsvtPackage` 严格校验七通道包。
- `rip_reader_test` 当前仅路由 `p0.rgbwsv.1/.2`，尚未接入七通道严格 Reader。
- 候选包 guard 可在异常时递归清理，但尚缺切片中途取消的包目录断言。
- CLI 已输出 `SLICE_TIMING` 与 `peakWorkingSetBytes`，可用于同机同配置相对观测。

## 7. Current / Target / Historical / Pending

| 口径 | 结论 |
|---|---|
| Current State | RGBWSVT 候选功能完整，缺统一 strict RIP CLI、取消和 Release 资源矩阵。 |
| Target State | G1..G8 均有仓库内可复现证据，T-08 完成后仅剩用户 G9 回签。 |
| Historical State | T-05..T-07 的定向 PASS 不等于完整生产矩阵。 |
| Pending Confirmation | 外部 RIP 适配按用户输入视为完成；设备和实物打印不属于本地可验证证据。 |

## 8. Matrix And Gates

| 行 | 预期 |
|---|---|
| 03 RGB / W / V | 七通道包、T 非空、T 与 RGBWSV 排他、严格 Reader PASS。 |
| 08 / 09 | 缩裹子网格拓扑不闭合，稳定 `TopologyInvalid`，不产生 Package。 |
| 无 T | T 全 255，前六通道与旧协议逐像素一致。 |
| 坏 T | 通道顺序、统计或 TIFF 字节被篡改时严格 Reader fail closed。 |
| 取消 | Package 已预留后取消，异常返回且目录完全移除。 |
| 确定性 | 同配置两次生成的全部生产 TIFF 字节一致。 |
| 资源 | Release 同机运行记录 elapsed/working-set；数值必须可用且无失败/半包。 |

资源结果是本机版本化回归证据，不是设备 SLA。为阻止明显退化，T-08 使用宽松相对门：RGBWSVT
`peakWorkingSetBytes <= max(legacy * 1.25, legacy + 64 MiB)`，总耗时
`<= max(legacy * 2.0, legacy + 5000 ms)`。每条路径运行三次，峰值取最大、耗时取中位数。

## 9. Risks

- 进程峰值工作集包含模型导入和运行库，不能解释为 T 缓冲本身；只作同机相对门。
- 文件系统缓存会影响耗时，因此采用宽松门和中位数，不宣称绝对性能。
- `rip_reader_test` 双协议路由必须保持旧错误码和旧包行为不变。
- T-09 会把显式 RGBWSVT 从候选升级为生产 opt-in；T-08 先验证功能，不提前改写准入状态。

## 10. Files

- `apps/rip_reader_test/main.cpp`
- `tests/matvol_t/LegacyRgbwsvtPackageTests.cpp`
- `tests/matvol_t/MatvolTProductionMatrixTests.cpp`（如矩阵需要独立目标）
- `scripts/run_matvol_t_t08_gate.ps1`
- `CMakeLists.txt`
- MATVOL-T 任务卡与当前状态报告

## 11. Verification

```powershell
cmake --build build-slicesoft-nmake --config Release --target `
  rip_reader_test matvol_rgbwsvt_legacy_package_tests matvol_t_production_matrix_tests
ctest --test-dir build-slicesoft-nmake -C Release -R "^matvol_.*(transfer|rgbwsvt|production_matrix).*" --output-on-failure
.\scripts\run_matvol_t_t08_gate.ps1 -BuildDir build-slicesoft-nmake -Config Release -SkipBuild
ctest --test-dir build-slicesoft-nmake -C Release --output-on-failure
git diff --check
```

完整 CTest 只在构建退出码为 0 后计入结论。

## 12. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.1 | T-08 完成：仓库内 strict RIP 双协议入口、03/08/09/无 T/坏 T/取消/确定性矩阵及 Release 相对资源 Gate 全部通过；完整回归受构建环境限制，未计入 PASS。 |
| 2026-08-26 | v1.0 | 冻结 T-08 真实资产、双协议 strict RIP、取消清理、确定性及相对资源 Gate。 |
