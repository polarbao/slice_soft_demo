# REPORT_12E-09D 生产纹理厚度与单材料材质当前状态

> 状态：COMPLETE / RELEASE MATRIX PASS
> 日期：2026-08-03

## 1. 阶段成果

```text
09D-01：冻结 Legacy/Global/诊断生产合同和稳定错误码；
09D-02：实现 ProductionTextureSettingsModel 与 Effective Config 写回；
09D-03：实现单材料浮雕 W/V 原子 Resolver；
09D-04：实现 Qt 条件化生产控件及诊断隔离；
09D-05：一键切片、摘要、uiAudit 和 package 状态证据同源；
09D-06：Release 生产矩阵、RIP strict 和阶段文档收口。
```

## 2. 生产语义

```text
Legacy top_surface_band：texture.topSurfaceLayers，等效厚度=层数*layerThicknessMm；
Global global_surface_shell：texture.surfaceShell.mode + widthMm；
all_texture：显式模式，不使用超大宽度哨兵值；
Diagnostic：只读分析，不写生产参数；
single relief：white -> W，varnish -> V，完整字段组原子更新。
```

## 3. 验证结论

Release 矩阵覆盖 Legacy 1/3/10、Global min/mid/all_texture、单材料 W/V。6 个定向测试、Qt self-test、2 个 UI Smoke、8 个生产 package 和 RIP strict 全部通过。

## 4. 固定边界

```text
schema=p0.rgbwsv.2；channelOrder=R G B W S V；
bitDepth=8；polarity=black_is_print；printValue=0；emptyValue=255；
Legacy 仍为默认，Global 仅显式 opt-in；
OpenVDB 仍是可选候选；
12G-TCWS 继续冻结。
```

## 5. 下一阶段

`12E-10A` 已具备技术和文档前置，负责生产 TIFF 与 Texture Surface / Model Fill / Partition / W/S/V 的同层最终一致性。
