# HOSTFLOW H-D-01 俯视画布接线当前状态

> 状态：**COMPLETE**
> 日期：2026-08-09
> 范围：参考宿主真实俯视 ViewData 显示、本地缩放和平移、失败闭合。

## 1. 已完成内容

1. `topCanvas` 已由静态 `QLabel` 替换为 `TopViewCanvasWidget`。
2. 模型导入成功后，宿主使用权威 `sceneHandle/sceneRevision` 刷新 `TopViewRenderPolicy`。
3. 俯视画布显示真实 `surfacePreview`、buildVolume 和 1 mm/10 mm 网格合成图。
4. 鼠标滚轮缩放、中键平移、左键双击恢复适配均为宿主本地操作。
5. ViewData、纹理或 QImage 生成失败时清空旧图并显示明确错误，不降级为灰模。
6. 新增真实小马纹理甲片可见性测试和截图证据。

## 2. 冻结边界

- 未修改 `PM_SPI_VERSION=1`、11 个 `pm_*` 导出或 15 项能力；
- 未修改 `p0.rgbwsv.2`、RGBWSV 通道顺序、TIFF 或 OpenVDB 默认值；
- 未修改主干 `apps/slicer_debug_ui/**`；
- 未实现 3D、拾取、拖拽及所有场景变更后的自动刷新，这些分别属于 H-D-02..04；
- 未把 `apps/slicer_ui_host_sim` 加入源码尺寸白名单。

## 3. 验证结果

| 验证 | Debug | Release |
|---|---:|---:|
| H-D-01 真实模型俯视画布 | PASS | PASS |
| 14E-04d 双视图合同回归 | PASS | PASS |
| H-A/H-B 与 Qt 宿主联合门禁 | 22/22 PASS | 22/22 PASS |
| Qt 宿主边界 | PASS | PASS |
| 源码尺寸守卫 | PASS（35 个既有 warning） | 同一源码门禁 |

关键运行输出：

```text
HOSTFLOW_H_D_01_TOP_CANVAS_PASS triangles=11680 previewCache=1 localNavigationCalls=0
14E-04d dual-view contract: PASS calls=30 topReads=1 threeDReads=2
```

截图证据：

```text
build-slicesoft/main/hostflow_hd01_evidence/Debug/hostflow_hd01_real_model_top_view.png
build-slicesoft/main/hostflow_hd01_evidence/Release/hostflow_hd01_real_model_top_view.png
```

## 4. R-A-01 同批结论

`model/obj` 的 36 个 OBJ 中 17 个超过约 13.8k 三角面阈值，LOD 跳采样风险确认为 P1。
俯视图读取 `surfacePreview`，因此 H-D-01 已闭合；H-D-02 会直接消费 3D mesh，开工前应先
完成 R-B 方案选择，至少不得把破洞风险静默带入用户界面。

## 5. 下一步

按 HOSTFLOW 顺序，下一张互不依赖且可独立闭合的卡为 H-D-05。H-D-02 虽然形式前置已满足，
但受 R-A-01 的 P1 结果影响，应先处理或裁决 R-B-01/R-B-02。
