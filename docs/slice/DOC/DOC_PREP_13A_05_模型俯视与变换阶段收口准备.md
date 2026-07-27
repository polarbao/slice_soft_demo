# DOC_PREP 13A-05 模型俯视与变换阶段收口准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-27
> 前置：13A-04 COMPLETE（`5caa7fb`）
> 任务性质：验证、说明和阶段证据收口，不新增产品能力

## 1. 目标

将 13A-01..04 已实现的实例合同、+Z 俯视、精确变换、镜像和 transformed preflight 作为一个可审计
候选进行统一回归，生成 M13-1 单模型俯视与变换阶段报告，并为 13B-02 多模型列表提供稳定前置。

## 2. 固定范围

```text
复核 X/Y、rotateZ、uniformScale、mirrorX/mirrorY、居中和重置；
复核只读源模型、scene/transform revision、dirty/stale 和 session config；
复核 source/transformed 与 Legacy/Global 独立准入；
复核 blocked 可见、生产动作 fail-closed；
补齐用户操作说明、索引、上下文和 REPORT_13A；
仅修复收口验证发现的 13A 回归，不扩展需求。
```

## 3. 非目标

```text
不接通 scene effective config 到生产 slicer_cli；
不实现模型列表、复制/删除、11x2 排版或联合切片；
不实现自动修复、3D 视口、鼠标 gizmo 或非均匀缩放；
不实现 TIFF 原生统一预览；
不改变 RGBWSV 生产协议、材料策略或双引擎默认值。
```

## 4. 验证矩阵

### 4.1 Core

```text
model_transform_unit_tests；
scene_view_geometry_unit_tests；
scene_transform_controller_unit_tests；
model_preflight_admission_unit_tests；
model_preflight_service_unit_tests；
transformed_model_preflight_unit_tests。
```

### 4.2 Qt

```text
--self-test；
model-top-view；
model-top-view-transform；
model-transform-preflight。
```

每个 UI Smoke 必须覆盖 1280x720、1440x900 和 1920x1080，确认模型、变换控件和准入状态无重叠。

### 4.3 资产

```text
xiao_ma、yecan：strict-PASS 正向；
Texture2D 3MF：纹理资源和镜像 UV 正向；
复杂浮雕或开放拓扑 fixture：blocked 反向。
```

资产缺失或与清单身份不一致时记录为未验证，不得用其他结果代替。

## 5. 完成输出

```text
REPORT_13A_模型俯视工作区与实例变换当前状态.md；
更新 REPORT_13_模型场景排版与TIFF原生预览准备状态.md；
更新 REPORT_12X 和 Stage 12/13 执行看板；
更新用户操作说明和文档索引；
更新 ai_workspace 当前上下文；
M13-1 候选结论；
下一任务明确为 13B-02，但不在本任务实现。
```

## 6. Gate

仅当 Debug build、定向 CTest、Qt self-test、三项 UI Smoke 和 Quick CI 全部通过，且没有把 blocked
场景误放行到生产，13A-05 才可标记 COMPLETE。外部设备 buildVolume、机器轴方向和 22 实例性能预算
不阻断本任务，但继续阻断后续 13B production GO。
