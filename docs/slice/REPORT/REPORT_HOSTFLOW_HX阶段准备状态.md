# REPORT HOSTFLOW H-X 阶段准备状态

> 状态：**ACTIVE — H-D-01..05 已完成 / H-E-01..06 已完成 / E3_GATE=PASS**（H-A/H-B/H-C 本地完成，外部 ACK 延期）
> 日期：2026-08-10
> 范围：HOSTFLOW H-A、H-B、H-C、H-D、H-E，不属于 Stage 14 编号任务。

## Current State

| 任务组 | 准备状态 | 当前可执行范围 | 阻断 |
|---|---|---|---|
| H-A 场景生命周期 | COMPLETE | H-A-01..04 全部完成；DTO 当前为 v1.7 | 无切片侧阻断 |
| H-B 宿主业务 UI | COMPLETE | H-B-01..08 全部完成 | 无切片侧阻断 |
| H-C 移植交付 | COMPLETE | H-C-01/02/03 全部完成 | 无切片侧阻断；打印侧 ACK 延期 |
| **H-D 视图接线** | **CODE COMPLETE** | H-D-01..05 已完成 | H-D-06 仅等待人工七步证据 |
| **H-E 参数深度与导入** | **COMPLETE** | H-E-01..06 全部完成；E1/E2/E3 Gate 均 PASS | 无切片侧阻断 |

> 🔴 **2026-08-08 复核更正**：本文原状态为 `LOCAL COMPLETE`，**该结论不成立**。
> H-A/H-B/H-C 闭合的是**业务与数据链路**，两层缺口未覆盖：
>
> **① 显示链路未接线（A 级）** —— `ViewWorkspaceWidget.cpp:13-27` 两个画布是
> `QLabel` + 静态文字；渲染与交互七个类在 app 内的唯一引用者是批处理自检。
> 决策记录见 `DOC_DECISION_HOSTFLOW_H_D_R1_视图接线归属与14E_04d延期作废.md`。
>
> **② 参数深度不足（A 级）** —— `assets/hostflow_hc02_migration_plan.json` 中
> 8 项 `adapt_to_host_profile` 的 `replacement` 指向的宿主功能尚不存在
> （支撑编辑、材料工艺 Profile 编辑、生产纹理设置）；另缺 STL 与批量导入。
> 因此**「参考宿主等价于封装前切片软件」目前不成立**。
>
> ✅ **2026-08-08 用户裁决**：**HQ-09 = 乙**（提升到等价水位，分 E1/E2/E3 三批执行）；
> **HQ-10 = 甲**（场景/项目保存加载归 PrintApp，参考宿主不实现 ——
> 因此**重启后场景丢失是预期行为，不是缺陷**）。
> 权威记录：`DOC_DECISION_HOSTFLOW_H_E_R1_参考宿主目标水位裁决.md`。

H-A-02 已完成：Facade 支持 add/remove 与候选态原子提交；Adapter 支持既有 handle、inline scene、
受控隐式 scene 三条路径；import model resource 可映射到 scene authority；DTO v1.6 `sceneContext`
提供宿主权威 Profile/buildVolume。Debug/Release 两组测试均为 2/2 PASS。

H-A-04 已完成：DTO v1.7 在既有 `scene.apply_operation` 增加单操作 `applyGridLayout`，
复用 `GridLayoutPolicy` 完成稳定顺序、隐藏占位、锁定、10 mm 净距、11×2 和 22 实例排版；
成功只增加一次 revision，容量/参数/混批/replay 冲突均 fail-closed。Debug/Release 各 4/4 PASS。

H-A-03 已完成：纯 C 与 Qt 宿主都从空场景经 11 个导出完成 import、add、layout、transform、
slice 和 verify。宿主手工 scene builder 已移除；`scene.get_snapshot` 返回完整权威 scene 并由宿主
不透明透传给 Worker。源路径身份和 signed-zero scene hash 已收口。Debug/Release 联合回归各
5/5 PASS，合同与宿主边界门禁通过。

H-B-01 已完成：参考宿主增加 OBJ/3MF 文件选择，经公开 SPI 完成 `model.import`、
`scene.apply_operation(addInstance)` 与快速预检，并在右侧展示模型列表、导入元数据和预检问题。
缺失文件与不支持格式显式 fail-closed；Debug/Release 各 5/5 联合门禁通过。

H-B-02 已完成：参考宿主模型列表支持单选、Ctrl/Shift 多选、全选和批量删除；删除经一次
`removeInstance[]` 原子 Commit，选择结果在宿主本地同步到中央工作区且不触发 DLL 调用。
Debug/Release 各 6/6 联合门禁通过，主干 `multi-model-list` A/B 行为 smoke PASS。

H-B-03 已完成：参考宿主增加多选实例精确移动、旋转、缩放、X/Y 镜像和规则排版入口。
选择与参数编辑保持零 DLL 调用；变换 operations 批和 `applyGridLayout` 均以单次原子 Commit
推进一次 revision。Debug/Release 各 8/8 联合门禁通过，主干变换、镜像预检与规则排版三组
A/B 行为 smoke PASS。

H-B-04 已完成：用户授权 HQ-08-A，参考宿主拥有独立 Profile 目录，切片模块只通过既有
`pm_module_info` 提供能力；宿主结构化求交并展示可用性、缺失能力和生产安全等级。Profile
选择仅更新宿主 session 草稿且零 DLL 调用。Debug/Release H-B-01..04 联合门禁各 6/6 PASS，
模块边界/缺失模块/自检各 3/3 PASS，主干 `production-mode-selector` A/B smoke 均 PASS。

H-B-05 已完成：参考宿主增加切片设置页，宿主拥有 635/600 DPI、0.038 mm 层厚、输出目录、
RGB/W/V 实体材料策略和设备 buildVolume；有效 Profile 使用既有自哈希合同生成并实时预览。
首次导入把 Profile/buildVolume 注入 `sceneContext`，场景建立后异值修改 fail-closed；本地参数
编辑保持零 DLL 调用。Debug/Release H-B-01..05 联合门禁各 8/8、模块边界各 3/3，主干
`slice-settings-model` 与 `generated-effective-config` A/B smoke 均 PASS。

H-B-06 已完成：参考宿主经公开 SPI 提交真实 `slice.rgbwsv` Worker 作业，使用 QTimer
非阻塞轮询 queued/running/terminal 状态，支持协作取消和句柄单次释放。提交前闭合 scene snapshot、
有效 Profile 与输出身份；终态展示 code/message/detail/packageDir。取消在 2 秒内终结且无
`.staging`、`.backup`、`.lease` 残留。Debug/Release 宿主联合门禁各 13/13、Worker 合同与
取消门禁各 6/6 PASS。

H-B-07 已完成：参考宿主新增“结果”工作区，仅经冻结的五项 `package.*` 能力完成生产包校验、
协议/实例/Profile 摘要、逐层 descriptor、生产 TIFF 层预览、命名报告及 RGBWSV 六通道图。
场景生产 Writer 补齐真实 `perInstance/profileEcho` 摘要，Facade 对旧包仍保持 fail-closed。
Debug H-B-01..07 联合门禁 16/16、Release 含 package query 联合门禁 18/18 PASS；
Debug/Release 宿主边界与源码尺寸守卫各 4/4 PASS。

H-B-08 已完成：参考宿主使用 `hostflow.workspace.1` 保存窗口 geometry、工作区分栏、当前页、
Profile、DPI、层厚、输出目录、实体材料与设备 buildVolume；sceneHandle、Worker 作业句柄、模型
资源身份与预览缓存不进入持久化。旧 schema、损坏值和屏幕外窗口安全回退，自检进程不读写真实
用户设置。Debug/Release H-A/H-B 联合门禁各 18/18，宿主边界各 4/4 PASS。

H-C-01 已完成：历史 68 个头文件范围已校正为当前 77 个，并通过机器清单唯一归类为
`A=6 / B=41 / C=30`。A 桶直接复用单元通过无 core 及本地依赖闭包门禁；B 桶成为 H-C-02
逐文件改造指南输入，C 桶明确不进入打印宿主。

H-C-02 已完成：41 个 B 桶单元均有迁移动作、公开替代实现、工作流和相对复杂度，按
scene/profile/job/package 四个工作包形成 38-59 人日建议。机器门禁确认 H-C-01 B 桶与计划
双向集合完全一致；这是打印侧排期输入，不代表打印侧已经实现或验收。

H-C-03 已完成：主干和参考宿主使用同一规范化模型，并把主干
`textured_nail_rgb_only_lower_support` 与宿主 `host-reference-default` 映射为等价 Legacy RGB
实体语义。E3 收口后 13 条差异覆盖 8 个维度，结论为
`10 equivalent / 2 known_trim / 1 slicer_only`。
Debug/Release 主干 5 个 smoke、宿主 6 个 UI self-test 和 H-A/H-B CTest 各 18/18 PASS。

H-D-01 已完成：参考宿主使用 `TopViewRenderPolicy` 和真实 `top` ViewData 显示当前场景的
纹理俯视、buildVolume 与网格；滚轮缩放和中键平移完全在宿主本地完成。Debug/Release
H-D-01、14E-04d 与 H-A/H-B 联合门禁通过。

H-D-02 已完成并优先闭合 RB-P1：`SceneRenderPolicy` 从固定 `lod2` 改为 `auto`，参考宿主
显示真实纹理 3D 帧，并支持 orbit/pan/光标中心 zoom、七向预设和正交/透视；相机操作期
实测 DLL 调用为 0。Release 36 资产矩阵为 22 个完整 lod0、14 个冻结纹理合同显式资产
拒绝、0 个破碎降级；1 字节预算负例显式返回 `PM-SLICER-VIEWDATA-BUDGET` 并清空旧帧。

H-D-05 已完成：结果页只对当前已校验作业返回的精确 `packageDir` 开放系统目录入口，
空路径、目录不存在、包无效或身份不一致均 fail-closed。H-D-02..06 的准备审查已冻结：
H-D-02 已完成，H-D-03/04 已解除前置，H-D-06 继续等待两卡及人工证据。

H-E E1/E2 已完成并通过批次复核：H-E-01 闭合 ASCII/binary STL 与负例；H-E-03 建立
独立 Profile 编辑链；H-E-04 增加六种材料策略、角色映射和白墨/光油参数；H-E-05 增加
生产纹理、UV、缺失策略、非表面策略和 Stage 15 白区载体，并进入 canonical Profile、
自哈希和工作区 schema 4。E3 已解锁，按 H-E-02 → H-E-06 顺序实施。

H-E-02 已完成：OBJ/3MF/STL 多选批次在场景变更前完成全部资源导入与快速预检，随后以
一次原子 `addInstance[]` Commit 推进一次 revision；任一失败会释放本批资源，实例集合和
revision 不变。Debug/Release 批量原子性、UI smoke 与源码尺寸门禁均 PASS。

## Target State

```text
H-A-01 → HQ-07/v1.6 → H-A-02 → H-A-04 → H-A-03
                                      ↓
H-B-01..08 核心业务宿主 → H-C-01..03 移植交付物
```

H-A-03 已证明宿主只经 11 个导出实现：

```text
空场景 → import → addInstance → transform/layout → slice → verify
```

宿主不得构造内部 scene JSON；SPI v1、11 导出、15 能力、p0.rgbwsv.2 和 TIFF 均不变。

## Historical State

- Stage 14F 切片侧已收口，打印侧外部验收延期。
- H-A-01..04 已在延期窗口内完成合同和运行时闭环。
- 原 HOSTFLOW 草案把默认构建体积当作可直接使用的实现输入；当前审计已纠正为宿主权威。

## Remaining Decisions

1. **HQ-07 已关闭**：用户接受 `sceneContext`，DTO 已受控提升到 v1.6。
2. **H-A-04 已关闭**：`applyGridLayout` 已独立实现并通过门禁，不新增能力或导出。
3. 打印侧 ACK 继续为 `PENDING / DEFERRED`，不得写成 PASS。
4. **HQ-08 已关闭**：用户授权 HQ-08-A，Profile 目录归宿主，模块能力经 `pm_module_info`
   查询并求交；不得新增第 16 项能力、扩展 module_info 或读取内部场景 JSON。

## Next Action

H-D-01..05 已闭合；H-D-06 只等待人工七步证据，自动化不得代替人工 PASS。
H-E 下一张卡为 H-E-06；完成后执行 E3 批次复核。
RB-P2/RB-P3 与 R-A-02 保持独立 RENDER 后续，不阻断 H-E。打印侧 ACK 维持
`PENDING / DEFERRED`。
