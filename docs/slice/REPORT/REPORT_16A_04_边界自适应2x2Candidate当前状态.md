# REPORT_16A-04 边界自适应 2x2 Candidate 当前状态

> 状态：**COMPLETE**
> 日期：2026-08-12

## 1. 完成内容

新增两个仅限 `relief_heightfield`、默认关闭的几何采样候选：

```text
S3 layer_slab_supersample_2x2_at_least_two_candidate：同层 >=2/4；
S4 layer_slab_supersample_2x2_any_hit_candidate：同层 >=1/4。
```

四个样本固定为 `(0.25,0.25)`、`(0.75,0.25)`、`(0.25,0.75)`、`(0.75,0.75)`。
明确内部复用中心列，明确外部跳过，只有三角形 XY 投影候选边界执行四点求交。

## 2. 语义与内存结果

Provider 按输出像素和输出层独立累计四个单区间样本的 Layer Slab 覆盖，不做整列预投票。
中间态只保存四组二维 Heightfield 单区间列，没有生成或常驻高分辨率三维体。中心未命中的
候选边界使用最高 Z 的有效子样本作为纹理代表点，固定遍历顺序保证确定性。

## 3. 验收结果

| 验收项 | 结果 |
|---|---|
| Debug 定向目标构建 | PASS |
| Stage 16 定向 CTest | 2/2 PASS |
| S3 Package / RIP strict | PASS，24 x 48 x 20，warnings=0 |
| S4 Package / RIP strict | PASS，24 x 48 x 20，warnings=0 |
| Legacy Golden TIFF SHA-256 | 0/25 差异 |
| 通用 mesh / 非法阈值 | fail-closed |
| 默认策略 | 仍为 Legacy |
| RGBWSV 协议 | 未修改 |

## 4. 当前边界

本卡不决定 S3/S4 默认值，不执行 Reality/Stage 15 的完整候选矩阵，也不实现姿态或性能优化。
下一张 16A 卡为 `16A-05`；用户已授权同步推进，`16B-01` 与 `16C-01` 可在各自准备 Gate
通过后独立实施，`16D` 仍受 `16A-06` 和必要的 16B 依赖阻塞。
