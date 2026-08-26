# TASKS_MATVOL-T RGBWSVT 缩裹材料通道任务清单

> 文档状态：**COMPLETE / T-00..T-09 COMPLETE**  
> 分支：`codex/matvol-t-channel-protocol`  
> 日期：2026-08-26

## 1. 边界

旧 `p0.rgbwsv.2`、旧工艺文件、默认 Profile、SPI v1 与 RGBWSV 极性保持不变。
新版协议为显式 opt-in；RIP 外部适配视为已完成，但仓库内合同测试不得省略。

## 2. 原子卡

| 卡 | 交付 | 状态 | 前置 | 日期 |
|---|---|---|---|---|
| T-00 | 决策、DEV、任务卡、初始报告与独立分支 | **COMPLETE** | 用户授权 | 2026-08-25 |
| T-01 | 配置化材质 RGB resolver、稳定错误与 01/02 角色回归 | **COMPLETE** | T-00 | 2026-08-25 |
| T-02 | T-only 过滤网格、拓扑 Gate、compact plan 与单层 mask | **COMPLETE** | T-01 | 2026-08-25 |
| T-03 | 六转七单层排他合成与 W/V/RGB 工艺 fixture | **COMPLETE** | T-02 | 2026-08-25 |
| T-04 | `p0.rgbwsvt.1` schema、LibTIFF 7 sample Writer/Reader | **COMPLETE** | T-03 | 2026-08-25 |
| T-05 | Legacy CLI 生产候选接线、manifest/report/preview | **COMPLETE** | T-04 | 2026-08-25 |
| T-06 | Scene/Worker/Host 新协议透传与能力协商 | **COMPLETE** | T-05 | 2026-08-25 |
| T-07 | 新旧工艺文件双轨迁移与 workspace/profileHash | **COMPLETE** | T-06 | 2026-08-26 |
| T-08 | 03/08/09、无 T、坏 T、Package/RIP/取消/内存矩阵 | **COMPLETE** | T-07 | 2026-08-26 |
| T-09 | 用户生产 opt-in 回签与专项收口 | **COMPLETE** | T-08 | 2026-08-26 |

## 3. T-00 实际结果

```text
branch/worktree 隔离完成；原 product/packaged-slicer 脏改未带入；
材质 01=甲片、02=缩裹的用户裁定已冻结；
颜色识别配置化，不按文件名或材质名硬编码；
冻结 p0.rgbwsvt.1 = R G B W S V T；
旧工艺保留，新工艺新增副本；
建立 G1..G9 生产 Gate。
```

验证：文档定向检索、`git diff --check`。未执行 C++ 构建。

## 4. T-01..T-04 实际结果

```text
T-01  新增 transferChannelPolicy 与 output.packageProtocol；按配置的材质 Kd/RGB
      精确匹配，材质名只在匹配成功后作为几何 key；缺失/多匹配/非法配置有稳定错误。
      03/08/09 均识别材质 02，且未按名称 02 自动推断角色。

T-02  在既有 MaterialVolumePlan 上增加可选单材质过滤，保留全网格拓扑上下文；
      新增 transfer compact plan 与调用方持有的逐层 mask。
      03.obj 已实际生成 material 02 非空 T occupancy；无 T 返回空 mask；开面 fail closed。
      08/09 仅完成颜色识别，其材质 02 当前有 3 条真开边，尚不能计入体积切片 PASS。

T-03  新增 RGBWSV -> RGBWSVT 单层合成器：非 T 像素前六通道逐字节复制，
      T 像素清空 RGBWSV 后只写 T，越出 model mask 失败。
      新增 RGB、白墨、光油三份 `_rgbwsvt` 工艺原型副本，旧文件未覆盖。

T-04  新增 contracts/p0.rgbwsvt.1.schema.json；LibTIFF 支持显式 7 sample，
      strict Reader 支持 stripped/tiled 与 none/packbits；handwritten Writer 明确拒绝 7 sample。
      Legacy 生产入口仍主动拒绝启用 T，防止在 T-05 前产出不完整 Package。
```

构建与验证：

```text
NMake Release 定向构建：PASS（MSVC /W4 /WX）
新增测试：resolver 9/9、volume plan 3/3、composer 5/5、TIFF/schema 6/6
定向 CTest：8/8 PASS
  experimental_config_unit_tests
  tiff_writer_contract_unit_tests
  matvol_facts_unit_tests
  matvol_reality_plan_tests
  matvol_transfer_resolver_unit_tests
  matvol_transfer_volume_plan_tests
  matvol_rgbwsvt_composer_tests
  matvol_rgbwsvt_tiff_io_tests
```

## 5. 停止条件

```text
旧 p0.rgbwsv.2 任一字节漂移；
未显式选择新版工艺却输出 T；
按材质名 02 或固定 RGB 硬编码；
匹配到 T 但拓扑失败时回落普通甲片；
T 与 RGB/W/S/V 在同一像素同时打印；
handwritten Writer 静默丢弃 T；
外部 RIP 已适配被误写成几何/设备打印已验收。
```

## 6. T-05 实施准备

准备文档：`docs/slice/DOC/DOC_PREP_MATVOL_T_T_05_Legacy生产候选接线准备.md`。

```text
T-05A  Legacy 直接 CLI 范围 Gate、生产姿态 plan 与逐层排他合成
T-05B  七通道 TIFF、manifest 与最终落盘字节统计
T-05C  transfer report 与 T Preview
T-05D  CLI 候选端到端、失败清理和旧协议定向零漂移
```

用户已授权继续后续开发，无新增产品输入阻塞；每张后续主卡仍须先建立独立 PREP 文档。

T-06 准备文档：`docs/slice/DOC/DOC_PREP_MATVOL_T_T_06_SceneWorkerHost双协议透传准备.md`。
已按冻结方案完成新增 `slice.rgbwsvt`、file_contract minor=1、单可见实例 Scene 候选、
Package Query 动态 6/7 通道 DTO 与 Host 显式能力选择；旧能力和旧请求 minor=0 保持不变。

## 7. T-05A 实际结果

```text
新增 LegacyTransferChannelSession，按最终生产姿态和同一 raster grid 一次构建
T-only compact plan，并复用 caller-owned scratch 逐层生成排他 RGBWSVT。

真实 03.obj：compute-only Legacy CLI 166 层通过，T 材质解析为 02 且产生非空 T；
真实 08/09.obj：分别按其配置颜色命中 02，因开放拓扑在 session 构建期 fail closed；
无缩裹区域：前六通道逐字节保留，T 全空；
T-05A 完成时普通输出、callback、Scene/override 继续返回稳定错误，未产出伪七通道包。
```

验证：NMake Release `/W4 /WX` 定向构建 PASS；定向 CTest `10/10 PASS`；
`matvol_legacy_transfer_session_tests` 为 `4/4 PASS`；SourceSizeGuard 自测及全扫描 PASS，
`slicer.cpp` 相对比较基线净减少 1 行；`git diff --check` PASS。

## 8. T-05B 实际结果

```text
Legacy 逐层写包已支持显式 p0.rgbwsvt.1：每层经 LibTIFF 写入 7 sample TIFF，
随后立即严格回读并校验尺寸、存储、压缩及最终像素字节；manifest 的 R/G/B/W/S/V/T
统计只从这些最终落盘字节累计，statisticsSource 固定为 persisted_tiff_bytes。

新版 manifest schema 冻结 channelCount=7、R G B W S V T 顺序、uint8 contiguous、
black_is_print 极性及七通道统计结构；缺 T、缺最终字节统计或统计来源错误均被拒绝。

真实 03.obj 在“TIFF + manifest/report、Preview 关闭、无 callback/override”窄候选范围内
完成七通道 Package；无缩裹匹配时前六通道相对旧协议逐字节一致且 T 全 255。
默认 CLI 因仍请求旧 Preview 而继续返回 E_MATVOL_T_PROTOCOL_INVALID，等待 T-05C/D。
08/09 保持 EXPECTED_BLOCKED，不进入任何修复工作。
```

验证：NMake Release `/W4 /WX` 定向构建 PASS；定向 CTest `12/12 PASS`；
新增 metadata 用例 `3/3 PASS`、Legacy Package 集成用例 `3/3 PASS`；
SourceSizeGuard 自测及全扫描 PASS，`slicer.cpp` 相对比较基线净增长 0 行；
旧 `samples/configs/material_process` 无修改，`git diff --check` PASS。

## 9. T-05C/05D 实际结果

```text
transfer_channel_report.json 已冻结为 slicesoft.transfer_channel_report.1，记录识别配置、
材质 02/区域状态、逐层及总 T 像素、七通道统计和排他重叠计数；统计来源固定为
persisted_tiff_bytes。material_process_report 与 slice_report 同步改用最终落盘七通道统计，
不再把已转移到 T 的区域虚报为 RGB/W/S/V。

Preview 支持 transfer/t，按 manifest 协议使用 7 字节 stride 并生成 transfer_t 伪彩预览；
默认 Legacy CLI 已可在不依赖内部 override 的情况下完成 RGBWSVT 候选写包。

新增候选目录 Guard：目标目录已存在时拒绝覆盖，未完成运行自动清理本次新建目录，
成功写完 TIFF、报告和 manifest 后才提交。RGB、白墨、光油三份新版工艺均完成候选包；
同一 RGB 工艺重复两次的全部 TIFF 解码字节一致。旧工艺目录保持未修改。

08/09 继续只作为“颜色识别成功、开放拓扑 fail closed”负例，不开展修复。
```

验证：NMake Release `/W4 /WX` 定向构建 PASS；定向 CTest `13/13 PASS`；
metadata `5/5 PASS`、Legacy Package `3/3 PASS`、三工艺 CLI `4/4 PASS`；
SourceSizeGuard 自测及全扫描 PASS，`slicer.cpp` 相对比较基线净减少 72 行；
旧 `samples/configs/material_process` 无修改，`git diff --check` PASS。

## 10. T-06 实际结果

```text
新增 Worker 能力 slice.rgbwsvt 与 p0.rgbwsvt.1 严格配对；旧 slice.rgbwsv 继续只接受
p0.rgbwsv.2/minor=0，新能力使用 minor=1。Module/Worker discovery 同时声明双能力、双产物，
错配 capability/minor/output contract 在执行前 fail closed。

SliceRequest 增加显式 output_contract。生产 Facade 对旧协议继续进入原多模型路径；新协议仅允许
Legacy 单可见实例 Scene，并将正式 instance transform 透传给 T-05 Runner；零/多实例不裁剪、不回退。

Package Query 按 manifest.schema 分派六/七通道严格 Reader，公共只读 DTO 改为动态通道集合；
summary/verify/layer descriptor/report/T 单通道 preview 均以包内协议为准。参考 Host 根据有效
Profile 的 packageProtocol 选择新旧能力，模块缺少新能力时禁止提交；Package Review 动态显示 6/7 通道。

修复了新能力未加入 Worker dispatcher 已知能力表导致的进程 fast-fail，并增加 RGBWSVT 分派回归；
Worker 无诊断异常现在回传 process exit code。08/09 未修改，继续作为拓扑 fail-closed 负例。
```

验证：NMake Release `/W4 /WX` 定向构建 PASS；合同、Module、Worker、Package Query 与 Host
定向 CTest `14/14 PASS`；SourceSizeGuard 自测及全扫描 PASS；旧工艺目录未修改。
未执行完整 224 项回归、T-08 RIP/性能矩阵或设备打印。

## 11. T-07 实际结果

```text
旧 samples/configs/material_process 目录保持零 diff，15 份既有工艺 SHA256 逐项复核无漂移；
在 samples/configs/matvol_t/process_profiles 新增 10 份独立 `_rgbwsvt` 工艺副本，
全部显式声明 p0.rgbwsvt.1、R/G/B/W/S/V/T 与外部 transferChannelPolicy，输出目录均为安全相对路径。

参考 Host 新增外部工艺加载器，不在 C++ 中固化缩裹 RGB/Kd 或材质名 02；
新增 host-reference-transfer-channel 显式 Profile，并从新工艺中提供 RGB、W、V 三个代表预设。
旧 Profile 继续走旧序列化分支；新 Profile 将协议、七通道和完整 T 策略纳入规范 hash。
15 份旧工艺固定 SHA256 与去除运行路径差异后的旧 Profile 固定 hash 已进入自动化门禁；
外部 RGBWSVT 工艺缺失时新 Profile 显式不可用，不形成可选但不可执行的空入口。

Workspace schema 升级到 v8，保存并恢复协议与 T 策略；v7 明确迁移为旧 RGBWSV + T disabled，
v7 携带后加的 T-only Profile/preset 标识、未知版本、协议/启用错配及未知 T 策略均 fail closed。
Package Review 对旧空通道 DTO 保留六通道兼容；Host 协议路由已拆出独立模块并守住既有债务行数。
08/09 模型及资产未修改，也未开展任何拓扑修复。
```

验证：隔离 NMake Release 定向构建 PASS（新增/相关 C++ 目标均 `/W4 /WX`）；T-07 Host/Profile/
Workspace、合同、UI smoke 及 T-05/T-06 回归共 `32/32 PASS`；SourceSizeGuard 自测与全扫描 PASS
（41 项既有 warning），Qt Host Boundary PASS；旧工艺目录零 diff，Host 硬编码审计 PASS。
未执行完整 225 项回归、T-08 Package/RIP/取消/内存矩阵或设备打印。
补充：`slicer_ui_host_sim` 主目标已完成链接，但隔离 NMake 的既有中文手册 POST_BUILD 复制发生
路径转码失败并返回 2，因此未把该主目标记为构建 PASS；相关定向目标和 5 项 UI smoke 均通过。

## 12. T-08 实际结果

```text
rip_reader_test 已按 manifest.schema 分派六/七通道严格 Reader；旧 RGBWSV 路径保持原行为。
真实 03 RGB/W/V 三工艺严格 RGBWSVT Package/RIP 通过，T 非空且与 RGBWSV 排他；
08/09 均稳定 TopologyInvalid 且不产生 Package，模型文件未修改；无 T 前六通道精确一致且 T 全空；
篡改 T 统计稳定 E_LAYER_STATISTICS_MISMATCH；Package 预留后取消不残留半包；
同配置重复运行的全部生产 TIFF 字节一致。
```

验证：隔离 NMake Release 定向构建 PASS，T-08 聚焦 CTest `4/4 PASS`，证据脚本 PASS。
三次运行中 Legacy 总耗时中位数 `300.834 ms`、RGBWSVT `511.203 ms`；峰值工作集分别
`27,168,768` 与 `27,201,536` 字节，均通过同机宽松相对门。外部 RIP 适配仅按用户输入记录，
未在本地重测；设备和实物打印未测试。完整 226 项 CTest 因未先完成全目标构建而出现大量
缺失可执行文件，不计入回归结论；VS 主预设配置在编译器识别阶段挂起，亦不计为构建结果。

## 13. T-09 实际结果

```text
host-reference-transfer-channel 已从 restricted 升级为 production，但仍是非默认、显式选择的独立 Profile；
只有 slice.rgbwsvt + minor=1 + p0.rgbwsvt.1 + singleton Scene/Worker 路径满足全部合同后，
Runner 才写 manifest.productionAcceptance=admitted。直接 slicer_cli 和缺少完整 Scene 资格的调用仍写
rgbwsvt_candidate_unvalidated，不能借内部布尔值绕过准入。

p0.rgbwsvt.1 schema 与严格 Reader 已冻结 admitted/candidate 两个合法状态；缺失或未知状态 fail closed。
Worker 对 RGBWSVT 产物再次执行严格包校验并要求 admitted，否则返回 PM-SLICER-CONTRACT-0060。
03 Scene/Worker 正例通过；08/09 继续稳定拓扑拒绝，未修改模型、未修复、未回退。
旧 Profile、旧工艺目录、slice.rgbwsv、minor=0 与 p0.rgbwsv.2 默认路径保持不变。
```

验证：隔离 NMake Release 相关目标构建 PASS（MSVC `/W4 /WX`）；T-09 聚焦 CTest `12/12 PASS`
（11 项核心/合同测试加 1 项 Host Profile 测试），其中真实 03 Worker Scene 包为 `admitted`、direct CLI
仍为 candidate、未知准入值被严格拒绝。T-08 证据脚本复跑 `4/4 PASS` 且 strict RIP PASS。
SourceSizeGuard 自测与全扫描 PASS（41 项既有 warning），Qt Host Boundary PASS；旧工艺目录及 08/09
模型均为零 diff，`git diff --check` PASS。
完整 226 项 CTest 未在本卡重跑；设备和实物打印仍未测试，外部 RIP 适配只按用户输入接受。

## 14. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.14 | 完成 T-09：显式 Host/Worker Scene RGBWSVT 路径准入，manifest/Reader/Worker 冻结 admitted 合同；direct CLI 保持 candidate，03 正例通过，08/09 保持 fail closed；专项 T-00..T-09 收口。 |
| 2026-08-26 | v1.13 | 完成 T-09 独立 PREP：用户授权计入 G9，冻结仅 Scene/Worker 显式新协议 admitted、direct CLI candidate、严格 Reader 准入字段与旧默认零漂移边界。 |
| 2026-08-26 | v1.12 | 完成 T-08：双协议 strict RIP、03 正例、08/09 稳定拒绝、无/坏 T、取消清理、TIFF 确定性与 Release 相对资源 Gate PASS；T-09 转 READY。 |
| 2026-08-26 | v1.11 | 完成 T-08 独立 PREP：冻结 03 正例、08/09 拒绝、无/坏 T、双协议 strict RIP、取消清理、确定性与 Release 相对资源 Gate。 |
| 2026-08-26 | v1.10 | 完成多 Agent 收口审查修复：移除跨工作树输出路径，固化旧工艺/归一化旧 Profile hash 门禁，缺外部工艺时禁用新 Profile，拒绝 v7 T-only 标识，并恢复 Qt Host Boundary。 |
| 2026-08-26 | v1.9 | 完成 T-07：10 份 `_rgbwsvt` 工艺双轨、Host 外部工艺加载与新 Profile、规范 hash 闭合、workspace v8/v7 迁移；定向 CTest 32/32、SourceSizeGuard 与 Qt Host Boundary PASS；T-08 转 READY。 |
| 2026-08-25 | v1.8 | 完成 T-07 独立 PREP：冻结旧工艺 SHA256/旧 Profile hash、`_rgbwsvt` 双轨、新 Profile 分支和 workspace v8/v7→v8 迁移 Gate；T-07 转 PREPARED。 |
| 2026-08-25 | v1.7 | 完成 T-06：双版本 file_contract、双 Worker 能力、singleton Scene RGBWSVT Facade、动态 6/7 通道 Package Query 与 Host 显式路由接线；定向 CTest 14/14、SourceSizeGuard 全扫描 PASS；T-07 转 READY。 |
| 2026-08-25 | v1.6 | 完成 T-06 开工准备：新增能力而非改写旧能力，冻结 minor=1 协商、singleton Scene、双协议 Package Query 与 Host 路由边界；T-06 转 PREPARED，代码未开工。 |
| 2026-08-25 | v1.5 | 完成 T-05C/05D：最终七通道 report/Preview、默认 Legacy CLI 候选写包、失败目录清理、RGB/W/V 三工艺重复性与旧协议定向兼容回归；T-05 收口，T-06 转 READY 但须先建立 PREP。 |
| 2026-08-25 | v1.4 | 完成 T-05B：Legacy 七通道 TIFF/manifest 窄候选接线、最终落盘字节回读统计、03 与无 T 集成回归；默认 Preview CLI 继续 fail closed，T-05C 转 READY；08/09 明确不修复。 |
| 2026-08-25 | v1.3 | 完成 T-05A：Legacy compute-only 生产姿态 session 与逐层排他合成；03 正例、无区域正例、08/09 拓扑负例通过；输出与适配器继续 fail closed。T-05B 转 READY。 |
| 2026-08-25 | v1.2 | 完成 T-05 实施准备并经双 Agent 只读复核，拆为 05A..05D；冻结 Legacy 直接逐层写包形状、slicer.cpp G2 不增长、T-06 前 callback/Scene fail-closed；排期为乐观 7 次、建议 10 次会话。T-05 转 PREPARED。 |
| 2026-08-25 | v1.1 | T-01..T-04 完成：配置化 resolver、T-only plan/mask、排他合成、RGBWSVT schema、七通道 LibTIFF Writer/Reader 与新工艺原型副本；定向 CTest 8/8 PASS。T-05 转 READY，生产入口继续 fail closed。 |
| 2026-08-25 | v1.0 | 创建专项，完成 T-00，冻结双协议、配置化颜色、角色更正、双工艺与任务顺序。 |
