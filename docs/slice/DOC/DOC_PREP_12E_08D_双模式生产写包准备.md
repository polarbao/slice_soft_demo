# DOC_PREP_12E-08D 双模式生产写包准备

> 文档状态：PREPARED / EXECUTION BLOCKED BY 12E-08C-R3
> 日期：2026-07-20
> 目标：Legacy/Global 双模式路由与统一 RGBWSV TIFF 输出

## 1. 准备结论

12E-08D 的产品模式、配置合同、pipeline 路由、共享 writer、错误状态和原子任务已明确。当前只能完成准备，
不能开始生产接入，因为真实模型 repair/post-strict 和 Release budget 尚未通过。

## 2. 当前代码边界

```text
legacy slicer_cli 已生成正式 RGBWSV TIFF；
slicingMode 已用于 closed_mesh_scanline/relief_heightfield；
texture.applyMode 已包含 global_surface_shell 配置契约；
12E global 目前只生成 diagnostic data/report；
现有 UI engine role 是 LegacyProduction/OpenVdbUtilityCandidate，不等于双生产模式；
生产 writer 不应复制。
```

## 3. 目标架构

```text
SlicePipelineModeRouter
  ├─ legacy -> existing legacy production pipeline
  └─ global_surface_shell
       -> strict/repair admission
       -> partition/texture/raster/full closure
       -> GlobalSurfaceShellProductionComposerAdapter

两路最终统一进入：
  Existing RGBWSV TIFF Writer
  Existing p0.rgbwsv.2 Manifest Writer
  Existing Preview/Report Package Layout
  Existing RIP Strict Validation
```

## 4. 原子任务

```text
12E-08D-01：SlicePipelineMode Config/DTO/Router 与 admission fail-closed；
12E-08D-02：Global in-memory composer 到现有 RGBWSV layer DTO adapter；
12E-08D-03：共享 writer、TIFF/package/RIP/golden 和 no-fallback 验证；
12E-08D-04：显式 Global production Profile、Release matrix 与 GO/NO-GO 报告。
```

12E-09B 在 08D-04 GO 后实现 UI 的普通用户模式选择；09A 只显示 diagnostic 状态。

## 5. 08D-01 Gate

```text
12E-08C-R3-04 GO；
required real model strict PASS；
attribute preservation PASS；
Release budget frozen；
legacy regression PASS；
用户再次确认 production path。
```

当前 Gate：BLOCKED。

## 6. Writer 规则

```text
不新增第二个 TIFF writer；
不改变 writer 的 channel/bit-depth/polarity；
Global adapter 只负责构造现有 writer 所需 RGBWSV layer data；
production success 必须有 TIFF；
preview/report 保持附加输出；
writer 失败时整个 package 失败。
```

## 7. UI 准备

最终 UI 使用“切片模式”下拉框，而不是 OpenVDB 开关替代：

```text
传统切片（默认）
全局纹理壳层
```

Global blocked/diagnostic 时生产按钮不可用；UI 显示 blocker 和“运行诊断”。不得点击 Global 后实际执行 legacy。

## 8. 回归矩阵

| Case | 期望 |
|---|---|
| old config omitted | legacy TIFF hash/统计保持 |
| explicit legacy | 与 omitted 一致 |
| global diagnostic | report/preview 可有，production TIFF 不写 |
| global admitted | TIFF/package/RIP 全部 PASS |
| global blocker | stable error，无 legacy fallback |
| either production success | TIFF layer list 完整 |

## 9. 回滚

默认值保持 legacy。Global Profile 可整体移除或禁用而不影响 legacy writer、旧配置和 UI 默认流程。
