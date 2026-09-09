# XYPAD XY 原点输出画幅补白

日期：2026-09-09。用户先授权分析、文档、隔离分支与直接实施，后授权覆盖运行版、同步 Git，以及合入对应 product 分支并删除已合入的本地功能分支；未授权推送。

## Implementation Plan

### Problem Type
对已完成 XPAD 的 Y 轴扩展，仅属于输出画幅策略，不属于几何或排版策略。
### Layer(s) Involved
核心配置、六通道 Scene 合成、七通道 Legacy 最终层输出、宿主控件/Profile/持久化、测试和说明。
### Official Documents
XPAD 设计与任务卡、TIFFVIEW max_y_first 行序合同。设计见 `docs/slice/DOC/DOC_DESIGN_XYPAD_XY原点输出画幅补白.md`。
### Historical Documents
旧 XPAD “不补 Y”是当时授权范围，本次明确扩展；旧包保持原样，不重新开启 MEMFLOW。
### AI Workspace Evidence
从 product/packaged-slicer 创建 codex/xy-origin-canvas-padding。保留先前未提交的用户手册/RIP配图、analysis、cache、团队文档及 gubao-xin-2D 素材；不回退、不混合提交。
### Current Code Reality
X/Y 均已按原栅格相位补空列/空行；六通道借用/消费/流式合成共享双轴 offset；T 独立最终输出 helper 同步扩宽与扩高，默认 Y=false。
### Current State
本地实现与定向验证完成；2026-09-09 用户确认覆盖后，已从独立验证版更新 runtime/slicesoft/Release 并通过宿主自检；三个拆分提交已按后续授权快进合入 product/packaged-slicer，本地功能分支已删除，未推送。真实十模型实际新增左 475 列、下 36 行，原模型像素保持不变；补白量以实际 Raster 为准。
### Target State
保留原 X 开关，新增默认关闭的独立 Y 开关及 output.scenePadToOriginY。可 X-only/Y-only/XY/全关。保留模型变换、采样相位、Z、层数、所有打印字节；同步六/七通道输出、预览和统计。
### Historical State
旧配置缺省 Y=false，旧 X=true 不能自动解释为 XY=true；不在切完之后二次重写文件。
### Pending Confirmation
无。使用场景平台零点，而非逐模型局部零点。负坐标不裁剪、不移动、不放宽准入。T 单可见实例限制维持。
### Risk Points
内部 minY-first 与磁盘 maxY-first 方向相反；半像素取整漂移；双轴面积溢出；T preview mask/空值与统计分母遗漏；重复补白；默认值/Profile hash 漂移。
### Files To Change
SceneCanvasPadding、SceneRasterTypes、Orchestrator/Composer/ProductionService、OutputConfig、LegacyTransferCanvas 与 slicer 最小接线；宿主设置/桥接/工作区；扩展 XPAD 回归与宿主测试；本卡及设计/手册。
### Verification Plan
Release 定向构建后运行双轴矩阵（正/负/零/分数、各向异性 DPI、半像素、多实例、借用/消费/流式、溢出）；六/七通道真实写包全层原字节/空区/统计/预览对照；宿主开关/Profile hash/持久化；真实十模型按源姿态 XY 对照；保持当前运行程序不受影响，验证版独立部署。

## 清单

| 任务 | 状态 | 完成日期 | 实际验证 |
|---|---|---|---|
| XY-00 准备与分支隔离 | COMPLETE | 2026-09-09 | 读取 XPAD、TIFFVIEW 和当前两条生产路由；PREPARED / IMPLEMENTATION GO |
| XY-01 双协议核心 Y 补白 | COMPLETE | 2026-09-09 | Release 构建成功；六/七通道四态、原字节、空区、原始 TIFF 扫描行、统计与预览定向测试通过 |
| XY-02 独立 Y 开关与配置兼容 | COMPLETE | 2026-09-09 | hb05/hb08 及 UI smoke 通过；独立开关/Profile hash/持久化和旧 X-only 兼容通过 |
| XY-03 全层对照/真实资产/文档交付 | COMPLETE | 2026-09-09 | CTest 9/9 PASS；真实十模型 23 层、T 模型 32 层全层对照通过；独立部署自检与四程序哈希一致；手册及实际 UI 图 21 更新；未验证物理打印 |

实际参数、首次测试断言修正及交付边界见[验证报告](../../slice/REPORT/REPORT_XYPAD_XY原点补白验证与交付.md)。runtime/slicesoft/Release 已按用户确认覆盖运行文件；output/configs/model/samples 和用户设置未改动，被替换文件已备份；此前手册/RIP/素材改动均保留。

## 修订

- 2026-09-09 r1：冻结独立轴开关、默认兼容、下侧空白和整像素不重采样合同；授权范围不包含 Git 自动提交/合并。
- 2026-09-09 r2：XY-01..03 本地范围收口，记录 9 项定向回归及真实六/七通道全层证据，交付独立验证版、文档和实际界面配图；不声明全仓回归或物理打印通过。
- 2026-09-09 r3：用户确认覆盖，原 Release 目录更新完成；86 个部署文件哈希一致，宿主自检通过；不涉及 Git 提交或分支合并。
- 2026-09-09 r4：用户授权同步 Git 分支，核心与测试提交 12bca48d，宿主与兼容测试提交 e4cb2d27；文档及图 21 单独收口提交。提交前定向 CTest 再次 9/9 PASS（9.21 s）。其他手册/RIP/素材改动保留未提交，不合并或推送。
- 2026-09-09 r5：用户授权合并及清理，product/packaged-slicer 从 88f087ed 快进至 e87052fc，完整保留三个拆分提交；确认可达后用 git branch -d 删除 codex/xy-origin-canvas-padding。其他专项分支未删除，原有未提交文件内容哈希和状态核对保持不变；未推送远端。
