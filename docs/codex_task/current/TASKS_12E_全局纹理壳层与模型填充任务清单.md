# TASKS_12E 全局纹理壳层与模型填充任务清单

> 文档状态：R0 PREPARED / NOT ACTIVE
> 生成日期：2026-07-16
> 当前项目原子任务：无；12D-07 与 12E-01 均等待用户明确启动
> 规则：12E 文档已建立，但任何代码任务都不得自动开始

## 1. 阶段目标

实现完整三维模型上的 Texture Surface / Model Fill 互补分区，使纹理宽度可从工程最小值连续调节到模型全纹理阈值，并同步 Qt UI、report、preview、closure 和 regression。

固定边界：

```text
TextureSurface ∩ ModelFill = Empty；
TextureSurface ∪ ModelFill = Model；
width 增大时 texture 单调增加、fill 单调减少；
全纹理阈值处 fill=0、texture=model；
分类基于完整 3D 模型，不允许逐层二维近似冒充；
OpenVDB optional/OFF，不自动写 production TIFF；
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print。
```

## 2. 任务执行规则

```text
1. 每次只执行用户明确指定的一个 12E 原子任务；
2. 开始前运行 git branch --show-current 和 git status --short；
3. 阅读 Decision/PRD/DEV/DEMO/ROADMAP 和当前代码；
4. 报告无关 dirty state，不覆盖用户修改；
5. 大范围 production 接入前停下确认；
6. 每个任务运行定向验证和 git diff --check；
7. 不自动进入下一个任务；
8. 不提交，除非任务或用户明确要求。
```

## 3. 12E-00 文档准入

状态：DONE / DOCUMENTATION AND STARTUP PREPARATION ONLY（2026-07-16）

内容：

```text
新增 12E Decision/PRD/DEV/DEMO/ROADMAP/TASKS/CODEX_PROMPT；
新增 Config/DTO 准备、report schema、fixture/验收矩阵和启动状态报告；
在 12A PRD/DEV 中登记后续补充关系；
在 docs/slice 和 docs/codex_task 入口登记 planned stage；
不修改 C++、Qt、CMake、config fixture 或 production output。
```

完成标准：

```text
文档术语一致；
Current/Target/Historical/Pending 分离；
0.10 mm 工程下限、动态最大值和全纹理条件明确；
git diff --check 通过。
```

## 4. 12E-01 Config 与 DTO 契约

状态：PREPARED / READY FOR USER ADMISSION

目标：

```text
新增 global_surface_shell 配置 DTO、parser、validator、report DTO 占位；
新增 complement_of_global_texture_shell scope；
未实现 backend 时在切片/写包前以稳定错误明确阻断；
旧配置默认行为不变。
```

允许修改：

```text
src/slicer_core/config.*；
src/slicer_core/config/**；
新的 partition DTO/header；
tests/unit/experimental_config；
schema/docs；
最小 config fixture。
```

禁止：

```text
不生成 3D mask；
不接入 composer；
不改 UI；
不写 production package。
```

验证：config unit tests + `git diff --check`。

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R0_ConfigDTO契约准备.md；
docs/slice/DOC/DOC_SCHEMA_12E_TextureFillPartitionReport.md；
docs/slice/DOC/DOC_MATRIX_12E_全局纹理填充分区验收矩阵.md；
docs/slice/REPORT/REPORT_12E_启动准备状态.md。
```

## 5. 12E-02 Global Partition Service 骨架

状态：TODO

目标：

```text
新增 GlobalTextureFillPartitionService 和 backend-neutral result；
建立 model/texture/fill 3D mask 不变量检查；
支持 unavailable/blocked/diagnostic result；
不实现 production writer 接入。
```

完成标准：generated mask unit tests 证明 XOR/union/count invariants。

## 6. 12E-03 Legacy CPU 3D Distance Candidate

状态：TODO

目标：

```text
基于现有 mesh/BVH/geometry DTO 建立完整 3D inside/distance/closest-surface candidate；
计算 effective minimum、maxInteriorDistance、allTextureThreshold；
记录 runtime/peak memory；
严格 topology blocker。
```

禁止：

```text
不退化为逐层二维 morphology；
不在性能数据前宣称 production-ready；
不新增第三方依赖。
```

验证：box/sphere/thin-wall/cavity/topology unit/golden + benchmark report。

## 7. 12E-04 OpenVDB Conformance Adapter

状态：TODO

目标：

```text
复用 OpenVdbSurfaceShell/SurfaceTextureTransfer 生成同一 DTO 的 candidate 结果；
与 CPU candidate 比较 partition、distance、threshold、transfer stats；
OFF 返回 unavailable，不阻断默认 build。
```

禁止：

```text
不把 OpenVDB 设为默认；
不暴露 OpenVDB types；
不写 production TIFF；
不绕过 strict admission。
```

验证：OFF/ON 独立 build lane 和 conformance report。

## 8. 12E-05 Width Sweep 与 Report Schema

状态：TODO

目标：

```text
固化 slicesoft.texture_fill_partition.12e.1；
输出 requested/effective/min/max/allTexture；
输出 per-layer/totals overlap/unassigned/coverage；
实现 monotonic sweep validator。
```

完成标准：min/intermediate/max golden 全部通过，全纹理终点 fill=0。

## 9. 12E-06 Texture Transfer 与 Diagnostic Composer

状态：TODO

目标：

```text
使用 closest surface reference 为 TextureSurfaceMask3D 传递 OBJ/3MF 纹理；
按 Z layer 向 composer 提供 exact texture/fill masks；
先生成 diagnostic output/report，不改变默认 production path。
```

验证：OBJ/3MF/missing texture/missing UV/tie fixtures；outsideColored=0。

## 10. 12E-07 12D Closure 联动

状态：TODO

前置：12D semantic_masks exact contract 已可用。

目标：

```text
12D 直接读取 12E exact TextureSurfaceMask/ModelFillMask；
普通模式 ColorFillGap=0；
allTexture 模式 ColorFillGap=0 或 not_applicable(reason=all_texture_partition)；
repair disabled 不改 TIFF。
```

禁止：不提前实现或修改 12D repair 规则。

## 11. 12E-08 Production Admission

状态：TODO / REQUIRES CONFIRMATION

前置：

```text
R1/R2 正确性、性能、内存、topology、texture transfer 和 closure gate 通过；
用户确认 production 接入范围；
默认 OFF backend 可用，或另有正式架构决策。
```

目标：

```text
global_surface_shell 显式 Profile 可写 production RGBWSV；
allTexture 合法 fill=0；
旧 Profile 输出兼容；
RIP strict PASS。
```

本任务属于 production-path change，执行前必须再次给方案并等待确认。

## 12. 12E-09 Qt UI 设置与 Effective Config

状态：TODO

目标：

```text
新增全局三维纹理策略；
新增 width slider + QDoubleSpinBox，0.01 mm；
模型 preflight 动态 min/max/allTexture threshold；
coverage/partition/backend status；
session effective config；
保留 modelFill.material；
普通 UI 不暴露 backend 选择。
```

验证：self-test、UI smoke、三种窗口尺寸、最长文本无重叠。

## 13. 12E-10 Preview、Real Model Matrix 与收口

状态：TODO

目标：

```text
新增 Texture Surface/Model Fill/Partition preview；
真实 OBJ/3MF minimum/intermediate/allTexture matrix；
Release runtime/peak memory；
用户手册；
REPORT_12E。
```

完成标准：文档、代码、config、report、preview、UI、RIP 和 regression 一致。

## 14. 阶段完成标准

```text
12E-01..10 全部完成；
production admission 有明确 PASS 或 keep diagnostic 结论；
默认 OFF lane 和旧 Profile 无回归；
全纹理不是通过禁用 modelFill 实现；
REPORT_12E 记录实际命令、结果和残余风险。
```
