# REPORT_MATVOL-T RGBWSVT 缩裹材料通道当前状态

> 文档状态：**COMPLETE / T-00..T-09 COMPLETE / EXPLICIT PRODUCTION OPT-IN ADMITTED**  
> 版本：v2.0 | 日期：2026-08-26

## Current State

旧 DTO 与已发布 Package 仍固定为六通道 `p0.rgbwsv.2`。本分支已完成 Legacy 直接 CLI 的
`p0.rgbwsvt.1` 候选闭环：配置化识别、T-only 体积、逐层排他合成、七通道 LibTIFF、manifest、
最终落盘字节报告和 T Preview。Scene/Worker/参考 Host 已通过新增 `slice.rgbwsvt` 与
file_contract minor=1 完成受控透传；10 份新版工艺、显式 Host Profile、规范 hash 和 workspace v8
迁移已完成。T-09 已将合格 Host/Worker singleton Scene 路径准入为 `admitted`，直接 CLI 仍是
`rgbwsvt_candidate_unvalidated`。旧 `slice.rgbwsv`、minor=0、SPI v1 C ABI、旧工艺及默认 Profile 不变。

## Target State

新版工艺显式选择 `p0.rgbwsvt.1`，按配置的材质级 RGB/Kd 识别缩裹区域，严格求解其闭合体积，
逐层只写 T；甲片区域继续复用对应旧工艺的 RGB/W/V 语义。旧工艺与旧协议完整保留。

## Historical State

此前 MATVOL 文档和实现把 `01/02` 只当普通多材质 owner；本专项以前的分析曾误把 `01` 推断为缩裹。
用户已明确纠正：`01` 是甲片，`02` 是缩裹。该纠正只改变新专项目标，不回写历史资产证据。

## Pending Confirmation

T-09 已按用户授权收口，仓库内不再有本专项待办。外部 RIP 适配按用户输入视为完成；设备、实物打印
和现场 SLA 不属于本地已验证证据，不能由本报告扩写为 PASS。
用户已明确不修复 08/09，其当前缩裹几何固定作为 fail-closed 负例，不计入生产正例。

## 本轮结果

结论为 **方案可行，显式生产 opt-in 接线已完成**：

| 能力 | 结果 | 边界 |
|---|---|---|
| 配置化识别 | PASS | `03/08/09` 均按 Kd/RGB 命中材质 `02`；未硬编码名称或颜色 |
| `03.obj` T 体积 | PASS | material 02 生成非空逐层 occupancy；允许有界局部自交 |
| `08/09.obj` T 体积 | BLOCKED EXPECTED | material 02 当前有 3 条真开边；识别成功但拓扑 Gate 阻断 |
| 无缩裹模型 | PASS | 新协议下 T 全空，前六通道保持输入语义 |
| T 排他合成 | PASS | T 像素仅 T 打印，RGB/W/S/V 全空；越界 mask fail closed |
| 七通道 TIFF | PASS | stripped/tiled、none/packbits 精确 round-trip；handwritten 明确拒绝 |
| 旧协议兼容 | PASS（定向） | 旧配置仍解析为 `p0.rgbwsv.2`，既有 TIFF/MATVOL 定向回归通过 |
| Legacy compute-only | PASS | 03.obj 166 层完成生产姿态 plan 与逐层排他合成 |
| Legacy 七通道直接包 | PASS（候选） | direct CLI 保持 candidate；03 与无 T 投影通过，RGB/W/V 三工艺可重复 |
| Report/Preview | PASS | manifest、slice/material-process/transfer report 与 T Preview 使用最终七通道语义 |
| Scene/Worker/Host | PASS（显式生产） | 新 Profile + 新能力 + minor=1 + 单实例 Scene 写 admitted；默认仍走旧能力 |
| 工艺/Profile/Workspace | PASS（定向） | 10 份新工艺、外部 T 策略、规范 hash、workspace v8 与 v7 迁移完成；旧工艺零 diff |
| T-08 生产矩阵 | PASS（仓库内） | 双协议 strict RIP、03/08/09、无/坏 T、取消、确定性及 Release 相对资源门通过 |
| T-09 生产准入 | PASS（显式 opt-in） | 03 Worker Scene admitted；direct CLI candidate；未知/缺失状态严格拒绝 |

新增 10 份 `_rgbwsvt` 工艺副本，并在 Host 暴露彩色 RGB、白墨 W、光油 V 三个代表预设；
缩裹 RGB/Kd 与策略均来自外部 JSON，C++ 未固化颜色或材质名 `02`。15 份旧工艺 SHA256 零漂移。
新 Profile 显式写入协议、七通道和 T 策略并闭合规范 hash；旧 Profile 保持原序列化分支。
Workspace schema v8 已保存完整 T 策略，v7 迁移为旧协议且禁用 T，坏状态与未知版本 fail closed。
收口审查已将旧工艺 SHA256 和归一化旧 Profile hash 固化为自动化门禁，移除新工艺中的跨工作树
输出路径；缺外部 RGBWSVT 工艺时新 Profile 不可用，v7 携带 T-only 标识时拒绝迁移。

T-07 验证采用隔离 NMake Release 定向构建，新增/相关目标 MSVC `/W4 /WX` 通过；Host/Profile/
Workspace、合同、UI smoke 及 T-05/T-06 回归定向 CTest `32/32 PASS`；SourceSizeGuard 自测及全扫描
PASS（41 项既有 warning），Qt Host Boundary PASS，旧工艺目录零 diff，Host 硬编码审计 PASS。
未执行完整 225 项回归、T-08 完整 Package/RIP/取消/内存矩阵、设备或实物打印验证。
隔离 NMake 的 `slicer_ui_host_sim` 主目标完成链接后，在既有中文手册 POST_BUILD 复制处因路径转码
失败返回 2，故不计为主目标构建 PASS；相关定向目标与 5 项 UI smoke 均已通过。

T-08 使用同一 NMake Release 构建完成聚焦 CTest `4/4 PASS` 和证据脚本 PASS。03 的 RGB/W/V
七通道包均通过仓库内 strict RIP，08/09 稳定拓扑拒绝且不产包，无 T 投影、坏 T 拒绝、取消清理和
重复 TIFF 字节确定性均通过。三次运行的 Legacy/RGBWSVT 总耗时中位数为 `300.834/511.203 ms`，
峰值工作集为 `27,168,768/27,201,536` 字节，通过 T-08 同机相对门。完整 226 项 CTest 因未先完成
全目标构建而产生大量缺失可执行文件，不计为有效全回归；VS 主预设配置在编译器识别阶段挂起。

T-09 使用隔离 NMake Release 构建相关目标，MSVC `/W4 /WX` 通过；聚焦 CTest `12/12 PASS`，覆盖
schema/file_contract、架构分层、Facade 合同、Host Profile、真实 03 Worker Scene、strict Reader、
direct candidate 与既有 T-05/T-08 回归。T-08 Gate 随后复跑 `4/4 PASS` 且三份包 strict RIP PASS。
SourceSizeGuard 自测与全扫描 PASS（41 项既有 warning），Qt Host Boundary PASS；旧工艺和 08/09 模型
均无 diff。
本卡未执行完整 226 项 CTest、设备或实物打印验证；08/09 模型未修改。

## 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v2.0 | 完成 T-09：显式 Host/Worker Scene 路径准入，Reader/Worker 强制 admitted 合同；direct CLI 保持 candidate，专项收口。 |
| 2026-08-26 | v1.9 | 完成 T-08 仓库内生产矩阵：strict RIP、真实/负例、取消、确定性及 Release 相对资源 Gate PASS；仅剩 T-09 显式 opt-in 收口。 |
| 2026-08-26 | v1.8 | 完成多 Agent 收口修复：固定旧兼容基线、相对输出目录、缺外部工艺禁用、v7 T-only 标识拒绝及 Host 行数边界恢复；验证结论保持 32/32 PASS。 |
| 2026-08-26 | v1.7 | 完成 T-07：10 份新版工艺、Host 外部策略与显式 Profile、规范 hash、workspace v8/v7 迁移；定向 CTest 32/32、SourceSizeGuard 与 Qt Host Boundary PASS；生产仍未准入。 |
| 2026-08-25 | v1.6 | 完成 T-06：新增 `slice.rgbwsvt` 与 minor=1 双协议传输、singleton Scene Facade、动态 6/7 通道 Package Query 和 Host 显式路由；定向 CTest 14/14 PASS，生产仍未准入。 |
| 2026-08-25 | v1.5 | 记录 T-06 PREPARED：选择新增 `slice.rgbwsvt`、file_contract minor=1、单可见实例 Scene 和动态双协议 Package Query；未开始 Worker/Host 代码。 |
| 2026-08-25 | v1.4 | 记录 T-05C/05D 与 T-05 收口：最终七通道报告/T Preview、默认 CLI 候选写包、失败清理及 RGB/W/V 三工艺重复性通过；T-06 待独立 PREP，生产仍未准入。 |
| 2026-08-25 | v1.3 | 记录 T-05B：七通道 TIFF/manifest 窄候选接线及最终落盘字节统计完成；默认 Preview CLI 继续阻断，生产未准入；08/09 不修复。 |
| 2026-08-25 | v1.2 | 记录 T-05A：Legacy compute-only 生产姿态 session 与逐层合成已接入；输出/adapter 继续阻断，T-05B READY。 |
| 2026-08-25 | v1.1 | 记录 T-01..T-04 实现与验证：识别、几何、合成、schema、七通道 TIFF；登记 08/09 拓扑阻断及生产接线未完成。 |
| 2026-08-25 | v1.0 | 建立初始状态，记录角色更正、双协议目标和生产边界。 |
