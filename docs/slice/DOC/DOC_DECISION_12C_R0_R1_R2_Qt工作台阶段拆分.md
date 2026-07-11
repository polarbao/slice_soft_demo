# DOC_DECISION_12C R0/R1/R2 Qt 工作台阶段拆分

> 文档状态：Decision
> 日期：2026-07-10

## 1. 决策

12C 拆分为三个阶段：

```text
12C-R0：构建兼容、现状契约和 UI smoke 基线；
12C-R1：Profile + Settings + generated config 产品化；
12C-R2：统一预览工作区 + DiagnosticsDock + 最终验收。
```

不允许在 R0 fresh UI build 未通过时开始大范围 UI 重构。

## 2. R0 范围

```text
1. 解决 Qt 5.15.2 / MSVC 19.51 fresh build 兼容路线；
2. 建立可复现 CMake preset 或等价构建入口；
3. 固化现有 scenario/config/layer/overlay smoke；
4. 记录 1440x900、1280x720、1024x768 布局基线；
5. 冻结现有 panel 的复用边界。
```

构建候选必须在 R0 比较：

```text
A. 注册并固定兼容 Qt 5.15.2 的 VS2022/MSVC 工具链；
B. 在项目内增加最小 Qt 5.15.2/MSVC 19.51 compatibility shim；
C. 评估升级 Qt patch/LTS 版本的许可证、部署和回归成本。
```

R0 不升级依赖、不修改 Qt 头文件，必须先形成选择结论。

## 3. R1 范围

```text
1. 在 ScenarioRegistry 上增量固化 Profile metadata；
2. 缩减普通用户默认 Profile；
3. 新增 SliceSettingsModel；
4. 实现模板 + override + generated config；
5. 运行切片始终使用本次会话 effective config；
6. 统一配置帮助 metadata。
```

## 4. R2 范围

```text
1. PreviewWorkspace 统一三个现有预览 panel；
2. 共享 layerIndex、模式、图例和像素探针上下文；
3. 报告、曲线、日志进入可折叠 DiagnosticsDock；
4. 读取 12B-R2 utility report，只显示 candidate/diagnostic 语义；
5. 完成 UI smoke、布局检查、手册和阶段报告。
```

## 5. 架构边界

```text
Qt 仍只存在于 apps/slicer_debug_ui；
UI 只通过 config/report/package DTO 与 slicer_core 交互；
不修改 p0.rgbwsv.2；
不新增切片算法；
不默认启用 OpenVDB；
不将 utility promote 显示为 production replacement；
不删除 samples/configs regression fixture。
```

## 6. 退出标准

R0：fresh UI build、self-test 和关键 smoke 可复现。

R1：普通用户可通过 Profile + 设置生成 effective config，不需要手动编辑 JSON。

R2：统一预览与诊断区域完成，三种预览共享 layerIndex，布局无明显遮挡，手册和报告更新。
