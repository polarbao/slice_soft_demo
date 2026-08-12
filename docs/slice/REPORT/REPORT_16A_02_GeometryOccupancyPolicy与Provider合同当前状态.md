# REPORT_16A-02 GeometryOccupancyPolicy 与 Provider 合同当前状态

> 状态：**COMPLETE**
> 日期：2026-08-12

## 1. 完成内容

新增 STL-only `GeometryOccupancyPolicy`、列占用 DTO 和 `LayerOccupancyProvider`。Provider 可按 Legacy center-sample 语义物化逐层 mask 与列首末层；当前生产 `relief_heightfield` 路径显式选择并校验 Legacy 策略，但继续执行原有循环，确保默认输出零漂移。

## 2. 代码与测试

```text
src/slicer_core/geometry/GeometryOccupancyPolicy.h
src/slicer_core/geometry/LayerOccupancyProvider.h
src/slicer_core/geometry/LayerOccupancyProvider.cpp
tests/stage16/LayerOccupancyProviderTests.cpp
```

定向测试覆盖默认策略、Legacy 列范围、空列、范围裁剪、非法输入以及未实现候选 fail-closed。

## 3. 验收结果

| 验收项 | 结果 |
|---|---|
| STL-only 策略与 Provider API | PASS |
| Legacy 默认显式接入 | PASS |
| Layer Slab 未提前启用 | PASS / fail-closed |
| 2x2 未提前启用 | PASS / fail-closed |
| Stage 16 定向 CTest | 2/2 PASS |
| Golden TIFF layer 字节差异 | 0/25 |
| RGBWSV 协议和默认配置 | 未修改 |

## 4. 当前边界

`16A-02` 没有实现半开 layer slab、边界 2x2、多区间列或生产默认切换。下一张依赖已满足的卡是 `16A-03 Layer Slab Candidate`；其候选配置、heightfield 准入和 fail-closed 规则需单独实施。
