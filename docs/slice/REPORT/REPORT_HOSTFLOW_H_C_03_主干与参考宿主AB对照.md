# REPORT HOSTFLOW H-C-03 主干与参考宿主 A/B 对照

> 状态：**LOCAL PASS / EXTERNAL ACK DEFERRED**
> 日期：2026-08-10
> 原子任务：H-C-03

## 1. 结论

主干 `slicer_debug_ui` 与参考宿主 `slicer_ui_host_sim` 已在同一规范化模型和等价 Profile 语义下完成业务对照。13 条差异记录全部归入：

- `equivalent`：10 条；
- `known_trim`：2 条；
- `slicer_only`：1 条。

核心操作员流程已经闭合：

```text
空场景 → 导入模型 → 选择/排版 → 选择 Profile → 调整参数
       → 提交/取消切片 → 校验生产包 → 查看层与通道 → 恢复工作区
```

该结论表示业务语义等价，不表示两个 UI 视觉一致、Profile id 相同或打印侧已完成验收。

## 2. 规范化输入

双方使用 `samples/models/openvdb/surface_shell_cube_no_uv.obj`。主干 Profile 为
`textured_nail_rgb_only_lower_support`，参考宿主 Profile 为 `host-reference-default`；二者映射为
Legacy、全实体 RGB、RGBWSV 严格包语义。公共参数为 635/600 DPI、0.038 mm 层厚、RGB 实体和
230 x 100 x 60 mm buildVolume。

Profile 名称不同是宿主拥有 Profile 目录的冻结设计，不是行为偏差。

## 3. 已知差异

| 类别 | 差异 | 结论 |
|---|---|---|
| 导入 | 两端支持 OBJ/3MF/STL；参考宿主支持批量原子导入和严格白区非阻断预检 | `equivalent`；更多切片专用诊断仍归 `slicer_only` |
| Profile | 主干读内部 ScenarioRegistry，参考宿主使用自有目录并与 `pm_module_info` 求交 | `known_trim`；符合 HQ-08-A |
| 作业 | 主干经 CLI/QProcess，参考宿主经 Worker ABI | `equivalent`；用户终态与取消语义一致 |
| 结果 | 参考宿主保留生产包、层、通道和命名报告，不复制全部工程调参面板 | `known_trim` |
| 诊断 | OpenVDB、拓扑修复、原始配置和深度工艺诊断只留主干 | `slicer_only` |

完整机器清单见 `assets/hostflow_hc03_ab_matrix.json`。

## 4. 自动化证据

Debug 与 Release 均执行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/RunHostflowAbComparison.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File scripts/RunHostflowAbComparison.ps1 -Configuration Release
```

每条轨道包含主干 5 个 smoke、参考宿主 6 个 UI self-test、`^hostflow_h[ab]` 联合 CTest 和矩阵门禁。

结果：

- Debug：`HOSTFLOW_HC03_PASS config=Debug main=5 host=6`；H-A/H-B CTest `18/18 PASS`；
- Release：`HOSTFLOW_HC03_PASS config=Release main=5 host=6`；H-A/H-B CTest `18/18 PASS`；
- 矩阵：`13` 条、`8` 个维度、`10 equivalent / 2 known_trim / 1 slicer_only`；
- H-C-01：`77` 个主干头文件，`A=6 / B=41 / C=30`；
- H-C-02：`41` 个 B 桶迁移单元完整覆盖。

## 5. 边界与后续

- `apps/slicer_debug_ui/**` 未修改；
- SPI v1、11 个导出、15 项能力和生产协议未修改；
- H-C-03 关闭 HOSTFLOW 本地交付，不替代打印侧代码移植；
- 打印侧 ACK、目标设备、目标 RIP 和物理打印仍为 `PENDING / DEFERRED`。
