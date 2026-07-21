# DOC_ANALYSIS_12E R3-04 后续可达性与模型治理

> 文档状态：CURRENT ANALYSIS
> 日期：2026-07-21

## 1. R3-04 后能否继续实现 12E 目标

可以，但必须区分“功能开发可继续”和“生产准入可继续”：

| 目标 | 当前可否继续 | 条件 |
|---|---|---|
| Texture Surface width 从最小值到 allTexture | 可以 | 使用 strict PASS 的闭合 OBJ/3MF |
| UI 设置推荐/有效最小宽度 | 可以 | 先接 R4 preflight/dynamic metrics facade |
| Model Fill 白墨/光油/RGB/material role | 可以 | diagnostic composer/effective config；生产 TIFF 等待 08D |
| required 真实 OBJ global production | 不可以 | 三例先修复并通过属性/post-strict/full chain |
| 12E-08D production adapter | 不可以 | R4-08 GO + 用户授权 |

因此，R3-04 的 NO-GO 是资产准入结论，不是 Texture/Fill 分区方案失败。

## 2. 模型处理路线判断

推荐同时保留两类模型：

```text
Clean positive fixtures：快速开发算法、UI、材料和 allTexture；
Required real models：验证真实资产鲁棒性和生产 Gate。
```

只换成正常模型会让正向功能继续，却无法证明真实模型问题已解决。立即开发通用复杂自相交自动重建则
风险过大，可能破坏 UV、材质、尺寸和薄壁。当前最合理路线是：正常模型推进功能，三个 required 模型走
外部修复/独立重建后 intake 审计；如果外部流程无法规模化，再立 R5 重建预研。

## 3. 导入检测是否应成为产品功能

应该。检测必须是切片调用链的正式 Gate，而不是独立“诊断按钮”：

```text
导入 -> 快速检查 -> 最终姿态/变换 -> 完整预检 -> 当前模式准入 -> 切片
```

UI/CLI 都必须调用同一个 core facade。检测为 blocked、missing 或 stale 时停止当前动作并显示稳定错误；
global 的拓扑错误不得回退 legacy。legacy 可保留兼容警告，因此相同诊断事实需要模式相关严重级别。

## 4. 为什么插在 08D 前

08D 会把 global 结果接到生产 TIFF writer。若没有统一 preflight，用户可以通过不同按钮绕过 strict Gate，
也无法区分“模型错误”“backend 不可用”和“生产未准入”。因此 R4 是 08D 的必要前置，而不是可延后的 UI
装饰。

## 5. 当前建议

从 R4-01 开始。R4-01..05 可立即开发；R4-06..08 在外部修复模型到位前保持 blocked。不要启动 08D，
也不要把正常模型的 PASS 写成 required real-model PASS。

