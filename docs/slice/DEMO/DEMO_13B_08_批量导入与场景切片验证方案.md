# DEMO 13B-08 批量导入与场景切片验证方案

> 文档版本：v1.2
> 文档状态：VERIFIED
> 日期：2026-07-28
> 批准记录：2026-07-28，验证合同随 13B-08 原子任务执行

## 1. 验证目标

证明 Qt 工作台能够一次导入多个真实模型，并从当前场景显式执行联合切片，而不是退回单模型文件
对话框或旧配置。

## 2. Fixture

优先使用仓库真实模型：

```text
model/obj 中已通过 Stage 13 功能矩阵或模型资产预检的 OBJ；
一个 Texture2D 3MF；
一个故意损坏/不支持的模型副本；
1/3/11/12/22 个实例场景。
```

正式设备 buildVolume 未提供时，自动化仅使用明确标记的 functional fixture 幅面，结果不得写
`productionReady=true`。

## 3. 用例

### Case 13B-08-01 三模型批量导入

```text
一次选择三个模型；
确认只出现一次文件对话框；
确认三个实例依次加载；
确认同一俯视画布显示三者及纹理；
确认批次结束后只执行一次自动排版；
确认摘要 selected=3/imported=3/failed=0。
```

### Case 13B-08-02 容量阻断

场景已有 21 个实例时选择 2 个文件，预期：

```text
SCENE_BATCH_IMPORT_CAPACITY_EXCEEDED；
场景仍为 21 个实例；
没有静默只导入一个。
```

### Case 13B-08-03 部分失败

同批选择两个正常模型和一个坏模型，预期两个成功、一个失败，失败路径和错误码可见，后续文件仍被
处理，场景没有整体回滚。

### Case 13B-08-04 当前场景主动作

```text
三模型导入后不再打开文件对话框；
点击“切片当前场景”；
自动生成 scene_config.effective.json；
执行场景预检；
启动显式 scene CLI route；
成功后自动加载一个 package。
```

### Case 13B-08-05 Stale 与阻断

覆盖：

```text
碰撞；
越界；
一个实例预检 blocked；
切片期间修改 scene revision；
未准入 Global；
缺 buildVolume；
用户取消。
```

所有失败均不得生成伪成功 Package 或静默回退。

### Case 13B-08-06 输出合同

验证：

```text
一个 scene package；
每层一个 RGBWSV TIFF；
scene report 实例数为 3；
manifest scene hash 与冻结快照一致；
p0.rgbwsv.2 / uint8 / black_is_print；
RIP strict PASS。
```

## 4. 自动化入口

实际入口：

```powershell
ctest --test-dir build -C Debug -R "scene_batch_import|multi_model_production_service|scene_slice_route" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-batch-import-three
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-slice-current --package <output>
.\scripts\run_13b_08_scene_workflow.ps1 -BuildDir build -Config Release
.\scripts\run_ci_quick.ps1
```

统一矩阵入口：

```powershell
.\scripts\run_13b_08_scene_workflow.ps1 -BuildDir build -Config Debug
.\scripts\run_13b_08_scene_workflow.ps1 -BuildDir build -Config Release
```

2026-07-28 已在 Debug/Release 实际通过；设备输入未关闭，结论为 FUNCTIONAL PASS。

## 5. 通过标准

```text
批量导入正向/容量/部分失败/取消全部通过；
主按钮始终可见且状态可解释；
当前场景联合切片不打开单模型对话框；
一个场景产生一个严格可读 Package；
旧单模型和现有 Stage 13B 矩阵不回归；
UI Smoke、Debug full、Release targeted、Quick CI 通过。
```
