# DOC_PREP_HOSTFLOW H-B-08 设置持久化与工作区状态准备

> 状态：**IMPLEMENTATION COMPLETE / VALIDATION PASS**
> 日期：2026-08-08
> 任务：`H-B-08` 设置持久化与工作区状态
> 前置：`H-B-05 COMPLETE`；当前 H-B-01..07 均已完成

## 1. 目标

参考宿主使用宿主自有、带版本的用户设置完成：

1. 保存并恢复窗口 geometry、工作区分栏、当前工作区页和右侧业务页；
2. 保存并恢复可编辑的 Profile、DPI、层厚、输出目录、实体材料和设备 buildVolume；
3. 无配置、旧 schema、损坏值或屏幕外窗口时回退安全默认布局与生产默认参数；
4. smoke/self-test 进程不得读写真实用户设置；
5. 设置恢复不调用切片 DLL，不进入切片 JSON，也不改变生产协议。

## 2. 会话恢复边界

本卡恢复的是宿主操作偏好和工作区，不恢复运行时 `sceneHandle`、Worker 作业句柄、模型资源句柄
或临时预览缓存。上述对象只在当前 `pm_module_t` 生命周期内有效，持久化后重放会形成悬空身份。
重启后场景保持为空，用户重新导入模型；首次导入时使用已恢复的 Profile/buildVolume 建立新的
权威场景。这样闭合 H-B-05 中“Profile/buildVolume 必须在建场景前确定”的约束，而不绕过
scene revision 或 authority。

## 3. 冻结状态合同

宿主设置 schema 为 `hostflow.workspace.1`，写入用户级 `QSettings`，包括：

- `geometry`、主 `QSplitter` sizes；
- 当前顶层 workspace tab、当前业务 inspector tab；
- `profileId`；
- `dpiX/dpiY/layerThicknessMm/outputDirectory/materialStrategy`；
- `buildVolume.widthMm/heightMm/zLimitMm/origin/xDirection/yDirection`。

不保存：模型路径列表、scene snapshot、module path、package 临时预览、作业状态、报告内容。
视图表现的 `top/three_d` 与投影方式继续由既有 `ViewPresentationSettings` 管理，避免双写。

## 4. 实现分层与所有权

```text
HostMainWindow
  -> HostWorkspaceState（QSettings schema/校验/安全回退）
  -> HostSliceSettingsPanel::SetPersistentSettings
  -> ReferenceHostProfileCatalog 可用 Profile 求交后恢复 profileId
```

允许修改：

- `apps/slicer_ui_host_sim/**`
- `tests/hostflow/**`
- `apps/slicer_ui_host_sim/CMakeLists.txt`
- HOSTFLOW 任务清单、执行指令和当前状态报告

禁止修改：

- `apps/slicer_debug_ui/**`
- `src/slicer_core/**`、`src/slicer_module/**`
- `contracts/**`、RGBWSV/TIFF/manifest
- `.specstory/**`

## 5. 校验规则

- schema 必须精确匹配；旧版本整体废弃，不进行字段猜测迁移；
- DPI 范围 72..2400，层厚 0.001..10 mm；
- buildVolume 三轴为正且不超过 10000 mm，轴向合同必须为冻结值；
- materialStrategy 只接受 `rgb_solid/white_solid/varnish_solid`；
- tab 索引必须在当前页数内，splitter sizes 数量和总尺寸必须有效；
- geometry 恢复后必须与可用屏幕相交，否则回退 1080×720 安全布局；
- 已保存 Profile 若不再通过能力求交，使用首个可用 Profile，不伪造不可用选择。

## 6. 验收

1. 临时 INI 完成保存、恢复和字段逐项对账；
2. 损坏 schema、非法参数和非法布局 fail-safe，不崩溃；
3. UI smoke 能定位顶层 tabs、业务 tabs、splitter 与生产默认参数；
4. Debug/Release H-B-01..08 联合门禁通过；
5. Qt 宿主边界、缺失模块、源码尺寸守卫和 `git diff --check` 通过。

## 7. 实现与验证结论

依赖、所有权、恢复边界和 fail-safe 规则均已明确，不需要新增能力、导出或 DTO。
`H-B-08` 已按本合同完成：

- `HostWorkspaceState` 提供 `hostflow.workspace.1` 保存、恢复、校验与安全重置；
- `HostMainWindow` 仅在正常交互进程恢复和保存用户偏好，自检进程保持隔离；
- `HostSliceSettingsPanel` 恢复宿主参数但不恢复模型或运行时资源身份；
- 已保存 Profile 在模块能力求交后选择，不可用时回退首个可用项。

实际验证：

- Debug/Release `^hostflow_h[ab]` 各 18/18 PASS；
- Debug/Release Qt 宿主边界、缺失模块、smoke 与源码尺寸守卫各 4/4 PASS；
- `ValidateSourceSizeGuard.py --base-ref HEAD` PASS；
- `git diff --check` PASS。
