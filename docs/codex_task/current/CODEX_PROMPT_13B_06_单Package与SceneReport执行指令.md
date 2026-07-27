# CODEX_PROMPT 13B-06 单 Package 与 Scene Report 执行指令

> 状态：READY FOR FIXTURE DEVELOPMENT
> 日期：2026-07-27
> 前置：13B-05 FIXTURE COMPLETE

## 1. 必读

```text
AGENTS.md；
docs/slice/REPORT/REPORT_13B_05_全局Raster与联合层合成当前状态.md；
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md；
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md；
docs/slice/DEMO/DEMO_13B_多模型排版联合切片验证方案.md；
docs/slice/DOC/DOC_PREP_13B_06_单Package与SceneReport准备.md。
```

## 2. 执行顺序

```text
13B-06A：先写 scene report DTO/schema 与负向测试；
13B-06B：为共享 RGBWSV writer 增加 typed scene extension；
13B-06C：实现 MultiModelScenePackageWriter，仅消费 13B-05 完整联合层；
13B-06D：验证单 package、单层单 TIFF、scene report、原子失败与 RIP strict；
运行定向回归和 Quick CI；
生成 REPORT_13B_06_单Package与SceneReport当前状态.md。
```

## 3. 固定边界

```text
一个 scene 只发布一个 package；
每层只写一个全局 TIFF；
不重新切片、不反读 TIFF、不重做材料合成；
scene report 必须与 TIFF/manifest 同一 staging 事务发布；
manifest 只增加可选 scene 和 reports.scene；
保持 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
保持 Legacy 默认，Global/OpenVDB 显式 opt-in；
不允许混合引擎、逐实例 package 或部分场景成功；
不做 13B-07 真实模型矩阵和 13C TIFF 原生预览。
```

## 4. 验收

```text
SceneLayerComposeResult 成功输出一个 package；
manifest 固定协议字段完全不变；
scene report 可按 sceneId/instanceId 审计并与合成统计对账；
RIP strict 接受 scene 可选字段；
旧单模型 writer 输出保持兼容；
blocked、identity mismatch、schema/path/mode/grid/layer 错误全部 fail-closed；
失败不留下 staging 或伪成功 package；
相同输入结果确定；
Quick CI 回归通过。
```

## 5. 停止条件

```text
需要改变 TIFF/manifest 固定协议；
需要放宽 RIP 必填校验；
需要复制 package 事务；
需要在发布后补写 scene report；
需要虚构设备参数；
13B-05 或单模型 writer 回归失败。
```
