# CODEX_PROMPT 13B-04 幅面、碰撞与逐实例准入执行指令

> 状态：COMPLETE（FUNCTIONAL FIXTURE）
> 日期：2026-07-27
> 前置：13B-03 COMPLETE
> Production Gate：正式设备 buildVolume/origin/axes OPEN
> 完成报告：`REPORT_13B_04_幅面碰撞与逐实例准入当前状态.md`

## 1. 必读

```text
AGENTS.md
docs/slice/REPORT/REPORT_13B_03_11x2规则排版当前状态.md
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md
docs/slice/DEMO/DEMO_13B_多模型排版联合切片验证方案.md
docs/slice/DOC/DOC_PREP_13B_04_幅面碰撞与逐实例准入准备.md
docs/slice/DOC/DOC_CHECKLIST_13_未决产品输入与阶段Gate.md
```

## 2. 本次范围

1. 新增无 Qt `SceneCollisionService` 或等价服务；
2. 校验显式 fixture/device buildVolume、origin、axes 和 purpose；
3. 对可见实例执行 bbox 越界检查；
4. 执行 AABB 快筛和投影三角形精确碰撞；
5. 校验逐实例 admission、scene/transform revision 和 geometry identity；
6. 输出稳定错误码、per-instance 结果和碰撞统计；
7. 新增 `scene_collision_admission_unit_tests`；
8. 可在现有排版页显示诊断摘要，但不得在 Qt 层实现算法；
9. 更新状态报告、任务总览和上下文。

## 3. 固定边界

```text
只完成 fixture 功能 Gate；
Production 遇到 fixture/unresolved buildVolume 必须 fail-closed；
隐藏实例保留身份但不参与占用；
所有可见实例必须 admitted；
不自动移动碰撞实例；
不实现 13B-05 全局 Raster；
不写 TIFF/package；
不修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
不修改 Legacy 默认和 OpenVDB optional/OFF。
```

## 4. TDD 顺序

1. 先写 buildVolume、越界、AABB/精确碰撞、admission 和 stale 失败测试；
2. 运行目标并记录 RED；
3. 最小实现 DTO、稳定错误和纯函数服务；
4. 增加 hidden、fixture/production、确定性和逐实例身份测试；
5. 如有 Qt 摘要，增加现有 smoke 回归；
6. 运行定向 CTest 和 Quick CI；
7. 生成 `REPORT_13B_04_幅面碰撞与逐实例准入当前状态.md`。

## 5. 停止条件

```text
需要虚构正式设备 width/height/origin/axes；
需要把 fixture PASS 写成 production PASS；
需要提前实现联合 Raster/TIFF/package；
需要在 Qt 层复制碰撞算法；
任何失败会修改场景或发布部分成功结果；
13B-03 或单模型回归失败。
```
