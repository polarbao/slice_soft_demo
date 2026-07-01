# DEMO_00_08_Demo阶段验证与回归基线

> 文档版本：v0.1
> 文档状态：Formal DEMO Supplement / 00-08 Baseline
> 生成日期：2026-07-01
> 证据等级：B，来源为 archive 历史 DEMO / REPORT 汇总；验证状态需以当前命令重新运行为准

---

## 1. 目的

本文件把 00-08 阶段历史验证方案汇总为当前回归基线。

它不声明这些验证在当前会话已经通过，只说明后续维护时应该覆盖哪些行为。

---

## 2. 验证矩阵

| 阶段 | 验证主题 | 后续回归关注点 |
|---|---|---|
| 00 | 单材料切片输出 | package 是否生成、通道是否存在 |
| 00B | 8bit / black_is_print | bitDepth 与 polarity 不回归 |
| 00C / 01 | relief heightfield | 浮雕高度映射和层输出稳定 |
| 02 | 支撑 / 孤岛 | support mask、island diagnostics、SupportType metadata |
| 03 | RGBWSV 协议 | schema、channelOrder、negative tests |
| 03B | TIFF storage mode | stripped / tiled 读写兼容 |
| 03C | 回归和 reader 摘要 | summary 字段、脚本分层 |
| 04 | 彩色纹理 | OBJ/MTL/PNG 纹理采样 |
| 04A | fallback / support diagnostics | 缺失纹理、fallback、支撑诊断 |
| 05 | 材料策略 | white / varnish / support / model 优先级 |
| 05A | 工艺参数 | MaterialProcessProfile report |
| 06 | 3MF 多材料 | OBJ/MTL 与 3MF material mapping |
| 06A | 3MF negative | bad package、deflate、XML parser |
| 06B | 3MF Texture2D / ColorGroup | 纹理组、颜色组、资源解析 |
| 07 | Qt Debug UI | UI 启动、基础预览、report 显示 |
| 07A | 参数编辑 / profile | config editor、profile visualization |
| 07B | UI smoke | self-test、overlay smoke、配置编辑器收口 |
| R2 | 工程化回归 | schema/golden/CI 分层 |
| 08 | 支撑形态 | support shape/process report |
| 08A | bridge fixture | 支撑桥接 fixture、真实模型 profile |

---

## 3. 推荐验证入口

通用：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
```

历史回归：

```powershell
.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
```

UI：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

---

## 4. 回归原则

```text
1. 历史 DEMO 文档只说明应验证什么，不代表当前已运行。
2. 任何协议、输出、纹理、材料、支撑相关改动都应检查 00-08 回归基线。
3. 09P、10、11 新增能力不应破坏 00-08 已形成的协议和输入输出能力。
4. 若历史验证脚本已改名或失效，必须在 REPORT 中说明并补新入口。
```

