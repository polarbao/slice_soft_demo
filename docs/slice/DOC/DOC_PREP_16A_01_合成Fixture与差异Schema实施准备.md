# DOC_PREP_16A-01 合成 Fixture 与差异 Schema 实施准备

> 状态：**COMPLETE / IMPLEMENTED**
> 日期：2026-08-12
> 任务：`16A-01`

## 1. 准入

`REPORT_16_00_Stage16准入复核当前状态.md` 已形成 PARTIAL GO，用户已授权首张代码卡 `16A-01`。本任务只建立测试资产和工程差异合同，不接入生产采样路径。

## 2. 冻结内容

合成 Fixture 覆盖：

```text
flat_bottom_block；
ascending_wedge；
descending_wedge；
circular_contact_edge；
subpixel_thin_sheet；
multi_interval_column_negative。
```

差异合同覆盖：

```text
逐层 occupancy 的 false-positive / false-negative；
逐通道 R/G/B/W/S/V 打印像素差异；
connected-component delta；
首末非空层；
model/support/union 总量；
X/Y/Z 像素与物理尺寸偏差；
RIP strict 状态；
不可获得指标显式为 null。
```

## 3. 边界

```text
Fixture 不依赖 Reality 文件名或外部 TIFF；
期望值可由半开层区间和固定 2x2 覆盖手算；
多区间列只冻结 fail-closed 错误码，不实现通用网格采样；
不修改生产配置、Provider、TIFF、材料或支撑路径；
Legacy 默认和现有 Golden 不受影响。
```

## 4. 验证

```text
cmake --build build-slicesoft/main --config Debug --target stage16_geometry_sampling_fixture_tests
ctest --test-dir build-slicesoft/main -C Debug -R ^stage16_geometry_sampling_fixture_tests$ --output-on-failure
```

结果：构建 PASS，CTest 1/1 PASS。
