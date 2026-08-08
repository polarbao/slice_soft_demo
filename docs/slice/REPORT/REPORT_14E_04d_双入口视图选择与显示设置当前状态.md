# REPORT_14E-04d 双入口视图选择与显示设置当前状态

> 状态：COMPLETE（合同与门禁）／⚠️ **§5 的接线延期决定已于 2026-08-08 作废**
> 日期：2026-08-07 ｜ 修订：2026-08-08
> 适用分支：`feature/14-slicer-capability-package`

> 🔴 **2026-08-08 复核更正 —— 读本文前必看**
>
> 本文第 2、3 节的结论**仅对离屏渲染与合同门禁成立**。
> `stage14e04d_view_switch_tests` 全部是 offscreen 验证，**可见 UI 上没有任何像素输出**：
> `ViewWorkspaceWidget.cpp:13-27` 的两个画布是 `QLabel` + 静态文字，
> `HostMainWindow.cpp` 对 `m_workspace` 只调用 `SetMode`/`ShowViewError`/`SetSelectedInstances`。
>
> 因此 §5 中「真实场景载入与渲染策略接线由**打印侧集成时**按本任务类边界复用」
> 这一延期决定**作废**：接线归切片侧，已立为 `TASKS_HOSTFLOW` 的 **H-D-01/02/04**。
>
> 教训：**「离屏门禁全绿」不等于「界面上看得见」。** 涉及可见 UI 的卡片，
> 验收必须把 app 真的跑起来并归档截图。

## 1. 任务目标

14E-04d 在 14E-04c 的纹理三维渲染基础上，闭合打印宿主参考实现的俯视/3D 双入口、
默认视图与 3D 投影持久化、构建幅面网格以及白色纹理显示辅助。所有切换和显示辅助必须
保持宿主本地，不得改变场景、实例变换、作业、纹理像素、生产 TIFF 或模块 ABI。

## 2. 实现结果

- 新增 `ViewModeSwitch`，切换操作只修改宿主显示模式，不持有 `ModuleClient`，因此不会
  在切换期间触发 DLL 调用。
- 新增 `ViewWorkspaceWidget`，中央工作区提供“俯视 / 3D”分段入口和显式错误区域；
  `HostMainWindow` 增加“工作区 / 设置 / 模块诊断”三页结构。
- 新增 `ViewPresentationSettings`，在 `session_config.json` 的 `viewPresentation` 节点
  持久化 `defaultViewMode=top|three_d` 与
  `threeDProjection=orthographic|perspective`，并使用原子文件替换保存。
- top 渲染新增来源于权威 `buildVolume.widthMm/heightMm` 的平台范围、1 mm/10 mm
  自适应网格、非纯白平台、深色轮廓和未解析坐标系提示条；three_d 网格同步按
  4 px/mm 门槛隐藏小格。
- 当前公开 snapshot 只携带 buildVolume 的宽、高和可选 Z 限高，不携带原点与轴向；
  因此参考宿主按冻结合同显示产品坐标回退诊断，不把回退值冒充设备生产真值。
- missing texture 继续显式 fail-closed，不会静默显示灰模；无纹理且未声明贴图的模型仍可
  按 `textureStatus=not_provided` 正常显示，避免把合法单材料模型误判为纹理失败。

## 3. UI-M9..13 闭合结果

| 门禁 | 结果 |
|---|---|
| UI-M9 top 真实纹理与白色对比 | PASS |
| UI-M10 three_d UV/材质/纹理 | PASS |
| UI-M11 切换保持 scene/revision/cache 且 DLL 调用为 0 | PASS |
| UI-M12 缺失纹理显式失败 | PASS |
| UI-M13 默认视图与投影重启恢复 | PASS |
| 网格范围来自 buildVolume，1 mm/10 mm 自适应 | PASS |
| 显示辅助不修改纹理与生产输出 | PASS（结构边界） |

## 4. 验证证据

已实际执行：

```text
cmake --build --preset slicesoft-debug --target slicer_ui_host_sim \
  stage14e04_top_view_tests stage14e04c_three_d_tests \
  stage14e04d_view_switch_tests
ctest --test-dir build-slicesoft/main -C Debug \
  -R "slicer_stage14e0(2|3|4)" --output-on-failure
结果：9/9 PASS

cmake --build --preset slicesoft-release --target slicer_ui_host_sim \
  stage14e04_top_view_tests stage14e04c_three_d_tests \
  stage14e04d_view_switch_tests
ctest --test-dir build-slicesoft/main -C Release \
  -R "slicer_stage14e0(2|3|4)" --output-on-failure
结果：9/9 PASS
```

`stage14e04d_view_switch_tests` 还覆盖：session config round-trip、实际 top/three_d
纹理显示、100 次本地切换零模块调用、缓存身份保持、白纹理与深色轮廓可辨、缺纹理负例，
以及 offscreen Qt 分段控件即时切换。

## 5. 边界与下一步

本任务没有修改 `PM_SPI_VERSION=1`、11 个 `pm_*` 导出、15 项能力、
`p0.rgbwsv.2`、RGBWSV/8-bit/`black_is_print`、切片采样或生产 Package。主干
`slicer_debug_ui` 仍未迁移，参考宿主的中央画布目前是双视图交互入口。

> ⛔ **原文此处写「真实场景载入与渲染策略接线由打印侧集成时按本任务类边界复用」，
> 该决定已于 2026-08-08 作废。** 接线归切片侧，见 `TASKS_HOSTFLOW` H-D 组。

下一张任务卡为 14E-05：拆分主干 UI 既有大文件并关闭源码行数白名单；该任务与
参考宿主双视图合同解耦，应单独提交和验证。
