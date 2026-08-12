# REPORT_16B-02 ContactLevelingAnalyzer 当前状态

> 状态：**COMPLETE**
> 日期：2026-08-12

## 1. 实现内容

新增无 Qt `ContactLevelingAnalyzer`，在冻结的长轴约束下执行固定粗搜与精化，按首半 slab
接触面积、两侧包络和最小角度确定唯一 diagnostic-only 候选。每个候选临时归地并检查
+Z/+Y、角度、高度和占地预算；输入模型不被修改。

新增合成单测及真实资产诊断工具。六个真实/标准资产均生成满足冻结预算的只读候选，
生产自动定向和实例变换未改变。

## 2. 真实资产结果

| 资产 | 候选角度 | 首半 slab 面积增量 mm2 | 高度增量 mm | X 占地增量 mm | 结果 |
|---|---:|---:|---:|---:|---|
| Reality 101 | -5.4 deg | +0.044559 | +0.494341 | -0.209923 | PASS |
| Reality 102 | -7.3 deg | +0.078915 | +0.495570 | -0.223294 | PASS |
| Reality 103 | +12.0 deg | +0.253212 | +0.326654 | +0.017550 | PASS |
| Reality 104 | +7.7 deg | +0.408284 | +0.493972 | -0.185073 | PASS |
| Reality 105 | +2.2 deg | +0.496812 | +0.113166 | -0.051200 | PASS |
| 标准 nai_you | 0.0 deg | 0.000000 | 0.000000 | 0.000000 | PASS / 保持基线 |

机器证据：`assets/contact_leveling_diagnostic.json`，schema 为
`slicesoft.stage16.contact_leveling_diagnostic.1`，SHA-256 为
`371eb10a88cf5ecddce9ee161f2ee0af7a71e8acead4875c6720bcbf55171119`。

## 3. 验证

```text
Debug 构建：stage16_contact_leveling_analyzer_tests PASS
Debug 构建：stage16_contact_leveling_diagnostic PASS
CTest：contact posture / leveling / orientation baseline / real diagnostic 4/4 PASS
真实矩阵：Reality 5/5 + 标准甲片 1/1，共 6/6 PASS
```

诊断工具耗时约 45 秒；该数字只用于说明本地验证成本，不是生产切片性能基线。

## 4. 保持不变

```text
不修改 ModelReport 顶点；
不应用实例变换；
不修改 autoOrient、Facade、Worker、Qt 或配置；
不改变 RGBWSV、材料、支撑和 TIFF；
默认姿态仍为 P0。
```

## 5. 后续 Gate

16B-03 仍依赖 16A-05 冻结的采样比较口径；16B-02 的候选结果不构成实际调平授权。
16D 继续等待 16A-06，不能因本卡完成而提前接入配置、Facade、Worker 或 Qt。
