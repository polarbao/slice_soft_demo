# PRD 12E-09B Qt 双模式生产入口与能力锁定

> 状态：READY FOR IMPLEMENTATION
> 日期：2026-07-23
> 前置：12E-08D-01..06 COMPLETE

## 1. 目标

在现有 Qt 调试工作台中提供可审计的 Legacy/Global 双模式生产入口。普通用户可以明确选择模式，
查看准入状态和能力限制，并通过现有一键切片流程生成统一 RGBWSV package。

## 2. 用户故事

```text
作为普通用户，我能看到“传统切片”和“全局纹理壳层”两个产品模式；
作为普通用户，我不需要理解 OpenVDB backend；
作为操作人员，我能在运行前看到所选模式是否可用、为何被阻断以及实际生效模式；
作为质量人员，我能从 Effective Config、manifest 和报告追踪 requested/effective mode；
作为维护人员，我能确认 Global 失败不会被静默改成 Legacy 成功。
```

## 3. 功能需求

### 3.1 模式选择

```text
默认：传统切片；
可选：全局纹理壳层；
配置值：legacy / global_surface_shell；
切换模式后必须重新计算 admission；
模型、姿态、缩放、MTL、贴图或关键配置变化后旧 admission 必须 stale。
```

### 3.2 Global Profile

首版只开放已准入的显式候选：

```text
global_surface_shell_restricted_candidate：
  RGB + W；
  不开放 S/V 生产能力。

global_surface_shell_material_parity_candidate：
  RGB + W；
  lower/internal-void S；
  surface/outer V。
```

不支持的 support placement、形态、offset/dilation/bridge 等控件必须禁用并给出原因，不能静默忽略。

### 3.3 状态与提示

UI 至少显示：

```text
请求模式；
实际模式；
生产准入状态；
所选 Global Profile；
当前模型阻断原因；
是否生成生产 TIFF；
是否发生 fallback，固定为 false；
Global 资源开销提示；
本次实际切片/合成/TIFF/preview/report/总耗时和可用的峰值内存。
```

### 3.4 一键切片

现有“导入模型并切片”流程必须使用当前产品模式：

```text
选择模型；
生成 session-scoped Effective Config；
执行模型预检和所选模式 admission；
通过后启动所选 production pipeline；
加载同一 package 结构的 TIFF、preview 和 report；
失败时保留日志和稳定错误，不生成伪成功 package。
```

高级 OpenVDB 诊断/候选按钮保持独立，不代替产品模式选择。

## 4. 验收标准

```text
Legacy 为默认且历史一键切片行为不回归；
Global 必须显式选择；
restricted/material-parity 能力锁定正确；
Global topology blocked 时不启动 writer；
requestedPipelineMode == effectivePipelineMode；
fallbackApplied == false；
两种成功模式均生成 p0.rgbwsv.2 RGBWSV uint8 TIFF；
两种成功模式均通过 RIP strict；
UI 中文文本在 1280x720、1440x900、1920x1080 不遮挡；
模式切换、模型切换、失败后重试不会复用 stale admission/result。
```

## 5. 非目标

```text
不新增第三种产品模式；
不把 OpenVDB 设为默认或强制依赖；
不优化 Global 算法性能；
不新增 TIFF 通道或改变协议；
不自动修复复杂自相交模型；
不在 09B 完成 09A 的同层诊断 preview；
不把候选参考机器倍数宣传为产品 SLA。
```

## 6. 风险

```text
模式/Profile/后端概念混淆；
UI 保存的配置与 CLI 实际配置不一致；
禁用控件值仍残留在 Effective Config；
Global 失败后旧 package 被误当新结果；
异步进程结束后加载了错误 session；
高内存模型造成系统压力。
```

所有风险必须通过 fail-closed、session identity、capability lock 和回归矩阵控制。
