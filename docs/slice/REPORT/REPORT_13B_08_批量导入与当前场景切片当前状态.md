# REPORT 13B-08 批量导入与当前场景切片当前状态

> 文档状态：FUNCTIONAL COMPLETE / PRODUCTION INPUT OPEN
> 日期：2026-07-28
> 完成范围：13B-08-01..04
> 下一任务：13D-01 顶部作业栏

## 1. 阶段结论

13B-08 已关闭 Qt 工作台“只能逐个导入、当前场景不能直接切片”的作业流断点：

```text
一次选择多个模型
-> 串行导入并只排版一次
-> 调整当前 SceneDocument
-> 切片当前场景
-> 冻结 scene effective config
-> slicer_cli --scene-config
-> 一个 RGBWSV Package
-> RIP strict
-> 自动加载 TIFF 生产预览。
```

阶段结论：

```text
13B-08 功能开发：COMPLETE；
Debug/Release 作业流矩阵：PASS；
Qt 真实 OBJ + OBJ/MTL + Texture2D 3MF：PASS；
正式设备 production GO：INPUT_OPEN。
```

功能通过不替代设备输入。当前 Qt 场景动作使用显式标记的 fixture buildVolume，输出
`productionReady=false`。

## 2. 原子任务完成情况

| 任务 | 结果 | 主要证据 |
|---|---|---|
| 13B-08-01 批量导入队列 | COMPLETE | 多选、顺序队列、22 容量、部分失败、取消、单次排版 |
| 13B-08-02 场景生产服务与 CLI | COMPLETE | 无 Qt 服务、`--scene-config`、单 Package、稳定错误 |
| 13B-08-03 Qt 当前场景动作 | COMPLETE | 状态机、冻结快照、stale/cancel/no-fallback、自动回载 |
| 13B-08-04 真实作业流矩阵 | COMPLETE | 1/3/11/12/22、OBJ/3MF、RIP strict、Debug/Release |

## 3. 13B-08-04 新增实现

### 3.1 一键回归脚本

新增：

```text
scripts/run_13b_08_scene_workflow.ps1
```

脚本统一执行：

```text
13B-08 定向 CTest；
1/11/12/22 真实模型核心矩阵；
OBJ + Texture2D 3MF 正向；
容量、碰撞、越界、缺幅面、stale 负向；
批量导入、部分失败、当前场景、取消、stale、Global no-fallback UI Smoke；
三真实资产 Qt 当前场景切片；
所有正向 Package 的 RIP strict；
机器可读 scene_workflow_summary.json。
```

机器可读报告固定记录：

```text
资产路径、字节数和 SHA-256；
Profile、DPI、层厚、buildVolume 和 pipeline mode；
1/3/11/12/22 覆盖；
负向错误码；
Qt Package、sceneId/revision/hash；
生产协议和 production INPUT_OPEN。
```

### 3.2 真实 Qt 作业流 Smoke

新增 `scene-slice-real-assets`：

```text
model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj；
model/obj/yecan/3.obj；
samples/models/3mf/texture2d_checker_cube.3mf。
```

三者在一个 SceneDocument 中导入和排版，通过 Qt “切片当前场景”启动显式
`slicer_cli --scene-config`，产生一个 336 x 150 x 29 Package，并自动加载 29 个 TIFF 层。

该 Smoke 使用 127 x 127 DPI、0.20 mm 层厚的功能 Profile，避免把 UI 自动化冒充设备 SLA。

新增 `scene-batch-import-real-meigui`：

```text
model/obj/meigui_fudiao/02.obj；
model/obj/meigui_fudiao/03.obj；
model/obj/meigui_fudiao/04.obj。
```

该用例按 UI 同一串行批量导入控制器加载三个真实大 OBJ，要求 selected/imported=3、
failed/cancelled=0、场景实例数为 3，并在批次结束后完成一次自动排版。它验证真实资产导入稳定性，
不代表三个复杂浮雕已通过 Global strict 或设备生产准入。

### 3.3 3MF 稳定资源身份修复

真实 Qt 矩阵首次暴露了一个既有问题：

```text
SCENE_RESOURCE_UNRESOLVED:
scene model or adjacent resource hash mismatch
```

根因是 3MF 内部贴图每次导入会解压到当前配置的临时 cache 目录。旧资源 hash 把该物理缓存绝对
路径纳入身份，因此 Qt 导入和 CLI 回读同一个 3MF 时，贴图内容相同但缓存根不同，错误地被判为资源
变化。

新增共享 `ComputeSceneResourceHash()`：

```text
OBJ/外部纹理：继续记录稳定规范路径和内容 hash；
3MF 内部纹理：记录逻辑缓存文件名、texture_source 和内容 hash；
临时解压根目录不进入 3MF 资源身份；
贴图内容改变仍会改变 hash；
Qt Loader、场景生产服务、矩阵和单测使用同一实现。
```

服务错误同时增加 `modelId`、source hash 是否匹配和 expected/actual resource hash，便于后续定位。

## 4. 资产与格式矩阵

| 资产 | 类型 | SHA-256 | 用途 |
|---|---|---|---|
| `MF_Xiao_ma_Damuzhi_ty02.obj` | OBJ/MTL 纹理 | `4f2012e7...f8820ddc` | 主彩色真实模型 |
| `model/obj/yecan/3.obj` | OBJ | `a3a42100...6914363d` | 独立真实模型族 |
| `texture2d_checker_cube.3mf` | Texture2D 3MF | `d7ec3998...8445f2c57` | 3MF 贴图控制组 |

复杂浮雕 `aishen/meigui/titian` strict 覆盖仍为 `0/3`，没有被当作正向资产。

## 5. 规模与正向矩阵

Release 功能 Profile：

```text
Legacy；
127 x 127 DPI；
0.20 mm 层厚；
11 列 x 2 行；
列/行净距 20/30 mm；
fixture buildVolume；
productionReady=false。
```

| Case | 实例 | 格式 | Grid | Release 总耗时 | 结果 |
|---|---:|---|---|---:|---|
| 13B-M01 | 1 | OBJ | 61 x 117 x 28 | 0.63 s | PASS |
| 13B-UI-REAL-3 | 3 | OBJ/MTL + OBJ + 3MF | 336 x 150 x 29 | UI 作业流 | PASS |
| 13B-M11 | 11 | OBJ | 1661 x 117 x 28 | 4.69 s | PASS |
| 13B-M12 | 12 | OBJ | 1661 x 416 x 29 | 10.06 s | PASS |
| 13B-M22 | 22 | OBJ | 1661 x 416 x 29 | 12.54 s | PASS |
| 13B-M3F | 2 | OBJ + Texture2D 3MF | 176 x 117 x 28 | 1.11 s | PASS |

这些时间包括矩阵自身的 Package 写入和 RIP 校验，不是 core-only，也不是设备生产 SLA。

## 6. 负向矩阵

| 场景 | 稳定结果 |
|---|---|
| 23 实例/21+2 超容量 | `LAYOUT_INSTANCE_CAPACITY_EXCEEDED` |
| 两实例重叠 | `SCENE_INSTANCE_OVERLAP_BLOCKED` |
| 幅面越界 | `SCENE_INSTANCE_OUT_OF_RANGE` |
| 缺 buildVolume | `SCENE_BUILD_VOLUME_UNDEFINED` |
| scene revision 过期 | `SCENE_REVISION_STALE` |
| 同批坏模型 | 成功项保留，失败项 `SCENE_BATCH_IMPORT_ITEM_FAILED` |
| 切片期间取消 | `SCENE_SLICE_CANCELLED`，不回载旧 Package |
| 未准入 Global | `SCENE_SLICE_PIPELINE_MODE_NOT_ADMITTED`，不回退 Legacy |

负向用例不发布伪成功 Package。

## 7. 输出合同

核心矩阵和 Qt 真实作业流的所有正向 Package 均满足：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
one scene = one Package
one global layerIndex = one TIFF
multimodel_scene_report identity = frozen SceneDocument identity
RIP strict = PASS
```

## 8. 验证结果

实际执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_13b_08_scene_workflow.ps1 `
  -BuildDir build -Config Debug

powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_13b_08_scene_workflow.ps1 `
  -BuildDir build -Config Release
```

结果：

```text
Debug 定向 CTest：16/16 PASS；
Release 定向 CTest：16/16 PASS；
Debug/Release 正向规模和格式矩阵：PASS；
Debug/Release 负向矩阵：按预期 BLOCK；
七个场景作业流 UI Smoke：PASS；
核心矩阵和 Qt 真实作业流全部正向 Package：RIP strict PASS；
Debug 全量构建：PASS；
Debug 全量 CTest：81/81 PASS；
Quick CI：PASS；
git diff --check：PASS。
```

本机证据：

```text
output/benchmarks/13b_08/debug/scene_workflow_summary.json
output/benchmarks/13b_08/release/scene_workflow_summary.json
output/benchmarks/13b_08/<config>/real_model_matrix/real_model_matrix.json
output/benchmarks/13b_08/<config>/qt_real_assets_workflow.json
```

`output/` 为本机复测证据，不纳入 Git。

### 8.1 2026-07-28 Release 批量导入崩溃复测

本轮复现到部署版执行 `scene-batch-import-three` 时以 `0xc0000005` 退出。Windows dump 显示异常发生
在 `load_slice_config` 相关 `std::string` / `std::vector<std::string>` 生命周期代码；旧
`config.cpp.obj` 时间为 17:11，源文件时间为 17:17，而运行包在 17:20 发布，证明一次长时间
Release 构建期间源文件继续变化，旧 ABI 对象被链接并部署。

修正：

```text
PrepareSliceSoftRuntime.ps1 的指纹覆盖 C/C++ 源文件、头文件和 CMakeLists；
构建完成后重新计算输入指纹；
构建期间输入发生变化时拒绝写 stamp 和部署 runtime；
新增真实 meigui 02/03/04 批量导入 Smoke。
```

实际复测：

```text
Release -ForceClean 构建与部署：PASS；
Release startup：PASS；
Release scene-batch-import-three：PASS；
Release scene-batch-import-real-meigui：PASS；
Debug scene-batch-import-three：PASS；
Debug scene-batch-import-real-meigui：PASS；
Debug CTest：82/82 PASS；
Quick CI：PASS。
```

因此，批量导入功能缺口已关闭；原崩溃不是 `meigui` 几何本身触发，而是运行包混入了构建期间产生
的 ABI 不一致对象。

## 9. 生产 Gate

仍未关闭：

```text
正式设备 buildVolume；
设备原点和 X/Y 轴方向；
22 实例生产性能预算；
Global/OpenVDB 多模型生产准入；
aishen/meigui/titian 复杂浮雕 strict 正向覆盖；
跨实例联合支撑和 mixed-profile。
```

因此本阶段只能声明 `FUNCTIONAL COMPLETE / PRODUCTION INPUT OPEN`，不得声明正式设备
`PRODUCTION GO`。

## 10. 下一步

13B-08、13C-04 和 13C-05 的功能与证据链已经收口。当前固定入口为：

```text
13D-01 顶部作业栏
-> 13D Qt 工作台布局收口。
```

13D-01 的 PREP 和执行指令已经完整，当前状态为 `READY FOR DEVELOPMENT`。
