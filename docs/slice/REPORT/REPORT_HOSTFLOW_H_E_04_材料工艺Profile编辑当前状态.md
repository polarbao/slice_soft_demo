# REPORT HOSTFLOW H-E-04 材料工艺 Profile 编辑当前状态

> 任务：`H-E-04`
> 状态：**COMPLETE**
> 日期：2026-08-10

## 1. 实现结果

参考宿主已增加独立的材料工艺编辑器，并将材料策略、角色映射和工艺参数统一接入：

```text
HostMaterialSettingsPanel
  -> hostslicesettings.materialprocess
  -> HostEffectiveProfileBuilder
  -> HostBuildMaterialProfileFragments
  -> canonical Profile / compact Profile / profileHash
```

支持六种策略：RGB、RGB+W、RGB+V、RGB+W+V、单 W、单 V。角色映射支持白墨、
光油名称规则、默认角色、输入支撑材料准入以及白墨/光油工艺参数。

## 2. 持久化与边界

- 工作区 schema 提升到 3，保存宿主材料草稿，不保存运行时 scene/job/cache 身份；
- 参数编辑和持久化不调用 DLL；
- 生成结果进入有效 Profile 自哈希，场景建立后的异值仍由既有绑定逻辑 fail-closed；
- 不修改 SPI v1、11 个导出、15 项能力、ViewData、RGBWSV、TIFF 或 RIP；
- 不修改 `apps/slicer_debug_ui/**`。

## 3. 验证

Debug 与 Release 均构建：

```text
hostflow_hb05_slice_settings_tests
hostflow_hb06_slice_job_tests
hostflow_hb08_workspace_state_tests
slicer_ui_host_sim
```

Debug/Release 专项联合门禁均为 `9/9 PASS`，覆盖材料 Profile、工作区持久化、支撑回归、
切片作业、Qt 宿主边界和源码尺寸守卫。

## 4. 后续

E1 批次复核已通过，H-E-04 完成。下一张卡为 H-E-05 生产纹理设置；E2 复核完成前，
H-E-02/H-E-06 不得提前进入实现。
