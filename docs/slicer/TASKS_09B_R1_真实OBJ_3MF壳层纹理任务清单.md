# TASKS_09B_R1_真实OBJ_3MF壳层纹理任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：09B-R1  
> 建议提交目录：`docs/slicer/`

---

## Milestone 09B-R1-0：阶段与分支

- [x] 阅读 REPORT_09B
- [x] 提交并推送 09B 分支
- [x] 切出 `spike/09B-R1-real-model-shell-texture`
- [x] 确认不修改 production pipeline

---

## Milestone 09B-R1-1：SceneModel Adapter

- [x] 新增 `SceneModelTriangleMeshAdapter.*`
- [x] 转换 triangles
- [x] 保留 triangle UV/material mapping
- [x] 记录 source triangle index
- [x] 过滤退化三角形并记录
- [x] 保持 world-mm 坐标

---

## Milestone 09B-R1-2：Topology Diagnostics

- [x] 新增 `MeshTopologyDiagnostics.*`
- [x] 顶点量化去重
- [x] boundary edge 统计
- [x] non-manifold edge 统计
- [x] signed volume 统计
- [x] 可选全局 orientation flip
- [x] strict_closed / warn_and_attempt

---

## Milestone 09B-R1-3：Nearest Triangle Query

- [x] 新增 AABB BVH
- [x] closest point on triangle
- [x] barycentric coordinates
- [x] distanceMm
- [x] BVH 与 brute-force 小 fixture 对照测试

---

## Milestone 09B-R1-4：Texture Transfer

- [x] UV 重心插值
- [x] texture cache
- [x] 复用 `sample_texture_rgb`
- [x] texture → diffuse → fallback 优先级
- [x] maxTransferDistanceMm
- [x] 颜色来源统计

---

## Milestone 09B-R1-5：Real Model Report v2

- [x] schema = `p0.surface_shell_texture_report.2`
- [x] input 字段
- [x] meshDiagnostics
- [x] transferStats
- [x] performance
- [x] warnings/errors
- [x] 保留基础 shell stats

---

## Milestone 09B-R1-6：Fixtures

- [x] 真实 OBJ + MTL + PNG
- [x] 真实 3MF Texture2D
- [x] missing texture
- [x] no UV
- [x] open mesh
- [x] 可选 non-manifold

---

## Milestone 09B-R1-7：Demo 与 Tests

- [x] 新增 `surface_shell_real_model_demo`
- [x] 新增 `surface_shell_real_model_unit_tests`
- [x] 新增 `run_surface_shell_real_model_tests.ps1`
- [x] OBJ case 通过
- [x] 3MF case 通过
- [x] 负向 case 通过
- [x] 跨格式一致性检查

---

## Milestone 09B-R1-8：回归

- [x] generated-box 09B tests 仍通过
- [x] OpenVDB ON smoke 仍通过
- [x] OFF build 通过
- [x] OFF run_ci_quick 通过
- [x] production RGBWSV 未修改

---

## Milestone 09B-R1-9：状态报告

- [x] 更新 real model checklist
- [x] 生成 `REPORT_09B_R1_真实OBJ_3MF壳层纹理验证当前状态.md`
- [x] 判断是否进入 09B-R2 / 09P / 09C
