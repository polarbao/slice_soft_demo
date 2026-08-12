# REPORT_16B-01 边界带与接触指标基线当前状态

> 状态：**COMPLETE**
> 日期：2026-08-12

## 1. 完成内容

新增无 Qt、只读的 `ContactPostureMetrics`，冻结两侧 `12.5%` 边界带、前 `1/2` slab
接触面积代理、候选滚转角以及 +Z/+Y 方向约束。非法策略、空几何、错误长轴和空边界带
均显式拒绝，不修改模型顶点。

新增 `stage16_posture_baseline`，对 Reality 5/5 和标准 `nai_you` 记录资产 SHA-256、
定向、bbox 与指标，并输出 `slicesoft.stage16.posture_baseline.1`。

## 2. 真实资产结果

| 资产 | 定向 | 候选角 | 前 1/2 slab 面积代理 | 结果 |
|---|---|---:|---:|---|
| reality_101 | `identity_rotate_x_180_rotate_z_minus_90` | -4.906 deg | 0.0903 mm2 | PASS |
| reality_102 | 同上 | -2.628 deg | 0.0983 mm2 | PASS |
| reality_103 | 同上 | -1.968 deg | 0.1101 mm2 | PASS |
| reality_104 | 同上 | 6.349 deg | 0.1166 mm2 | PASS |
| reality_105 | 同上 | 10.568 deg | 0.6572 mm2 | PASS |
| standard_nai_you | `rotate_x_90_rotate_z_180` | 0.00003 deg | 0.0146 mm2 | PASS |

六个资产均满足长轴 +Y、正面 +Z、尖端 +Y 和 `abs(angle) <= 12 deg`。这只是测量
基线，不表示上述候选角已经适合应用。

## 3. 验收

| 验收项 | 结果 |
|---|---|
| Debug 定向目标构建 | PASS |
| 合成指标单测 | PASS |
| Reality 5/5 + 标准甲片 | 6/6 PASS |
| 基线重复生成 | SHA-256 一致 |
| 自动定向既有单测 | PASS |
| 默认切片/模型几何 | 未修改 |
| RGBWSV 协议 | 未修改 |

基线证据：`docs/slice/REPORT/assets/posture_baseline.json`。

## 4. 当前边界

接触面积为三角形裁剪后的 XY 投影面积之和，不等于去重后的二维并集。该定义用于固定
诊断基线和比较候选，不能直接替代生产层占用。下一张接触姿态卡为 `16B-02`；16D 仍等待
`16A-06` 和必要的 16B 候选。

