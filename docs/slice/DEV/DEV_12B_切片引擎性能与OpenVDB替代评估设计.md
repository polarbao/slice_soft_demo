# DEV_12B_切片引擎性能与OpenVDB替代评估设计

> 文档版本：v0.2
> 文档状态：DEV / Stage 12B
> 生成日期：2026-07-05
> 更新日期：2026-07-08
> 前置文档：PRD_12B_切片引擎性能与OpenVDB替代评估.md

---

## 0. 当前执行入口

12B 已拆分为 R0/R1/R2。R0/R1 已完成，当前执行 R2：

```text
docs/codex_task/current/TASKS_12B_R2_OpenVDB_SDFUtility定位任务清单.md
```

R0 已固化 benchmark schema 和真实 Release baseline；R1 已完成 legacy hotspot profile、首个低风险优化和 heightfield fast path 可行性评估；R2 只评估 OpenVDB 是否适合作为 SDF utility，不替代 legacy production path。

## 1. 设计目标

建立可复现的 benchmark 和 engine gate：

```text
1. coreComputeMs 不包含 TIFF/preview/report I/O；
2. legacy 和 OpenVDB 使用同一模型姿态、同一分辨率、同一层厚；
3. 输出 semantic summary 可比较；
4. benchmark 可在 OpenVDB OFF / ON 构建中分层运行；
5. 结果写入 JSON，便于报告和 CI 读取。
```

---

## 2. 建议模块

```text
SlicingBenchmarkRunner
  负责读取 benchmark config、运行各引擎、输出 JSON。

SliceEngineAdapter
  抽象 legacy/openvdb/heightfield_fast_path 的统一接口。

LegacyEngineAdapter
  调用现有 slicer_core legacy path，并关闭生产文件写出。

OpenVdbEngineAdapter
  调用 OpenVDB candidate path，OpenVDB OFF 时返回 unavailable。

SemanticComparator
  比较 layerCount、grid、modelPixels、supportPixels、RGB/W/V/S 统计。

TimingScope
  拆分 importMs、coreComputeMs、materialComposeMs、ioWriteMs、previewWriteMs。
```

---

## 3. Benchmark 数据契约

建议输出：

```json
{
  "schema": "slicesoft.benchmark.12b.1",
  "caseName": "nai_you_new_same_pose",
  "buildType": "Release",
  "modelPath": "model/obj/nai_you_new/...",
  "grid": { "width": 283, "height": 531, "layers": 717 },
  "engines": [
    {
      "engine": "legacy",
      "available": true,
      "coreComputeMs": 49.716,
      "ioWriteMs": 0.0,
      "previewWriteMs": 0.0,
      "modelPixels": 13781916,
      "supportPixels": 36902358,
      "semanticHash": "..."
    },
    {
      "engine": "openvdb",
      "available": true,
      "coreComputeMs": 1038.711,
      "outputSemanticsComparable": false,
      "failureReason": "support_semantics_not_implemented"
    }
  ],
  "comparison": {
    "openvdbVsLegacyCoreRatio": 20.893,
    "replacementPass": false
  }
}
```

---

## 4. 公平比较规则

```text
1. 必须使用 Release 构建作为性能结论；
2. Debug 结果只能用于功能调试；
3. same-pose 必须显式记录 transform/autoOrient 结果；
4. 同一 benchmark 禁止一个引擎写 TIFF，另一个引擎不写；
5. 不可比较时必须输出原因，不允许给出速度结论；
6. OpenVDB ON/OFF 构建均应有脚本入口。
```

---

## 5. OpenVDB 优化方向

仅在确认语义可比较后再优化：

```text
1. 缓存 MeshToVolume / SDF grid；
2. 按层批量采样，而不是逐像素高开销查询；
3. 使用 tile/leaf node 遍历减少空白区域计算；
4. 仅对外侧光油/壳层/距离场使用 OpenVDB；
5. 对 2.5D 甲片模型启用 heightfield fast path，不强制走 VDB。
```

---

## 6. 其他加速实现设计

### 6.1 Legacy Active Edge / Z Bucket

设计：

```text
1. 预处理 triangle z range；
2. 按 layerIndex 建 bucket；
3. 每层只扫描相交三角；
4. scanline 复用边交点；
5. 纹理采样按 triangle/cache 复用。
```

### 6.2 Heightfield Fast Path

设计：

```text
1. 输入模型先通过 admission 判断是否近似 2.5D；
2. rasterize topHeight/bottomHeight；
3. 每层 modelMask = bottomHeight <= z <= topHeight；
4. 支撑按列和轮廓快速生成；
5. 表面纹理只在 top/shell mask 采样。
```

### 6.3 Hybrid Engine

设计：

```text
legacy/heightfield 负责 production model/support；
OpenVDB 负责 outer varnish shell、clearance、复杂拓扑诊断；
最终统一进入 12A semantic composer。
```

---

## 7. 验证命令

建议新增或扩展：

```powershell
.\scripts\run_12b_core_benchmark.ps1 -BuildType Release -Engine legacy
.\scripts\run_12b_core_benchmark.ps1 -BuildType Release -Engine openvdb
.\scripts\run_12b_core_benchmark.ps1 -BuildType Release -Engine all -NoImageWrite
```

OpenVDB OFF 轨道：

```powershell
cmake --build build --config Release --target slicer_cli
.\scripts\run_12b_core_benchmark.ps1 -Engine legacy
```

OpenVDB ON 轨道：

```powershell
cmake --build build-openvdb-09p --config Release --target slicer_cli
.\scripts\run_12b_core_benchmark.ps1 -Engine openvdb
```

---

## 8. 输出结论格式

每轮 benchmark 后必须给出：

```text
1. 当前最快 production 引擎；
2. OpenVDB 是否可比较；
3. OpenVDB 是否通过 replacement gate；
4. 耗时瓶颈在 core 还是 I/O；
5. 下一步优化应该投 legacy、heightfield、OpenVDB hybrid 还是 GPU/BVH。
```
