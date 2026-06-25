# TASKS_09B_R2_鲁棒性性能与多材质策略任务清单

> 文档版本：v0.2
> 文档状态：已按当前实现更新
> 适用阶段：09B-R2
> 说明：`[x]` 表示当前实验链路已完成或已验证；未勾选项保留为后续阶段工作。

---

## Milestone 09B-R2-0：分支与基线

- [x] 阅读 REPORT_09B_R1
- [ ] 提交并推送 09B-R1
- [x] 切出 `spike/09B-R2-shell-robustness-performance`
- [x] 保存 09B-R1 golden 摘要

## Milestone 09B-R2-1：真实业务 Golden

- [x] 真实指甲 OBJ fixture
- [x] 真实指甲 3MF fixture
- [x] 复杂浮雕 fixture
- [x] fixture hash / 来源 /许可记录
- [ ] OBJ/3MF 跨格式一致性
  - 说明：OBJ 与 3MF 都已跑通，但拓扑、材质和纹理数量不同，当前只记录各自 golden，不声明跨格式像素一致。

## Milestone 09B-R2-2：Scale-aware Tolerance

- [x] 新增 `MeshScaleTolerance.*`
- [x] position epsilon 与 bbox/voxel 关联
- [x] area epsilon 与模型尺度关联
- [x] tie epsilon 可配置
- [x] report 输出实际 epsilon

## Milestone 09B-R2-3：拓扑与鲁棒性诊断

- [x] connected components
- [x] duplicate faces
- [x] opposite duplicates
- [x] local winding inconsistency
- [ ] self-intersection broad/narrow phase
  - 说明：当前完成 AABB broad-phase candidate / sampled 统计，尚未完成完整 narrow-phase 三角相交判定。
- [x] zero-volume components
- [x] min edge/area/aspect ratio
- [x] thin feature warnings

## Milestone 09B-R2-4：Stable Tie-break 与 Seam

- [x] stable nearest hit comparator
- [x] equal-distance test
- [x] UV seam fixture
- [x] material seam fixture
- [ ] clamp/repeat fixture
  - 说明：clamp 已覆盖，repeat/wrap fixture 未建立。
- [x] 不跨 seam 平均 UV
- [x] 不跨 material seam 混色

## Milestone 09B-R2-5：BVH/Cache Instrumentation

- [x] query count
- [x] visited nodes
- [x] tested triangles
- [x] max visited nodes
- [x] BVH node count/bytes
- [x] texture cache hit/miss/bytes
- [x] per-material/per-texture counts

## Milestone 09B-R2-6：内存与 Benchmark

- [x] OpenVDB grid memory
- [x] mask memory
- [x] preview memory
- [ ] process peak working set
  - 说明：当前报告输出字段，但 Windows 进程峰值未接入，使用 `peakEstimatedBytes` 作为阶段基线。
- [x] generated 1k/10k/50k fixture
- [ ] 可选 100k fixture
- [x] Release benchmark script
- [x] BVH vs brute-force 采样对照

## Milestone 09B-R2-7：Voxel/Thickness Matrix

- [x] voxel 0.10/0.05/0.025
- [x] shell 0.05/0.10/0.20
- [x] shell monotonic
- [x] memory/time growth
- [x] small feature preservation

## Milestone 09B-R2-8：Report 与 Golden

- [x] benchmark report schema
- [x] robustness report fields
- [x] golden expected JSON
- [x] performance 不做 strict time equality
- [ ] warnings/errors 使用稳定 code
  - 说明：当前仍为字符串 warning/error，后续需要固化枚举 code。

## Milestone 09B-R2-9：Regression

- [x] 09B generated-box tests
- [x] 09B-R1 real-model tests
- [x] OpenVDB smoke
- [x] OFF build
- [x] OFF run_ci_quick
- [x] production RGBWSV 不变

## Milestone 09B-R2-10：状态报告

- [x] 更新 robustness/performance checklist
- [x] 生成 `REPORT_09B_R2_壳层纹理鲁棒性性能与多材质策略当前状态.md`
- [x] 判断是否进入 09P / 09C / 09B-R3
