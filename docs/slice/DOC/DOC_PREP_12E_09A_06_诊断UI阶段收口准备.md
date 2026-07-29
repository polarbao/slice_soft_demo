# DOC_PREP 12E-09A-06 诊断 UI 阶段收口准备

> 文档状态：EXECUTED / COMPLETE
> 版本：v1.0
> 日期：2026-07-29

## 1. 目标

对 09A-01..05 的配置、中文参数、异步分析和同层语义预览进行统一回归，形成可审计的阶段报告、
用户说明和上下文索引。09A-06 不新增诊断算法。

## 2. 收口范围

```text
single_model 与 scene/current-instance 身份；
Diagnostic Effective Config 原子保存和 stale；
中文 width/modelFill 控件；
Worker 成功、失败、取消、重入和关闭安全；
TIFF 真源上的同层 Texture/Fill/Partition；
默认 OpenVDB OFF；
生产 Profile、package、RIP 和协议不变；
1280x720、1440x900、1920x1080 及 150% 字体尺度。
```

## 3. 输出

```text
docs/slice/REPORT/REPORT_12E_09A_诊断UI阶段收口.md
docs/user_guides/SLICE_12E_09A_纹理填充诊断使用说明.md
docs/slice/README.md
docs/codex_task/current/TASKS_12E_09A_诊断UI任务清单.md
docs/codex_task/current/TASKS_12_13_后续开发计划总览清单.md
docs/slice/REPORT/REPORT_12X_阶段计划与完成度总览.md
AGENTS.md
```

## 4. 验证

```text
09A core/config/worker/preview 定向 CTest；
Debug 全量 build；
Debug 全量 CTest；
Qt self-test；
diagnostic settings、same-layer preview、三窗口和取消/失败 UI smoke；
Quick CI；
默认 OpenVDB OFF 回归；
git diff --check；
文档链接和状态一致性检查。
```

## 5. Gate

只有以下条件全部满足才能写 `09A COMPLETE`：

```text
09A-05 报告为 COMPLETE；
没有跨层、跨实例或 stale 复用；
blocked/not_evaluated 没有显示成 PASS；
生产 TIFF/RIP 回归保持；
状态报告列出实际命令和结果；
12E-10A 前置被明确解除。
```

失败时记录真实阻断，09A 状态保持 PARTIAL，不得通过文档改写结果。

## 6. 执行结果

2026-07-29 已完成统一回归、三窗口与 150% 字体尺度 Smoke、全量 Debug Build、84/84 CTest、
Quick CI、用户说明和阶段报告。详细证据见：

```text
docs/slice/REPORT/REPORT_12E_09A_诊断UI阶段收口.md
docs/user_guides/SLICE_12E_09A_纹理填充诊断使用说明.md
```
