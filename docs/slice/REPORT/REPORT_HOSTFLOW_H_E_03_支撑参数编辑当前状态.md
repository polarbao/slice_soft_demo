# REPORT HOSTFLOW H-E-03 支撑参数编辑当前状态

> 状态：**COMPLETE / E1 BATCH REVIEW PENDING**
> 日期：2026-08-10
> 任务：`H-E-03`
> 范围：参考宿主支撑 Profile 编辑、有效配置、自哈希与草稿持久化

## 1. 当前结论

参考宿主已建立第一段可扩展的宿主 Profile 编辑框架。支撑参数不读取模块内部样例，
也不由 UI 拼接 JSON，而是沿单一数据链路进入未来切片请求：

```text
HostSupportSettingsPanel
  → hostsupportsettings / hostslicesettings
  → HostEffectiveProfileBuilder（范围与组合校验）
  → HostBuildEffectiveProfile（C request builder）
  → profileHash
  → slice.rgbwsv
```

## 2. 已实现范围

- 新增可折叠“宿主 Profile 支撑段”，提供启用、模式、XY 外扩和最小面积；
- 提供内部闭合镂空开关/最小面积、最大支撑投影铺底开关/层数；
- 默认下表面投影显式写 `placement=lower`；高级 mode 省略 placement，避免生产解析器
  以显式 placement 覆盖 mode。UI 不开放 upper/both；`value=0`、`fillRule=all_internal_voids`、
  `source=max_support_footprint` 保持冻结；参考宿主的生产 Legacy 入口固定写入
  `layerPlacement=prepend_below_model`，使铺底成为模型下方新增的物理层；
- 关闭支撑时强制 `mode=none`、关闭内部镂空/铺底，并同步
  `materialProcessProfile.support.expected=false`；
- 支撑任一字段变化都会改变有效 Profile 自哈希；不支持组合和越界值 fail-closed；
- 工作区 schema 升至 2，持久化宿主支撑草稿，不持久化 scene/job/cache 身份。

生产解析器对 `baseProjection.layerCount` 的实际上限为 `1000`，本任务已修正准备文档
中原 `10000` 的笔误，UI、C++ 校验、C builder 和持久化门禁使用同一上限。

## 3. 验证证据

2026-08-10 已完成 Debug/Release 构建，并分别执行：

```powershell
ctest --test-dir build -C Debug -R "^(hostflow_he03_support_settings|hostflow_he03_support_persistence|hostflow_hb05_slice_settings|hostflow_hb08_workspace_state|hostflow_hb06_slice_job|slicer_stage14e02_qt_host_boundary_test|slicer_source_size_guard_self_test)$" --output-on-failure
ctest --test-dir build -C Release -R "^(hostflow_he03_support_settings|hostflow_he03_support_persistence|hostflow_hb05_slice_settings|hostflow_hb08_workspace_state|hostflow_hb06_slice_job|slicer_stage14e02_qt_host_boundary_test|slicer_source_size_guard_self_test)$" --output-on-failure
```

结果：Debug `7/7 PASS`，Release `7/7 PASS`。Release 专项输出：

```text
HOSTFLOW_HE03_PASS profile=host-reference-default dpi=635x600 layer=0.038 support=editable
HOSTFLOW_HE03_PERSISTENCE_PASS schema=2 runtimeHandles=persisted:false
```

### 3.1 2026-08-17 生产铺底链路更正

用户生产包证据确认，UI 已写入 `baseProjection.enabled=true` 和
`layerCount=30`，但宿主 Profile builder 仍写入历史兼容值
`overlay_existing`，因此没有在模型下方新增 30 个物理 TIFF 层。当前已改为
`prepend_below_model`，并增加端到端回归：开启铺底后总层数必须增加 30，
且前 30 张 TIFF 必须只包含非空 S 通道。Release 相关宿主回归 `7/7 PASS`，
13G 铺底 Package/RIP 专项 PASS（`46 = 16 + 30` 层）。

## 4. 边界与下一步

- 未新增或修改 SPI、能力、导出函数、RGBWSV、TIFF、RIP 与 ViewData 合同；
- 未修改 `apps/slicer_debug_ui/**`；
- 上表面支撑、材料工艺和生产纹理字段不在 E1 范围；
- H-E-01 与 H-E-03 均完成，必须先执行 E1 批次复核，再决定 E2 的
  H-E-04/H-E-05 是否沿用当前编辑框架。
