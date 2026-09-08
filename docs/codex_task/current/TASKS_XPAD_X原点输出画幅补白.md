# XPAD X 原点输出画幅补白

日期：2026-09-08。状态真源。用户授权分析、文档、分支隔离及直接实施，无需再次询问。

## Implementation Plan

### Problem Type
输出画幅策略扩展，不是自动排版、模型移动或几何修复。

### Layer(s) Involved
宿主切片设置/持久化/有效 Profile，核心 Scene 全局 Raster 合成；复用现有写包、报告和预览。

### Official Documents
FRAME 输出画幅与打印几何分离设计、TIFFVIEW 朝向统一合同、现有 SPI UTF-8/Profile hash 合同。正式方案见 `docs/slice/DOC/DOC_DESIGN_XPAD_X原点输出画幅补白.md`。

### Historical Documents
旧 FRAME 定位材质扩大 XY，不代表用户必须增加定位模型。本任务不重开 MEMFLOW 或改变定位命名。

### AI Workspace Evidence
从 `product/packaged-slicer` 新建 `codex/x-origin-canvas-padding`；无关模型、analysis、cache、docs/team-collaboration 保留。无需新工作树，运行时与源代码分离，验证前不覆盖 runtime。

### Current Code Reality
`MultiModelSliceOrchestrator` 从可见实例 Raster 的最小 originX/Y 求并集，丢弃并集外原点空白。用户十个 OBJ 的源顶点 XY 并集为 `(20.07984,1.506456)..(194.0495,59.67867)` mm，禁用定向/排版并且批次 XY 偏移为零时，X=0 到主体左端确有约 20.08 mm 空白。

### Current State
XPAD-00..05 已完成，覆盖 RGBWSV 与 RGBWSVT Scene。五项拆分提交已快进合入 `product/packaged-slicer`，开发分支 `codex/x-origin-canvas-padding` 已删除，未推送。实际验证见 `docs/slice/REPORT/REPORT_XPAD_X原点输出画幅补白收口总结.md`；现用程序重新启动导致部署保护拒绝覆盖，已另交付 `runtime/slicesoft-xpad-t/Release`。无关模型/缓存/团队文档仍留在工作树，未覆盖或混入提交。

### Target State
增加默认关闭的“补齐至 X=0”选项，仅扩大所有输出层左侧空白；模型变换、Y/Z/层数、主体字节和通道工艺不变。Profile 明确携带 scene 专用可选字段，旧配置缺省不变。

### Historical State
关闭时仍使用现有输出包围范围；原包不就地修改。

### Pending Confirmation
无。采用场景/打印平台 X=0，而非逐模型局部原点；只补左侧，不自动补 Y，不移动负坐标模型，不放宽越界准入。

### Risk Points
半像素对齐、重复补白、全空区域误喷墨、统计/manifest/预览尺寸不同步、溢出与额外内存/磁盘成本；失败必须保持原有原子写包保证。

### Files To Change
OutputConfig 及解析、MultiModelLayerComposeRequest/Orchestrator、ProductionService 接线；宿主设置、Profile bridge、WorkspaceState；独立测试与 CMake；仅最小改动，不重构其他专项。

### Verification Plan
默认关闭零漂移；开启全层左侧 255、主体逐字节一致；负/零/分数 X、不同 DPI、溢出、流式/保留式；宿主勾选、Profile hash、持久化；真实十模型 Scene 对照与 TIFF strict；定向回归和源码规模、diff 检查。不将离线测试称为物理打印验证。

## 任务清单

| 任务 | 状态 | 完成日期 | 验证 |
| --- | --- | --- | --- |
| XPAD-00 分析/准备/隔离分支 | COMPLETE | 2026-09-08 | 当前代码、真实源坐标及 FRAME 合同核查；PREPARED / IMPLEMENTATION GO |
| XPAD-01 核心画幅扩展与配置接线 | COMPLETE | 2026-09-08 | 默认/空列/主体字节/统计/像素相位/流式/溢出用例 PASS；真实十模型 600 DPI 全 23 层严格读包及主体字节零差异 |
| XPAD-02 UI 开关/Profile/持久化 | COMPLETE | 2026-09-08 | hb05/hb08 单元及 UI smoke PASS；默认关闭、Profile hash、持久化往返、T 组合显式拒绝 |
| XPAD-03 真实场景/回归/交付收口 | COMPLETE | 2026-09-08 | Release 构建成功；定向 CTest 9/10，唯一既有 adapter 失败单列；独立 runtime 部署及自检 PASS；不含物理打印 |

## 修订记录

## T 通道追加实施准备（2026-09-08）

- Problem Type / Layer(s) Involved：RGBWSVT Scene 的 Legacy 输出网格补白；不扩展几何采样域，不修改 T 归属或 V/T 冲突规则。
- Official Documents：本清单与 XPAD 设计、现有 RGBWSVT 协议/Legacy 输出实现。Historical Documents：六通道收口为历史已完成范围，本次授权替代“不支持 T”的限制。
- AI Workspace Evidence / Current Code Reality：分支 `codex/x-origin-canvas-padding`；T 通过 ProductionSliceFacadeFactory 单可见实例路径调用 run_slicer，不经过六通道合成器。保留工作树无关模型/缓存/团队文档，不纳入提交。
- Current State / Target State：已有 T+padding 拒绝门；目标移除该门，在 T 层最终合成后、首次写 TIFF 前补全七通道空列，同时接线输出网格、预览 mask、报告及返回尺寸。
- Historical State / Pending Confirmation：关闭时保留全部旧行为；用户已授权 T 支持、拆分提交、合入 product 并删除开发分支，无待确认。既有 T 单实例准入限制不扩大。
- Risk Points：误扩 T 材料域、统计分母或预览 mask 仍用旧宽度、七通道空白误打印、溢出、耗时测量顺序偏差。
- Files To Change：独立 Legacy 输出补白 helper，slicer.cpp 最小接线且不增加冻结文件行数，Profile gate/UI tests，独立 T 对照与耗时测试，文档收口。
- Verification Plan：T 非空多层七通道主体字节/空列/strict Reader/预览及报告；关闭、负/零/分数 X、溢出；现有 T/宿主/XPAD 回归；真实十模型 600 DPI 交替顺序重复 A/B 测时；构建、diff/规模门禁后按任务提交并快进合并。
- 准备裁决：PREPARED / IMPLEMENTATION GO。生产合入已由本轮用户明确授权。

| 追加任务 | 状态 | 完成日期 | 验证 |
| --- | --- | --- | --- |
| XPAD-04 T 通道输出补白 | COMPLETE | 2026-09-08 | 合成盒 3 层、真实 03.obj 32 层四对包均 PASS；七通道主体/补列/strict Reader/报告/PPM 一致，T 打印量不变；定向回归 21/22，唯一既有 adapter 失败 |
| XPAD-05 耗时实测/提交合入 | COMPLETE | 2026-09-08 | 两组均热身一对后交替测三对：六通道增量中位 0.648 s，T 样本 0.484 s；五项拆分提交已快进合入 product，祖先关系核查通过，开发分支已删除，未推送 |

## 历史修订

- 2026-09-08：完成准备，冻结左侧整像素扩展、默认关闭、不重写旧包、模型位置不变，用户已授权直接实施。
- 2026-09-08：完成核心与宿主接线；600 DPI 半像素用例发现重新取整的一列漂移，改为保留旧 offset 再加整列数，回归通过。RGBWSVT 独立路由本期不支持，UI 与核心显式拒绝。
- 2026-09-08：真实十模型 127 DPI/0.5 mm 与 600 DPI/0.2 mm 对照 PASS；600 DPI 扩宽 475 列，主体字节全层一致。源码规模门禁 PASS（60 项既有警告）；交付 `runtime/slicesoft-xpad/Release`，旧运行时不替换。GUI 人工交互和物理打印未验证。
- 2026-09-08：追加 T 支持完成，Release 构建、21/22 定向回归及两组重复测时收口。运行中的原软件未终止；T 兼容独立测试版部署、自检、四个二进制 SHA256 对照 PASS，原 output 文件数/总字节保持 3689 / 6751741933。
- 2026-09-08：完成拆分提交：`dd6f3d0d` 六通道核心、`735af5c0` T 输出、`c82ecc25` 宿主开关、`9b49b10c` 回归/测时、`a4ad4d70` 文档；`product/packaged-slicer` 从 `97afc31d` 快进至 `a4ad4d70`，祖先检查退出码 0，`git branch -d` 已删除开发分支。随后仅追加本次 Git 收口记录，不改已测代码。
