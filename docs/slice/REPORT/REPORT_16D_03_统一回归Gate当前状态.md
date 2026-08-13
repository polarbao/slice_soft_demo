# REPORT_16D-03 统一回归 Gate 当前状态

> 状态：**COMPLETE / PASS**
> 日期：2026-08-13
> 默认决策：**Legacy + S0 + P0 保持默认；不授权 S3/P3 默认切换**

## 1. 范围与代码身份

16D-03 对 Stage 16 已完成的采样、姿态、性能和宿主展示执行统一回归，不新增材料语义，不修改
`p0.rgbwsv.2`、`uint8`、`black_is_print`、`R G B W S V` 通道顺序，也不执行 16B-04 或 16D-05。

本次统一 Gate 使用 `build-slicesoft/main` 的 Debug/Release 显式构建轨道。历史 Quick CI 固定引用
`build` 的基础设施债已修复：脚本现在允许显式选择构建目录和配置，同时保留原来的默认入口。

## 2. Gate 结果

| 验证轨道 | 结果 | 证据与说明 |
|---|---|---|
| Stage 16 与 Stage 14 定向 CTest | Debug 24/24、Release 24/24 PASS | 覆盖 sampling/posture、SPI、Worker、cancel、HostFlow、Stage 15 白区策略 |
| Legacy 零漂移 / Quick CI | PASS | Debug Quick CI 完整通过；source guard、quick regression、schema、support、golden、UI self-test、真实 Overlay 全绿 |
| Full regression | PASS | Release `run_regression.ps1 -Mode full -SkipBuild`，包含坏 3MF/坏 Package 负例 |
| Stage 15 材料闭合 | G1/G2/G3/G4/G5 PASS | Release 自动 Gate；Golden SHA-256 与 Quick CI 零漂移均通过 |
| 13G 支撑连续与铺底 | Debug/Release PASS | 支撑最大投影铺底、30 个物理前置层和 RIP strict 通过 |
| Package / RIP strict | PASS | 回归、13G、Stage 15 与 16C-02 证据均保持严格读取通过 |
| Reality 5/5 | PASS（复用版本化证据） | 16A-05 sampling matrix 与 16B-03 posture matrix 均覆盖 Reality 101..105 |
| 13B / 16C 1/11/12/22 | 12/12 RIP strict PASS（复用版本化证据） | S0/S3/S4 x 1/11/12/22 输出确定性零漂移；历史 JSON 记录 `worktreeDirty=true`，仅作为工程基线 |
| Runtime | Debug/Release PASS | `PrepareSliceSoftRuntime.ps1 -DeployOnly` 发布；两套 manifest 均记录 SPI self-test PASS、31 个场景和 LibTIFF |

Release Runtime 在替换既有目录时检测到文件占用，脚本按既有保护逻辑保留 `output` 并就地更新不可变
payload；最终 manifest 和 SPI self-test 均成功。这是部署占用告警，不是协议或构建失败。

## 3. 回归中修复的问题

1. `Stage14D04ACancellationTests` 已适配当前 `SliceRequest` runner 合同，并在租约测试前创建临时根目录；
2. `run_regression.ps1` 不再把 PowerShell 脚本前遗留的 native `$LASTEXITCODE` 误判为 3MF 生成失败；
3. Quick CI 及 schema/support/golden 子脚本支持显式 `BuildDir` 和 `Config`，统一使用当前主构建轨道；
4. Stage 15 G3 把相同构建参数透传给 Quick CI，避免再次落回历史 `build`；
5. 外部 `rip_project` runtime/SDK drop 被明确排除在 SliceSoft 源码与 source-size Gate 之外。

## 4. 性能与生产结论

16C-02 的 127 dpi / 0.2 mm 功能场景证明 S0/S3/S4 的 1/11/12/22 工程矩阵可重复，
但 S3/S4 没有在所有场景中显著快于 S0。该结果不是正式设备 SLA：buildVolume、原点、轴向、
22 实例目标时限和峰值内存预算仍为 `INPUT_OPEN`。

因此 16D-03 的通过只解锁 16D-04 阶段报告：

```text
Current candidate: S3（仅 relief_heightfield 显式 opt-in）
Diagnostic posture: P3（只读诊断，未授权应用）
Production defaults: Legacy + S0 + P0
Default switch: NOT AUTHORIZED
```

## 5. 外部边界

未执行打印机、外部目标 RIP、实物工艺和正式设备 SLA 验收；没有把本地参考 TIFF 提升为生产
Golden。16B-04 姿态接入和 16D-05 默认切换继续要求用户单独明确授权。
