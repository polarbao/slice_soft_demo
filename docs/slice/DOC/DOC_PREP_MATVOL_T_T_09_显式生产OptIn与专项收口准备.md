# DOC_PREP MATVOL-T T-09 显式生产 Opt-In 与专项收口准备

> 状态：COMPLETE  
> 日期：2026-08-26  
> 分支：`codex/matvol-t-channel-protocol`

## 1. Problem Type

T-09 是生产准入状态变更与专项收口任务。用户已明确要求继续依次完成 T-08、T-09，并确认外部 RIP
适配可视为完成，因此该指令构成 G9 显式 opt-in 授权。任务不改变 T 通道几何、颜色识别或 TIFF 字节
协议，只把已经过 T-08 仓库内矩阵的受控 Scene/Worker/Host 新协议路径标记为可显式生产使用。

## 2. Layers

```text
新版 _rgbwsvt 工艺 + Host 显式 Profile
  -> slice.rgbwsvt / file_contract minor=1
  -> singleton Scene Worker/Façade
  -> Legacy RGBWSVT Runner（production opt-in 标志）
  -> manifest productionAcceptance=admitted
  -> strict Package Reader / RIP
```

直接 `slicer_cli` 样例路径继续是 `rgbwsvt_candidate_unvalidated`，避免绕过 Host/Worker 能力协商和
Scene 准入。旧 Profile、旧工艺、`slice.rgbwsv`、minor=0 与 `p0.rgbwsv.2` 默认路径不变。

## 3. Official Docs

- `DOC_DECISION_MATVOL_T_RGBWSVT协议与缩裹材料通道.md`
- `DEV_MATVOL_T_可配置缩裹识别与RGBWSVT双协议设计.md`
- `DOC_PREP_MATVOL_T_T_08_生产矩阵与准入验证准备.md`
- `contracts/p0.rgbwsvt.1.schema.json`
- `contracts/file_contract_v1.md`

## 4. Historical Docs

- `REPORT_MATVOL_T_RGBWSVT缩裹材料通道当前状态.md`
- `TASKS_MATVOL_T_RGBWSVT缩裹材料通道任务清单.md`
- T-05..T-07 的 candidate/restricted 表述

历史候选状态用于定位需升级的表述，不作为生产准入证据；T-08 当前 Release 结果才是本卡的仓库内
实现基线。

## 5. AI Workspace Evidence

多 Agent 只读审查用于核对 Host/Worker Scene 边界、Profile 安全级别、Reader 和测试影响。所有代码、
文档与验证结果仍由当前隔离工作树统一复核，Agent 建议本身不计 PASS。

## 6. Current Code Reality

- RGBWSVT Scene 入口仅允许一个可见实例，Worker capability 与 output contract/minor 严格配对。
- Host 新 Profile 只有部署了外部 `_rgbwsvt` 工艺时可用，且不会静默回退旧协议。
- Runner 当前以 `transfer_scene_candidate` 放行 Scene adapter，并把所有 RGBWSVT 包写成候选状态。
- direct CLI 与 Scene/Worker 共用 Runner，因此必须用调用方显式标志区分准入，不能全局改为 admitted。
- `p0.rgbwsvt.1` schema 和严格 Reader 尚未冻结 `productionAcceptance` 的合法值。

## 7. Current / Target / Historical / Pending

| 口径 | 结论 |
|---|---|
| Current State | T-08 仓库内 Gate PASS，但所有 RGBWSVT manifest 仍标记 candidate。 |
| Target State | 仅显式 Host/Worker Scene 新协议路径写 `admitted`；direct CLI 保持 candidate。 |
| Historical State | T-05..T-08 的“候选/受限”结论在当时正确，收口后保留为历史过程。 |
| Pending Confirmation | 外部 RIP 适配按用户输入接受；设备和实物打印仍未由本仓库验证。 |

## 8. Admission Contract

1. `host-reference-transfer-channel` 改为 `production`，但仍是非默认、显式选择的独立 Profile。
2. 只有 `slice.rgbwsvt`、minor=1、`p0.rgbwsvt.1`、singleton Scene 及外部新版工艺全部满足时，
   Runner 才收到 production opt-in。
3. manifest `productionAcceptance` 只接受 `admitted` 或 `rgbwsvt_candidate_unvalidated`；严格 Reader
   对缺失和未知值 fail closed。
4. direct CLI、测试样例和任何没有上游准入标志的调用继续输出 candidate。
5. 03 是当前生产正例；08/09 保持开放拓扑拒绝，不修复、不回退、不计正例。
6. 默认 Profile、旧工艺和六通道协议不得发生语义或 hash 漂移。

## 9. Risks

- 若在 Runner 中无条件写 admitted，direct CLI 会绕过生产入口，属于严重准入泄漏。
- 若只改 Host 标签不改 manifest，RIP/审计无法判断包的真实准入来源。
- schema 改为要求字段后，旧候选样例也必须继续写合法 candidate 值；不得让 Reader 静默接受缺字段。
- 本卡不能把“外部 RIP 已适配”扩写为设备打印、实物质量或现场 SLA 已通过。

## 10. Files

- `src/slicer_core/slicer.h/.cpp`
- `src/slicer_core/engine/ProductionSliceFacadeFactory.cpp`
- `src/slicer_core/output/rgbwsvt/RgbwsvtPackageReader.h/.cpp`
- `apps/rip_reader_test/main.cpp`
- `apps/slicer_ui_host_sim/HostProfileCatalog.cpp`
- `contracts/p0.rgbwsvt.1.schema.json` 与 `contracts/file_contract_v1.md`
- Host/Worker/Reader/MATVOL-T 测试、专项决策/DEV/报告/任务卡/用户说明

## 11. Verification

```powershell
cmake --build build-slicesoft-nmake/Release --target `
  slicer_cli rip_reader_test matvol_t_production_matrix_tests `
  worker_slice_executor_tests host_profile_panel_tests host_slice_job_tests
ctest --test-dir build-slicesoft-nmake/Release -C Release -R "(matvol_|worker_slice_executor|host_profile_panel|host_slice_job|file_contract|json_schema|stage14b)" --output-on-failure
.\scripts\run_matvol_t_t08_gate.ps1 -BuildDir build-slicesoft-nmake/Release -Config Release -SkipBuild
git diff --exit-code -- samples/configs/material_process
git diff --check
```

T-09 还需专门断言 Scene/Worker 包为 `admitted`、direct CLI 仍为 candidate、未知/缺失准入值被严格
Reader 拒绝。完整回归只有在全目标构建成功后才计入结果。

## 12. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.1 | 按冻结边界完成实施：Scene/Worker admitted、direct CLI candidate、严格 Reader/Worker 二次校验；聚焦 CTest 12/12 与 T-08 Gate 复跑通过。 |
| 2026-08-26 | v1.0 | 冻结显式生产 opt-in 边界：Scene/Worker admitted、direct CLI candidate、默认旧协议不变、08/09 继续 fail closed。 |
