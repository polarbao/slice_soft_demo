# TASKS 13G 支撑投影铺底与层间连续性任务清单

> 状态：13G-00..07、13G-R1 COMPLETE
> 版本：v1.2
> 日期：2026-07-30

## 1. 固定顺序

```text
13G-00 证据与正反面 Gate
-> 13G-01 配置和 DTO
-> 13G-02 Core 最大投影铺底
-> 13G-03 SupportType 与 report
-> 13G-04 Qt 控件与 Effective Config
-> 13G-05 fixture / compatibility / RIP
-> 13G-06 Reality 单模型 Release
-> 13G-07 五模型矩阵与阶段收口
```

## 2. 原子任务

| Task | 内容 | 状态 | 完成标准 |
|---|---|---|---|
| 13G-00A | 冻结 Reality 五模型 Z 下包络和 20/21 层证据 | COMPLETE | Audit 含五模型表格与真实 TIFF 数据 |
| 13G-00B | 甲片 front-up 自动定向修正 | COMPLETE | synthetic face-up 保持、face-down 翻转、disabled 不动；Reality 5/5 翻转 |
| 13G-00C | 正确姿态下 segment_105 支撑复测 | COMPLETE | S 在 layerIndex 0..93 连续；20/21/30 层不再中断 |
| 13G-01 | `support.baseProjection` Config/DTO/validator | COMPLETE | 缺省兼容关闭；enabled/layerCount/source 解析与负向校验 PASS |
| 13G-02 | 最大支撑 footprint 与前 N 层应用 | COMPLETE | 0..N-1，无 off-by-one |
| 13G-03 | `ProjectionBase`、layer/totals report | COMPLETE | 原因和像素统计可审计 |
| 13G-04 | Qt 支撑铺底控件与一键链路 | COMPLETE | UI 修改进入 scene Effective Config |
| 13G-05 | 单元、兼容、材料一致性和 RIP | COMPLETE | 旧配置缺省关闭，新 fixture/RIP PASS |
| 13G-06 | segment_105 Release 单模型验证 | COMPLETE | TIFF/RIP/report PASS |
| 13G-07 | Reality 五模型轻量矩阵与收口 | COMPLETE | 5/5 只读定向矩阵、单模型 Release 和报告完成 |
| 13G-R1 | 生产铺底改为新增模型下方物理层 | COMPLETE | UI 写入 prepend_below_model；总 TIFF 层数 +N；旧 overlay fixture 兼容；RIP PASS |

## 3. 停止条件

```text
frontUp 判定无法稳定区分 synthetic/标准/Reality 甲片；
正确姿态复测证明产品对“内部支撑”仍有不同解释；
base footprint 会覆盖模型或破坏 V 优先级；
旧 fixture 在字段缺省时产生 TIFF 变化；
任何协议字段发生变化。
```

命中停止条件时不得继续叠加补丁。

## 4. 验证规则

每个原子任务必须：

```text
先运行 targeted test；
再运行 git diff --check；
代码任务至少运行对应 Debug target；
阶段收口运行 Quick CI 和 Reality 单模型 Release/RIP；
不得把 0.10 mm 趋势诊断当作 635/600 生产验收。
```

## 5. 安全边界

```text
Legacy 保持默认；
OpenVDB 保持可选且默认关闭；
不修改 p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8 / black_is_print；
不按文件名特判；
支撑铺底只写 S。
```
