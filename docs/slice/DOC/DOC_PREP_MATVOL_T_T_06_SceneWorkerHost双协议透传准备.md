# DOC_PREP_MATVOL-T T-06 Scene/Worker/Host 双协议透传准备

> 文档状态：**IMPLEMENTED / T-06 COMPLETE**  
> 版本：v1.1 | 日期：2026-08-25  
> 任务真源：`../../codex_task/current/TASKS_MATVOL_T_RGBWSVT缩裹材料通道任务清单.md`

## 1. 目标与边界

把 T-05 已闭环的单模型 Legacy `p0.rgbwsvt.1` 候选能力，通过受控的新能力合同接入
Scene/Worker/参考 Host，并让 Package Query 能按 manifest.schema 查询和预览六/七通道包。

本卡不修改 `slice.rgbwsv`、`p0.rgbwsv.2`、SPI v1 C ABI、默认 Profile、旧工艺文件或外部 RIP；
不修复 08/09，也不在本卡完成工艺目录迁移和 workspace/profileHash 升级。

## 2. 当前代码事实（A 类证据）

```text
Worker/Host 当前只声明 slice.rgbwsv，output.contract 固定 p0.rgbwsv.2；
file_contract 为 major=1/minor=0，Worker discovery 只声明 produces p0.rgbwsv.2；
ProductionSliceFacadeFactory 只绑定 MultiModelProductionService 六通道场景入口；
PackageSummary、LayerDescriptor、VerifyResult 使用固定 array<...,6>；
PackageQueryFacade、strict RIP reader 和 TiffLayerSource 只接受六通道；
HostSliceJobController 固定发送 p0.rgbwsv.2，Host Package Review 固定校验六通道；
T-05 的 RGBWSVT Legacy 入口已支持 input/instance 变换前后的同 grid T plan，
但为防止伪 DTO，仍主动阻断 override/callback/Scene 请求。
```

因此 T-06 不是简单改一个 `channelCount`，而是能力协商、场景适配、Package Query DTO 和 Host
共同受控修订；直接把现有 `slice.rgbwsv` 改成双协议会破坏 Stage 14 冻结合同，不采用。

## 3. 受控方案

### 3.1 新能力而非改写旧能力

新增 Worker-only 能力 `slice.rgbwsvt`：

```text
slice.rgbwsv   -> 只允许 p0.rgbwsv.2，原请求/结果/错误保持不变
slice.rgbwsvt  -> 只允许 p0.rgbwsvt.1，显式 opt-in，无旧协议回退
```

ModuleInfo 的 `provides/workerCapabilities/produces` 追加新值，不删除或重命名旧值。

### 3.2 file_contract 小版本协商

保持 `major=1`，Worker discovery 升为 `minor=1` 并声明两个 produces 与两个切片能力。
旧 `slice.rgbwsv` 请求继续使用 `minor=0`；新 `slice.rgbwsvt` 请求必须使用 `minor=1`。
模块仅在 discovery 同时包含 `p0.rgbwsvt.1` 和 `slice.rgbwsvt` 时发起新请求；旧 Worker
因 minor/produce/capability 任一不足而在启动作业前稳定拒绝。

### 3.3 Scene 范围

T-06 先冻结为 **单可见实例 Scene 候选**：Scene 可携带正式 instance transform，Runner 将
模型路径和实例变换作为 `run_slicer` 的 input/instance override，T plan 仍在最终生产姿态后建立。

```text
恰好 1 个可见实例  -> 允许进入 RGBWSVT Legacy Scene Runner
0 个或多个可见实例 -> PM-SLICER-PROFILE-0031，写包前 fail closed
callback 六通道 DTO -> 继续禁用，不扩充或伪装
```

多实例七通道场景合成涉及 T 所有权、实例重叠优先级和 MultiModel writer 泛化，超出 T-06；
不得让新能力静默进入现有六通道 MultiModelProductionService。

### 3.4 Facade 与 Package Query

`SliceRequest` 增加显式 `output_contract`，生产 Factory 按合同分派旧场景 Runner 或新的
singleton RGBWSVT Runner。新 Runner 复用 T-05 session/Writer/report，不复制 T 识别或合成逻辑。

Package Query 保留现有 capability 名称，按 manifest.schema 分派严格 Reader。公共只读 DTO 中固定
六元素数组改为动态通道向量；这是 C++ API 受控修订，但不改变 SPI v1 C 结构。旧包返回六项，
新包返回七项。T Preview 至少支持 `single_channel + [T]`；组合预览只接受 manifest 实际存在的通道。

### 3.5 Host

Host 根据所选工艺的 `output.packageProtocol` 选择 `slice.rgbwsv` 或 `slice.rgbwsvt`，并在模块能力
和 produces 不满足时禁用提交。Package Review/Layer Descriptor/Preview 通道列表来自 Package Query，
不得在 UI 再固定为六通道。T-07 之前只接测试工艺，不改默认工艺目录和 workspace schema。

## 4. 原子拆分

| 子卡 | 内容 | 完成定义 |
|---|---|---|
| T-06A | file_contract minor=1、新 capability、Module/Worker discovery 与路由 | 旧 minor=0 请求不变；新请求缺任一能力时写前拒绝 |
| T-06B | singleton Scene RGBWSVT Runner、Facade/Worker materialization 与取消清理 | 03 单实例成功；多实例/08/09/取消均 fail closed 且无残包 |
| T-06C | Package Query 双协议 Reader、动态 DTO、T layer descriptor/preview/report | 六/七通道 summary/verify/descriptor/preview 均由 manifest 驱动 |
| T-06D | 参考 Host 能力选择、提交和 Package Review/Preview 动态通道 | 新工艺选新能力；旧工艺仍发旧能力；无静默回退 |
| T-06E | 合同/schema/Module/Worker/Host 回归与文档收口 | 定向矩阵全 PASS，任务卡和状态报告同步更新 |

## 5. 预计影响文件

```text
contracts/file_contract_v1.*
contracts/slicer_capability_dtos.{json,md}
src/slicer_core/api/{SliceDtos,PackageDtos}.h
src/slicer_core/engine/ProductionSliceFacadeFactory.*
src/slicer_core/api/implementation/PackageQueryFacade*
src/slicer_core/output/rgbwsvt/*PackageReader*
src/slicer_module/{ModuleInfo,WorkerContract,CapabilityCarrierRouter,WorkerJobService,PackageCapabilityAdapter}.*
apps/slicer_worker/{WorkerApplication,slice/WorkerSliceRequestMaterializer,slice/WorkerSliceExecutor}.*
apps/slicer_ui_host_sim/{ModuleClient,HostSliceJobController,HostPackageReview*,HostSliceSettings*}.*
tests/contracts、tests/module、tests/worker、tests/hostflow、tests/matvol_t
```

优先新增独立 Runner/Reader 模块；G2/G1 冻结或超长文件只做窄接线，禁止把七通道逻辑继续堆入
`slicer.cpp`、`WorkerJobService.cpp` 或 Host 大文件。

## 6. 风险与停止条件

```text
旧 slice.rgbwsv 接受 p0.rgbwsvt.1；
新能力在 Worker discovery 不匹配时仍启动作业；
singleton Scene 丢失 instance transform 或重复自动定向；
多个可见实例被静默裁成一个；
Package Query 仅凭 TIFF SamplesPerPixel 推断协议；
把固定 array<6> 机械改大导致旧包 checksum/JSON 形状漂移；
Host 缺能力时回退旧协议；
取消/异常留下可被误认为完成的 Package；
08/09 被顺手修复或计为生产正例。
```

命中任一项立即停止并回到合同/Runner 边界复核。

## 7. 验证计划

```text
构建：NMake Release，MSVC /W4 /WX；构建和 CTest 退出码分开判定
合同：minor 0/1 正负例、produce/capability 缺失、错误 capability/contract 组合
Worker：03 单实例 RGBWSVT；旧 RGBWSV；多实例拒绝；08/09 拒绝；取消和失败清理
Package：六/七通道 summary/verify/descriptor/report；T 单通道预览；坏七通道包
Host：旧/新工艺能力选择、缺能力禁用、Package Review 动态 6/7 通道
兼容：现有 Stage 14 Worker/Module/Host 定向测试与旧 Package Query 全通过
门禁：SourceSizeGuard 自测+全扫描、git diff --check、旧工艺目录无修改
```

完整 RIP、性能和设备打印仍属于 T-08/T-09，不在 T-06 宣称通过。

## 8. 准备结论

T-06 前置事实已查清，方案可实施；因其包含 file_contract 小版本、C++ Package DTO 和生产
Facade 受控修订，实施时应按 06A -> 06B -> 06C -> 06D -> 06E 顺序，不一次横跨全部边界。
预计需要 2 至 3 次会话；T-06A/06B 代码开工前需提交本 PREP 对应的 Implementation Plan。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-25 | v1.1 | 按准备边界完成 T-06；新增能力与旧能力严格隔离，定向 CTest 14/14、SourceSizeGuard 自测及全扫描 PASS；完整 RIP/性能/设备验证仍归 T-08/T-09。 |
| 2026-08-25 | v1.0 | 完成 T-06 准备：冻结新增 `slice.rgbwsvt`、file_contract minor=1 双版本请求、单可见实例 Scene 候选、Package Query 动态 DTO 与 Host 能力协商方案。 |
