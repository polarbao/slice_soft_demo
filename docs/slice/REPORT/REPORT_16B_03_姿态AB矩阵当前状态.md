# REPORT_16B-03 姿态 A/B 矩阵当前状态

> 状态：**COMPLETE / P3 NOT YET APPROVED FOR PRODUCTION**
> 日期：2026-08-12

## 1. 实现内容

新增 `stage16_posture_matrix`，在导入、自动定向且落地的模型副本上比较 P0/P2/P3。新增
`ApplyContactLevelingAngle` 作为可测试的无副作用几何操作，P0 生产默认、Profile、Facade、
Worker 与 RGBWSV 协议均未改变。

## 2. Reality 真实矩阵

| 资产 | P2 接触改善 mm² | P2 支撑像素差 | P3 角度 | P3 接触改善 mm² | P3 支撑像素差 | P3 高度增量 mm |
|---|---:|---:|---:|---:|---:|---:|
| Reality 101 | -0.023881 | -3,146,345 | -5.4° | +0.044559 | +6,059,541 | +0.4943 |
| Reality 102 | -0.009523 | -538,767 | -7.3° | +0.078915 | +3,297,861 | +0.4956 |
| Reality 103 | -0.019533 | -1,149,454 | +12.0° | +0.253212 | +3,624,004 | +0.3267 |
| Reality 104 | -0.043528 | -4,064,931 | +7.7° | +0.408284 | +4,399,776 | +0.4940 |
| Reality 105 | -0.556987 | -2,669,311 | +2.2° | +0.496812 | +487,659 | +0.1132 |

P2 的下包络一次平衡在 Reality 5/5 上均降低了首半层接触面积，因此不能只凭“两侧更接近
等高”决定生产姿态。P3 在 5/5 上提高接触面积，但支撑变化很大，且 103 命中 +12° 边界，
说明 P3 仍需 16B-04 受控 opt-in 和工艺评估，不能直接成为默认。

标准甲片 `MF_nai_you` 的 P2/P3 均约为 0°，表明已平衡资产不会被明显改姿态。

## 3. 准入与约束

```text
Reality P0/P2/P3 S3 core-only：15/15 PASS
准入状态变化：0
+Z/+Y 约束：18/18 PASS（含标准甲片三姿态）
高度增量预算：P3 Reality 5/5 <= 0.5 mm
X 占地增量预算：P3 Reality 5/5 <= 0.5 mm
生产默认：P0
```

## 4. 验证

```text
Debug stage16_contact_leveling_analyzer_tests：PASS
Debug stage16_posture_matrix_tests --quick：PASS
Release posture_matrix Reality 5/5 + 标准甲片：PASS
```

归档证据：`assets/posture_matrix.json`

```text
schema = slicesoft.stage16.posture_matrix.1
assetCount = 6
pass = true
SHA-256 = E7A6320BB2821FA551C5572ED69498774F6005C44C98472DB20F7FD1EC0CCF2F
```

## 5. 后续 Gate

16B-03 证明 P2 不适合作为当前候选；P3 是唯一值得继续的姿态候选，但尚未获准进入生产。
16B-04 必须显式选择 P3、保持 P0 默认、定义 fail-closed 和 `autoOrient=false` 不受影响，并取得
用户单独授权。16D-01 当前只能先接入 16A-06 已批准的 S3 采样字段。

