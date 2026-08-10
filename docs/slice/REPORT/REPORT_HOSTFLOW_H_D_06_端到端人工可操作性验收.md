# REPORT HOSTFLOW H-D-06 端到端人工可操作性验收

> 状态：**LOCAL PASS / RELEASE MANUAL + DEBUG/RELEASE UI SMOKE**
> 日期：2026-08-10
> 原子任务：H-D-06

## 1. 验收结论

参考宿主已在 Release 配置中人工走通“导入、显示、选择、变换、排版、切片、生产包结果”七步。
Debug 与 Release 配置分别通过 7 组宿主 UI self-test。H-C-03 A/B 矩阵已增加“显示”维度。

本结论只证明切片侧参考宿主的本地可操作性，不代表打印侧、目标 RIP、洁净机或物理打印验收通过。

## 2. 验收环境

| 项目 | 实际值 |
|---|---|
| Release 宿主 | `build-slicesoft/main/apps/slicer_ui_host_sim/Release/slicer_ui_host_sim.exe` |
| Release 模块 | `build-slicesoft/main/Release/slicer_module.dll` |
| Profile | `host-reference-default` / 彩色纹理生产 |
| 资产 | 真实带纹理 OBJ 甲片；最终成功场景为 10 个实例 |
| 证据分辨率 | 72 x 72 DPI，仅用于压缩人工证据包体，不修改产品默认值 |
| 最终包 | 145 层，595 x 75 px，`p0.rgbwsv.2`，RGBWSV，`black_is_print` |

## 3. 七步人工证据

| 步骤 | 结果 | 证据 |
|---|---|---|
| 1. 启动并加载工作区 | PASS | [01_release_workspace.png](assets/hostflow_hd06/01_release_workspace.png) |
| 2. 导入并显示真实纹理模型 | PASS | [02_import_display.png](assets/hostflow_hd06/02_import_display.png) |
| 3. 切换 3D 显示并检查纹理 | PASS | [03_three_d_display.png](assets/hostflow_hd06/03_three_d_display.png) |
| 4. 选择实例并提交变换 | PASS，revision 2 → 3 | [04_transform_commit.png](assets/hostflow_hd06/04_transform_commit.png) |
| 5. 执行规则排版 | PASS，revision 3 → 4 | [05_layout_commit.png](assets/hostflow_hd06/05_layout_commit.png) |
| 6. 提交切片并到达完成终态 | PASS，`PM-SLICER-OK-0000`，约 10.3 s | [06_slice_complete.png](assets/hostflow_hd06/06_slice_complete.png) |
| 7. 查看严格校验后的生产包、层预览、通道曲线和报告 | PASS | [07_package_result.png](assets/hostflow_hd06/07_package_result.png) |

“打开包目录”按钮已在结果页调用。应用退出后临时包由宿主生命周期清理，因此未把退出后的外部
`rip_reader_test` 记为 PASS；本次可确认的是应用内生产包严格校验通过。

## 4. 失败路径与恢复

人工流程同时确认两条 fail-closed 路径：

1. 初次排版超出宿主 `buildVolume` 时返回 `PM-SLICER-LAYOUT-0021` / `SCENE_INSTANCE_OUT_OF_RANGE`；通过将全部实例向 +X/+Y 移动 5 mm 后重新准入。
2. 输出父目录不存在时返回 `PM-SLICER-OUTPUT-0050 failed to create package lease`；创建宿主拥有的输出父目录后重试成功。

两条失败均未静默回退，也未修改切片模块默认语义。

## 5. 自动验证

Debug 与 Release 均实际完成以下 7 项：

```text
--self-test
--hostflow-import-ui-self-test
--hostflow-profile-ui-self-test
--hostflow-settings-ui-self-test
--hostflow-job-ui-self-test
--hostflow-result-ui-self-test
--hostflow-workspace-ui-self-test
```

其中工作区 smoke 暴露了一个测试常量漂移：持久化 schema 已升级到 v4，但自测仍期待 v1。
本任务把期待值改为 `HostWorkspaceState::SchemaVersion() == 4`，未改变运行时 schema。

## 6. 验收边界

- Release 完成了人工七步；Debug 只完成同构 UI smoke，未重复人工七步。
- 未修改 SPI v1、11 个导出、15 项能力、ViewData、Worker 或 RGBWSV 生产协议。
- 未修改 `apps/slicer_debug_ui/**`。
- 未取得打印侧 ACK、目标 RIP、洁净机和物理打印证据。
