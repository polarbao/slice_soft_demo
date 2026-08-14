# REPORT HOSTFLOW H-F-06 旧版切片语义与结果单预览回归修复

> 状态：COMPLETE
> 日期：2026-08-14
>
> 更正（2026-08-14）：本报告当时的方向结论只证明新旧生产 TIFF
> 栅格没有旋转或镜像，不能证明“工作区图像”和“结果 PNG”采用相同显示行原点。
> 后续 H-F-07 确认 TIFF 行 0 为最小 Y，而 PNG 行 0 位于顶部，并在纯显示层完成
> `+Y` 向上统一；详见
> `REPORT_HOSTFLOW_H_F_07_连续切片与结果朝向统一修复.md`。

## 1. 问题与证据

对比 `runtime/slicesoft/Release/output/h260814133559923/package` 与
`runtime/slicesoft/Release/output/ui_sessions` 的同一五模型生产包：两者均为
`2499 x 623`、145 层，实例包围盒和变换一致。六通道 TIFF 占用投影的原方向 IoU 为
`0.9999901`，翻转 X/Y 后反而明显下降，因此生产栅格没有发生旋转或镜像。

通道总量却发生了材料重分类：

| 包 | R | G | B | W | S | V |
|---|---:|---:|---:|---:|---:|---:|
| 新宿主缺陷包 | 82,204,628 | 82,449,640 | 83,279,951 | 996,182 | 0 | 0 |
| 旧版兼容包 | 0 | 0 | 0 | 13,405,847 | 71,281,292 | 0 |

旧包的 71,281,292 个 S 像素在缺陷包中仍位于打印占用内，说明不是几何缺失，而是被
RGB/W 提前占用。该次比对只证明新旧生产 TIFF 没有旋转或镜像；工作区和结果 PNG 的
显示行原点并未在 H-F-06 覆盖，后由 H-F-07 确认并统一。本次不修改相机方向。

## 2. 根因

旧版纹理配置显式使用：

```json
"relief": {
  "baseZMm": 0,
  "fillMode": "intersection_range"
}
```

参考宿主把纹理任务切换为 `relief_heightfield` 时，动态 Profile 构造器遗漏了 `relief`
对象。核心配置默认值为 `surface_to_base`，于是纹理列从顶面一直填到基准面，原本应位于
模型下方的 lower support 区域被 RGB/W 实体材料占用，最终得到 `S=0`。

## 3. 修复

1. 动态纹理 Profile 显式写入并参与自哈希：`relief.fillMode=intersection_range`、
   `relief.baseZMm=0`；非纹理 Profile 不增加该字段。
2. H-B-07 包回归改用带悬空体的纹理 Profile，要求最终生产包 S 通道打印像素大于 0。
3. 结果页删除首层 A/当前层 B 双渲染，只保留当前生产层单预览；默认通道从纯 RGB
   改为与旧版相同的 RGBWSV 合成，并显示当前层通道统计。
4. 生产 TIFF、`p0.rgbwsv.2`、通道顺序、位深、极性、S0/P0 默认值均未修改。

## 4. 验证

```text
Release build:
  hostflow_hb05_slice_settings_tests
  hostflow_hb07_package_review_tests
  slicer_ui_host_sim

Release CTest: 4/4 PASS
  hostflow_hb05_slice_settings
  hostflow_hb05_settings_ui_smoke
  hostflow_hb07_package_review
  hostflow_hb07_result_ui_smoke

runtime/slicesoft/Release:
  STAGE14E02_SELF_TEST_PASS
  HOSTFLOW_HB07_UI_PASS
```

运行时已部署到 `runtime/slicesoft/Release`；已有 `output` 目录保留。
