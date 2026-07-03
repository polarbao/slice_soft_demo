# DOC_DECISION_11B_UI配置生产预览与OpenVDB姿态收口

> 文档版本：v0.1  
> 文档状态：DOC_DECISION / Stage 11B  
> 生成日期：2026-07-04  
> 决策主题：是否新增 Stage 11B 处理 UI 配置收敛、生产预览解释和 OpenVDB 姿态一致性

---

## 1. Context

Stage 11 已完成 UI 层预览、常用配置面板和多模型能力边界。Stage 11A-R1 已完成 OpenVDB candidate 写包闭环，但当前仍存在以下用户可见问题：

```text
OpenVDB candidate UI 一键配置与 legacy 一键配置的姿态策略不一致；
UI texture_rgb preview 与 TIFF RGB 生产数据语义不同，容易被误读；
大量 JSON 配置仍暴露给用户；
OpenVDB 是否可替代 legacy 缺少明确 gate 和同姿态 benchmark 文档。
```

---

## 2. Decision

新增 Stage 11B：

```text
UI 配置收敛、生产预览解释与 OpenVDB 姿态一致性收口
```

Stage 11B 是小收口阶段，不替代 Stage 12，不改变 OpenVDB 默认关闭原则，不改变 RGBWSV 协议。

---

## 3. Scope

Stage 11B 覆盖：

```text
OpenVDB candidate UI 配置姿态修复；
生产 RGB 预览和六通道像素探针；
texture.nonSurfaceRgbPolicy 设计；
UI 切片设置界面规划；
配置/Profile/fixture 分层；
OpenVDB 替代 legacy 的生产 gate 和 benchmark 计划。
```

---

## 4. Non-goals

```text
不默认启用 OpenVDB；
不删除 legacy path；
不修改 p0.rgbwsv.2；
不修改 RGBWSV 通道顺序；
不实现设备通信/RIP 半色调/喷头 bitstream；
不承诺 OpenVDB 已可替代 legacy；
不把 non-production OpenVDB 输出当生产输出。
```

---

## 5. Alternatives Considered

### 5.1 直接进入其他项目开发

暂不采纳。

原因：

```text
OpenVDB candidate 姿态配置不一致是当前 UI 可见问题；
生产 RGB/texture RGB 预览误解仍会反复出现；
配置过多影响用户继续使用。
```

### 5.2 直接让 OpenVDB 替代 legacy

拒绝。

原因：

```text
真实模型 strict_closed 仍可能失败；
OpenVDB candidate 支撑策略尚未与 legacy 等价；
同姿态探索性 benchmark 未显示性能显著提升；
OpenVDB OFF 默认轨道仍是项目安全红线。
```

### 5.3 新增 11B 小收口

采纳。

优点：

```text
可以快速修正 UI 配置错误；
可以减少通道解释误判；
可以把配置复杂度逐步转入 UI 设置；
可以为后续 OpenVDB replacement gate 提供清晰边界。
```

---

## 6. Exit Criteria

Stage 11B 完成时应满足：

```text
OpenVDB candidate UI 生成配置与 legacy 使用同样 modelTransform / autoOrient；
同一模型 inspect 显示 legacy 与 OpenVDB candidate 选择同样趴放姿态；
UI 支持生产 RGB 预览或已有明确实现任务；
UI 支持六通道像素探针或已有明确实现任务；
nonSurfaceRgbPolicy 有 PRD/DEV/配置校验设计；
配置/Profile/fixture 分层方案落地到文档；
OpenVDB 替代 legacy 的 gate、benchmark 指标和阶段路线明确。
```

---

## 7. Follow-up

Stage 11B 后可选择：

```text
若只需要稳定 UI 和 legacy 使用体验：进入其他项目开发；
若继续 OpenVDB：进入 OpenVDB production replacement gate 阶段；
若继续 UI 产品化：进入切片设置向导和 Profile 管理阶段。
```
