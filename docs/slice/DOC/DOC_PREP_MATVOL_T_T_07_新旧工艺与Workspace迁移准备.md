# DOC_PREP_MATVOL-T T-07 新旧工艺与 Workspace 迁移准备

> 文档状态：**IMPLEMENTED / T-07 COMPLETE**  
> 版本：v1.2 | 日期：2026-08-26  
> 任务真源：`../../codex_task/current/TASKS_MATVOL_T_RGBWSVT缩裹材料通道任务清单.md`

## 1. 目标与边界

在不修改旧工艺文件、旧默认 Profile 和旧 `profileHash` 的前提下，为生产/示例工艺建立
显式 `_rgbwsvt` 副本，并使参考 Host 能配置、生成、保存和恢复完整的 T 通道策略。

本卡不修复 08/09，不修改 SPI v1 C ABI，不删除旧工艺，不把 RGBWSVT 设为默认，也不宣称
完整 RIP、性能、设备或实物打印通过。

## 2. 当前代码事实

```text
samples/configs/material_process 当前无 diff，是旧工艺字节兼容基线；
samples/configs/matvol_t/process_profiles 已有 RGB/W/V 三个原型，但不是完整工艺迁移；
HostSliceJobController 已能按 output.packageProtocol 选择新旧能力；
HostSliceSettings/ProcessPreset/Profile Builder 尚无协议、七通道和 transfer policy 字段；
Profile catalog 仍只声明 slice.rgbwsv；
workspace schema 当前为 v7，只接受精确版本，尚未保存 T 配置；
旧 Profile 的规范 JSON 未显式写 packageProtocol，机械补写会改变旧 profileHash。
```

## 3. 冻结方案

### 3.1 工艺双轨

旧 `samples/configs/material_process` 逐字节保留；新版只在 `samples/configs/matvol_t` 下以
独立 `_rgbwsvt` 文件名和工艺 ID 新增。新版工艺复用对应旧工艺语义，仅增加：

```text
output.packageProtocol = p0.rgbwsvt.1
output.channelOrder = R,G,B,W,S,V,T
transferChannelPolicy = 来自外部工艺配置的 RGB/Kd、缺失/多匹配/拓扑策略
```

不得在 C++ preset/profile builder 中固化 `[255,220,198]` 或材质名 `02`。

### 3.2 Host Profile 双分支

旧 preset 继续走原序列化分支，不新增显式 `packageProtocol`，以维持旧规范 JSON/hash。
新 preset 显式写入 `p0.rgbwsvt.1`、七通道顺序和完整 `transferChannelPolicy`，这些字段全部参与
profileHash。Profile catalog 对新 Profile 声明 `slice.rgbwsvt`，无能力时禁用，不回退旧能力。

### 3.3 Workspace v8

schema v8 持久化新版 preset ID、package protocol、T RGB 列表及 missing/multiple/topology 策略。
v7→v8 迁移补成旧协议 + transfer disabled，保留旧 preset ID；未知版本仍 fail closed。
workspace 不持久化 profileHash，恢复 settings 后重新构建；协议/启用状态错配、启用但 RGB 列表为空
或策略未知均拒绝。

## 4. 原子拆分

| 子卡 | 内容 | 完成定义 |
|---|---|---|
| T-07A | 冻结旧工艺 SHA256、旧 Profile JSON/hash 与默认选择 | 后续验证可证明零漂移 |
| T-07B | 建立生产/示例 `_rgbwsvt` 工艺副本 | 旧目录逐字节不变；新版配置可解析 |
| T-07C | 扩展 Host settings、ProcessPreset 与 Profile catalog | 新 preset 显式 opt-in；旧 preset 不变 |
| T-07D | Effective Profile 写入协议、七通道和 T policy | 新 hash 闭合；旧 hash 零漂移 |
| T-07E | workspace v8 与 v7→v8 迁移 | 新字段往返；坏状态 fail closed |
| T-07F | 新旧能力路由、兼容回归与文档收口 | 定向矩阵、门禁与状态文档通过 |

## 5. 风险与停止条件

```text
旧工艺任一 SHA256 漂移；旧 Profile 规范 JSON/hash 漂移；
Host 代码固化缩裹 RGB/Kd 或材质名；
新 preset 缺少 packageProtocol/T policy 仍可提交；
旧 Worker 缺新能力时回退 slice.rgbwsv；
v7 workspace 被删除而非迁移，或坏 v8 被静默降级；
08/09 被修改、修复或计为生产正例。
```

## 6. 验证计划

```text
配置：旧工艺目录 SHA256 前后相同；所有新版 JSON 解析并满足双协议约束
Profile：旧默认 JSON/hash 固定；新 Profile 的协议、七通道和 T policy 进入 hash
Workspace：v8 round-trip、v7→v8、未知版本/坏 T policy fail closed
Host：旧/新 preset 分别选择 slice.rgbwsv/slice.rgbwsvt；缺能力禁止提交
构建：隔离 NMake Release，MSVC /W4 /WX
门禁：定向 CTest、SourceSizeGuard 自测+全扫描、git diff --check
```

## 7. 准备结论

T-07 前置满足，可按 T-07A→F 连续实施。旧工艺与旧 hash 是第一优先级兼容 Gate；任何漂移均
先停止实施并回到 Profile 序列化边界定位。

## 8. 实施结果

T-07A→F 已连续完成：15 份旧工艺文件保持零 diff 与 SHA256 零漂移；新增 10 份独立
`_rgbwsvt` 工艺；Host 从外部 JSON 加载 T 策略并提供显式新 Profile；新 Profile 的协议、
七通道和 T 策略进入规范 hash，旧 Profile 不进入扩展分支；workspace v8 往返、v7 迁移及坏状态
fail-closed 均有自动化覆盖。08/09 未修改。

收口审查进一步固化 15 份旧工艺 SHA256 与归一化旧 Profile hash，要求所有新工艺只使用安全相对
输出目录；外部工艺缺失时禁用新 Profile；v7 若伪带 T-only Profile/preset 标识则拒绝迁移。

隔离 NMake Release 的相关目标构建通过；定向 CTest `32/32 PASS`；SourceSizeGuard 自测和全扫描
PASS（41 项既有 warning），Qt Host Boundary PASS；旧工艺目录零 diff 与 Host 硬编码审计 PASS。
未执行完整 225 项回归及 T-08 矩阵。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.2 | 根据多 Agent 收口审查补齐固定兼容基线、相对输出路径、缺配置禁用与 v7 T-only 标识负例。 |
| 2026-08-26 | v1.1 | 完成 T-07A→F：工艺双轨、Host 新 Profile、规范 hash 与 workspace v8/v7 迁移实现及定向验证。 |
| 2026-08-25 | v1.0 | 完成 T-07 准备：冻结工艺双轨、旧 hash 零漂移、Host 新 Profile 分支与 workspace v8/v7 迁移方案。 |
