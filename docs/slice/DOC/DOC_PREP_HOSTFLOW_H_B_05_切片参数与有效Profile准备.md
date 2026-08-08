# DOC PREP HOSTFLOW H-B-05 切片参数与有效 Profile 准备

> 状态：**IMPLEMENTATION COMPLETE / GATE PASS**
> 日期：2026-08-08
> 任务：HOSTFLOW H-B-05
> 范围：宿主切片参数、设备构建体积、有效 Profile 预览和本地校验。

## 1. 准备结论

H-B-05 的产品边界、公开合同、代码落点和验证入口已完成审计，可以进入实现。该任务只在
`slicer_ui_host_sim` 参考宿主内建立可编辑参数草稿，不提交切片作业；H-B-06 才负责
`slice.rgbwsv` 的提交、进度、取消和结果生命周期。

冻结结论如下：

1. Profile 目录继续归宿主，沿用 HQ-08-A，不读取内部 `slicer_scenarios.json`。
2. `buildVolume` 归宿主设备 Profile；模块不得写死、推断或回填设备默认值。
3. H-B-05 使用既有 `profile` DTO 字段，不新增能力、导出或公开 schema。
4. 有效 Profile 必须带自洽 `profileHash`；哈希算法与现有切片入口一致。
5. Profile、DPI、层厚、输出目录、材料策略和 buildVolume 的编辑均为宿主本地操作，
   不触发 DLL 调用。
6. 场景首次创建后，`resolvedProfileId` 与 `buildVolume` 已成为场景权威上下文；本卡禁止
   静默覆盖。若用户修改这两类值，必须提示新建场景后生效并阻止错误预览进入 H-B-06。

## 2. 参数合同

| 参数 | 默认值 | 所有者 | 校验 |
|---|---:|---|---|
| Profile | H-B-04 当前可用项 | 宿主 | 必须存在于宿主目录且通过模块能力求交 |
| X DPI | 635 | 宿主设备/工艺 | 72..2400 |
| Y DPI | 600 | 宿主设备/工艺 | 72..2400 |
| 层厚 | 0.038 mm | 宿主工艺 | 大于 0 且不超过 10 mm |
| 输出目录 | 用户文档下的参考输出目录 | 宿主 | 绝对路径、不得为磁盘根目录、父目录可写 |
| 模型材料 | RGB / W / V 三种实体策略 | 宿主工艺 | 必须映射到既有 `modelMaterial` 字段 |
| buildVolume | 230 × 100 × 60 mm | 宿主设备 | 三轴均大于 0；origin/axes 使用冻结语义 |

材料映射保持 RGBWSV 和 `black_is_print` 不变：

```text
RGB 实体：materialChannel=RGB, rgb=[0,0,0], W=255, V=255
白墨实体：materialChannel=W,   rgb=[255,255,255], W=0, V=255
光油实体：materialChannel=V,   rgb=[255,255,255], W=255, V=0
```

上述是参考宿主 H-B-05 的最小材料策略集合，不替代主产品的完整材料策略编辑器。

## 3. 有效 Profile 与预览边界

H-B-05 生成的预览必须是未来 H-B-06 原样提交的 `profile` 对象，而不是另造一套宿主配置
schema。生成流程为：

```text
宿主 Profile + 本地参数 + 当前场景参考模型
  -> 规范 Profile（移除 profileHash）
  -> SHA-256
  -> 写回 profileHash
  -> JSON 预览与本地校验
```

模型尚未导入时，参数控件可编辑，但有效 Profile 预览必须明确显示“等待导入模型”，不能伪造
`modelPath`。输出目录只做可写性预检，不在 H-B-05 创建生产包或 staging 目录。

## 4. 场景上下文规则

参考宿主当前在首次 `addInstance` 时创建隐式场景。H-B-05 必须把当时的宿主 Profile 与
buildVolume 注入 `sceneContext`，替换旧的硬编码参考上下文：

```text
未创建场景：Profile/buildVolume 可编辑，并作为 pending sceneContext
已创建场景：Profile/buildVolume 与场景绑定；同值编辑允许，异值编辑 fail-closed
DPI/层厚/输出目录/材料：不改变场景几何身份，可继续编辑并重建有效 Profile
```

切换 Profile 或设备体积后要处理既有场景，属于 H-B-08 的会话恢复/重建职责，本卡不自动销毁
场景，也不绕过 revision/scene authority。

## 5. 文件所有权

计划修改范围：

```text
apps/slicer_host_sim/HostRequestBuilder.*
apps/slicer_ui_host_sim/HostSliceSettings.*
apps/slicer_ui_host_sim/HostSliceSettingsPanel.*
apps/slicer_ui_host_sim/HostModelImportWorkflow.*
apps/slicer_ui_host_sim/HostMainWindow*.cpp
apps/slicer_ui_host_sim/CMakeLists.txt
tests/hostflow/HostSliceSettingsTests.cpp
```

主干 `apps/slicer_debug_ui/**` 只用于 A/B 对照，本任务不得修改。参考宿主不得 include 或链接
`slicer_core`。

## 6. 验证门禁

H-B-05 至少覆盖：

1. 635/600 DPI、0.038 mm、RGB/W/V 映射和 Profile hash 确定性。
2. DPI、层厚、输出路径、材料策略和 buildVolume 负例 fail-closed。
3. 参数编辑和 Profile 选择期间 DLL 调用数保持 0。
4. 首次导入时 `sceneContext` 使用宿主选定 Profile/buildVolume。
5. 场景创建后修改 Profile/buildVolume 被明确拒绝，不改变 revision。
6. Debug/Release `hostflow_hb01..05` 联合回归。
7. 参考宿主边界、缺失模块、自检和源文件 500 行守卫保持通过。

## 7. 停止条件

出现下列任一情况必须停止实现并建立受控修订：

- 需要第 16 项能力或第 12 个导出；
- 需要修改 SPI v1、RGBWSV/TIFF 或 `slicesoft.slice_profile.1`；
- 需要把 Profile 目录或 buildVolume 所有权下沉给模块；
- 需要读取内部场景配置或链接 `slicer_core`；
- 需要在 H-B-05 内提交、轮询或取消真实切片作业。

## 8. Revision History

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-08 | v1.1 | 完成宿主参数面板、有效 Profile、自有 buildVolume 注入、场景绑定保护和 Debug/Release/A-B Gate。 |
| 2026-08-08 | v1.0 | 完成参数、场景上下文、有效 Profile、文件所有权、测试矩阵与停止条件准备。 |
