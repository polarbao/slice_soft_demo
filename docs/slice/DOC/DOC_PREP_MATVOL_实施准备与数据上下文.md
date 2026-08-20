# DOC_PREP_MATVOL 实施准备与数据上下文

> 文档状态：**MATVOL-00 COMPLETE / MATVOL-01 PREPARED / IMPLEMENTATION NOT STARTED**
> 版本：v1.0 ｜ 日期：2026-08-20
> 决策：`DOC_DECISION_MATVOL_多材质纵深体积RGB与按需补白根治.md`
> 方案：`../DEV/DEV_MATVOL_逐层多材质所有权与白区载体兼容设计.md`

## 1. Implementation Plan

### Problem Type

Legacy 2.5D 高度场的多材质纵深语义缺失；不是 MTL 读取故障，也不是结果页图片翻转问题。

### Layer(s) Involved

```text
import/model metadata
geometry intersection and topology
material ownership/material composition
Stage 15 white carrier and closure
pipeline streaming and memory budget
reports/package preview/Qt profile
```

### Official Documents

- `DOC_DECISION_MATVOL_多材质纵深体积RGB与按需补白根治.md`
- `DEV_MATVOL_逐层多材质所有权与白区载体兼容设计.md`
- `DOC_DECISION_15_纹理纯白区按需补白与材料闭合修复专项.md`
- `DEV_15_纹理纯白区按需补白设计.md`
- `DOC_DECISION_16C_06_MEMFLOW_有界逐层流式内存根治.md`
- `DEV_16C_06_MEMFLOW_有界逐层流式切片设计.md`

### Historical Documents

`docs/archive/2026-06-30_slicer_legacy/dev/DEV_06_v0.2_3MFImporter_OBJMTLMaterialMapping设计.md`
仅作 C 级背景；它证明 MTL Kd/角色映射历史意图，不证明逐层纵深材质已实现。

### AI Workspace Evidence

```text
本机当前资产 hash、材质颜色、面数和边界边计数；
现有 Release slicer_cli 的 47 x 93 x 63 临时诊断；
输入材料映射开启后 43851 个非空 RGB 像素仍全部为 [63,190,126]。
```

这些证据尚在临时目录；MATVOL-01 必须转成仓库内确定性测试，禁止仅引用本次对话宣称 PASS。

### Current Code Reality

```text
ReliefColumnInfo 只保存 top_triangle_index/top_barycentric；
build_material_role_columns() 每列只解析最高三角面材质；
white_underbase 仅允许 Legacy full-volume texture，且拒绝 materialPolicy/roleMapping；
Stage 15 谓词 ApplyUnprintableWhiteCarrier 已是可复用纯函数；
MEMFLOW 已完成 Owned Layer、Occupancy 和 pre-shape Support 单层物化，生产仍为 Retained Dense。
```

### Current State

工作区表面预览正确读取绿色与浅桃色；生产层只能得到顶面材质沿列传播。结果页全通道组合还会把
S 通道显示为亮绿色，容易误判。

### Target State

显式候选 Profile 在同一 XY 列随 Z 输出正确材质 owner/RGB，并在最终 RGB 后复用 Stage 15 按需补白；
旧 Profile 逐字节不变，处理过程采用 bounded 单层物化。

### Historical State

历史 OBJ/MTL material mapping 只完成“输入面材质到列角色/颜色”的映射，不等于多材质体积布尔或
逐层 owner。不得把历史“已支持多材料”描述成当前问题已经解决。

### Pending Confirmation

MQ-01 壳层厚度、MQ-02 overlap 优先级、正式预算和 S3/S4 范围见 Decision §9。

### Risk Points

开放面伪体积、交点奇偶不稳定、材质重叠、Stage 15 组合放行错误、报告双计、dense 栈内存回归、
旧 Profile hash 漂移、结果页支撑伪彩色误导。

### Files To Change

首批预计只新增独立 DTO/provider/tests，不接生产；详细所有权见 DEV §9。任何 `slicer.cpp` 修改必须
遵循 wrap first，且不能与并行 MEMFLOW/RIPFLOW 脏改互相覆盖。

### Verification Plan

Unit -> synthetic oracle -> Reality 03 -> old Profile golden -> Package/Preview -> RIP strict -> Release memory/time。

## 2. 开工前数据包

每个执行 AI 必须记录：

```text
git branch --show-current
git status --short
compiler/CMake preset/configuration
slicer_cli --version
03.obj/03.mtl SHA-256
有效 Profile JSON/hash
DPI、层厚、姿态、支撑、压缩、采样策略
```

不得把不同姿态、DPI、层厚或 Profile 的结果直接做 before/after。

## 3. 固定 fixture

| Fixture | 用途 | 要求 |
|---|---|---|
| MV-F01 两个不重叠封闭盒体 | closed parity 基线 | 两种 Kd、逐层 owner 可手算 |
| MV-F02 两个重叠封闭体 | overlap | 显式优先级；同级 fail closed |
| MV-F03 封闭主体 + 开放顶面 | 当前资产最小复现 | reject 与 surface_band 两条路径 |
| MV-F04 多交点/空洞列 | interval stack | 不得 min/max 填平空洞 |
| MV-F05 纯白/近白材质 | Stage 15 | RGB 不改，差异仅 W |
| MV-F06 支撑/光油共存 | 组合顺序 | S/V/closure 不漂移 |
| Reality 03 | 最终真实模型 | hash 固定；资产许可未确认时只做本地 Gate |

测试生成器应优先在测试内构造小网格；不要依赖手工修改 Golden。

## 4. 原子卡准备状态

```text
MATVOL-01 PREPARED：只固化事实、oracle、报告 schema 草案，不改生产。
MATVOL-02 PREPARATION COMPLETE：DTO/错误/拓扑合同已在 DEV 冻结，待 01 Gate。
MATVOL-03..05 PREPARATION PARTIAL：实现顺序明确，依赖前卡实测。
MATVOL-06..10 PENDING：涉及 Stage 15/Host/生产接入，必须逐卡授权和验证。
```

## 5. 验收命令模板

目标名在相应实现卡创建后固定；不得在目标尚不存在时记录为已运行。

```powershell
cmake --build build-slicesoft/main --config Release --target `
  matvol_topology_unit_tests `
  matvol_interval_unit_tests `
  matvol_white_carrier_integration_tests `
  slicer_cli

ctest --test-dir build-slicesoft/main -C Release `
  -R "^matvol_" --output-on-failure

runtime/slicesoft/Release/rip_reader_test.exe `
  --package <MATVOL-package> --summary

git diff --check
git status --short
```

## 6. 停止条件

```text
需要修改 p0/SPI/Worker/RIP；
旧 Profile 任一 TIFF 像素漂移；
开放表面在未提供 policy 时被静默实体化；
同优先级材质重叠未 fail closed；
white carrier 修改 RGB/S/V 或新增 Z 层；
出现 O(material * layer * pixel) 所有权栈；
取消/错误后发布半包；
与并行脏改文件无法安全合并。
```

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-20 | v1.0 | 固化事实包、fixture、开工状态、验证模板与停止条件。 |
