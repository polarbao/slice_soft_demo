# HOSTFLOW H-C-03 主干与参考宿主 A/B 对照准备

> 状态：**PREPARATION GATE PASS**
> 日期：2026-08-08
> 原子任务：H-C-03
> 范围：只验证主干 `slicer_debug_ui` 与参考宿主 `slicer_ui_host_sim` 的业务语义；不修改主干 UI、SPI、生产协议或 TIFF。

## 1. 目标与边界

H-C-03 回答的是“参考宿主是否覆盖了主干 UI 的核心切片作业流，以及差异是否可解释”，而不是要求两个程序拥有相同源码、相同 Profile id 或逐像素相同界面。

本任务固定以下边界：

- 主干 UI 是切片专用工作台，也是功能规格来源，只读不改。
- 参考宿主只经冻结的 11 个公开导出和 15 项能力访问切片模块。
- Profile 目录归宿主。主干 `textured_nail_rgb_only_lower_support` 与参考宿主 `host-reference-default` 只做语义映射，不伪造同一身份。
- `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 和 TIFF 生产值不变。
- 不把切片专有的 OpenVDB、修复、原始配置和高级诊断工具强行移植到打印宿主。
- 打印侧 ACK 仍是 `PENDING / DEFERRED`；本地 A/B PASS 不代表外部集成 PASS。

## 2. 规范化对照输入

| 项目 | 固定值 |
|---|---|
| 模型 | `samples/models/openvdb/surface_shell_cube_no_uv.obj` |
| 主干 Profile | `textured_nail_rgb_only_lower_support` |
| 参考宿主 Profile | `host-reference-default` |
| Profile 语义 | Legacy、全实体 RGB、RGBWSV 生产包、严格校验 |
| X/Y DPI | `635 / 600` |
| 层厚 | `0.038 mm` |
| 实体材料 | `rgb_solid` |
| buildVolume | `230 x 100 x 60 mm`，`lower_left`，X/Y 正向 |

模型几何和核心参数相同；Profile id 不相同是宿主所有权设计的预期结果。

## 3. 对照维度与判定

机器矩阵必须覆盖八个维度：

1. `import_preflight`：模型导入、格式与快速预检；
2. `model_list_selection`：模型列表、扩展选择、全选和删除；
3. `transform_layout`：移动、旋转、缩放、镜像和规则排版；
4. `profile_settings`：Profile 能力求交、DPI、层厚、材料和 buildVolume；
5. `slice_job_cancel`：作业提交、进度、取消和终态；
6. `package_result`：包校验、层预览、通道图和报告；
7. `workspace_persistence`：用户偏好与布局恢复，不恢复运行时身份；
8. `diagnostics_scope`：切片专有诊断和宿主核心流程的范围差异。

每条差异只能使用以下结论：

- `equivalent`：用户目标与安全语义等价，实现路径允许不同；
- `known_trim`：参考宿主按已批准范围裁剪，但存在明确替代流程；
- `slicer_only`：功能属于切片专用工作台，不进入打印宿主。

禁止使用“视觉完全一致”“配置 id 完全一致”或“打印侧已验收”作为通过条件。

## 4. 自动化证据

`scripts/RunHostflowAbComparison.ps1` 负责：

- 运行主干五个业务 smoke：模型列表、规则排版、生产模式、切片参数和生效配置；
- 运行参考宿主六个 UI self-test：导入、Profile、设置、作业、结果和工作区；
- 运行 `^hostflow_h[ab]` Debug/Release 联合 CTest；
- 调用 `ValidateHostflowAbMatrix.py` 校验对照矩阵闭合。

`scripts/ValidateHostflowAbMatrix.py` 负责检查 schema、规范化输入、八维覆盖、唯一 id、合法结论和证据完整性。

## 5. 准备结论

H-B-01..08、H-C-01/02 已完成，规范化输入、差异分类、证据命令和停止边界均明确。
`H-C-03_PREPARATION_GATE=PASS`，可以进入实现与验证。
