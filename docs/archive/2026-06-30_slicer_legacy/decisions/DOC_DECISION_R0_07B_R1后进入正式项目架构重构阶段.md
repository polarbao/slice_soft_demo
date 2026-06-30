# DOC_DECISION_R0_07B_R1后进入正式项目架构重构阶段

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_07B_R1 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：P0 Demo 功能探索阶段收口，进入 R0 正式项目架构审查与重构设计阶段

---

## 1. 阶段判断

根据 `REPORT_07B_R1_UI真实OverlaySmoke与配置编辑器小收口当前实现状态.md`，07B-R1 已完成：

```text
1. preview.enabled=true 的 UI smoke fixture；
2. overlay-load-real smoke test；
3. preview_report.schema = p0.preview_report.1；
4. PreviewOverlayPanel 真实 package 加载；
5. RGB+W / RGB+V / RGB+S overlay 合成验证；
6. slicer_cli 生成 UiSmokeOverlayRgbwv；
7. self-test 通过；
8. quick regression 通过。
```

这说明 P0 Demo 的功能探索阶段已经具备完整闭环：

```text
输入 → 材料映射 → 纹理/颜色 → 支撑 → RGBWSV 输出 → 报告 → RIP 校验 → Qt 调试 UI → UI Smoke Test
```

因此，建议正式宣布：

```text
P0 Demo Feature Freeze
```

并进入：

```text
R0：正式项目架构审查与重构设计
```

---

## 2. 为什么现在进入 R0

当前 Demo 已经证明：

```text
1. OBJ/MTL/Texture 输入链路可跑通；
2. 3MF 输入链路可跑通；
3. 3MF deflate / validation / bad package 可跑通；
4. 3MF ColorGroup / Texture2DGroup 可跑通；
5. MaterialRoleMapping / MaterialPolicy / MaterialProcessProfile 可跑通；
6. Support / RGBWSV TIFF / RIP Reader 可跑通；
7. Qt Debug UI / Config Editor / Overlay / Smoke Test 可跑通。
```

继续增加功能会带来风险：

```text
1. Demo 结构继续膨胀；
2. model.cpp / slicer.cpp / UI 代码职责继续变重；
3. 配置字段继续扩散；
4. report schema 越来越难统一；
5. 后续 OpenVDB、支撑优化、设备联调会被 Demo 架构拖累。
```

所以现在应从：

```text
Feature-first
```

切换到：

```text
Architecture-first
```

---

## 3. R0 的性质

R0 不是编码大重构阶段。

R0 是：

```text
架构审查
模块边界定义
正式目录结构设计
配置 schema 设计
report/diagnostics schema 设计
pipeline 切分设计
测试/CI 分层设计
R1/R2 任务边界规划
```

R0 输出之后，再进入 R1 实际代码重构。

---

## 4. 必须纳入 R0 的新增策略约束

R0 必须纳入两个前置业务策略：

```text
1. TextureApplicationPolicy：彩色纹理全体积 / 表面壳层策略；
2. VarnishGeometryPolicy：光油外加 / 补偿式尺寸保持策略。
```

这两个策略不能继续作为临时 MaterialPolicy 分支硬编码。

它们必须成为正式项目的一等策略对象。

---

## 5. R0 不做什么

R0 不做：

```text
1. 不实现 surface_shell_texture；
2. 不实现 compensated_varnish；
3. 不引入 OpenVDB；
4. 不重写 slicer_core；
5. 不重写 Qt UI；
6. 不改 RGBWSV 输出协议；
7. 不新增设备通信；
8. 不做 RIP 半色调。
```

R0 只确定：

```text
正式项目如何容纳这些能力。
```

---

## 6. R0 完成后的路线

建议路线：

```text
R0：架构审查与正式项目重构设计
R1：核心模块边界重构
R2：配置 / 报告 / 测试 / CI 工程化收口
08：支撑形态与工艺优化
09：OpenVDB / SDF 几何内核预研
10：RIP / 设备链路真实集成
```

---

## 7. 结论

07B-R1 完成后，建议正式进入 R0。

R0 的首要产出不是代码，而是正式项目架构蓝图与重构计划。
