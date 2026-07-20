# DOC_EXEC_12E-08C-R1-04 真实模型 Pre-Repair Baseline 结果

> 文档状态：COMPLETE
> 日期：2026-07-20

## 1. 实现结果

新增只读 `MeshRepairPreflight` 核心服务、`mesh_repair_preflight` 诊断工具和
`run_12e_08c_r1_pre_repair_baseline.ps1` 真实模型证据脚本。服务复用当前最终姿态后的
`AdaptedTriangleMesh`、`MeshTopologyDiagnostics`、`MeshRobustnessDiagnostics`、canonical hash 与
Eligibility Policy，生成 `slicesoft.mesh_repair.12e_08c.1` 报告。

固定边界：

```text
repairEnabled=false；
repairAttempted=false；
operations=[]；
postRepair.available=false；
productionOutputWritten=false；
admission.productionAllowed=false。
```

脚本为每个 case 记录 effective config、OBJ/MTL/texture 或 3MF 资产 SHA-256、source/geometry/attribute/options
hash、最终外部 transform、拓扑统计、资格决策和人工建议。每个 case 连续执行两次，重复性 hash 排除性能计时；
`--require-openvdb-off` 会拒绝误用 OpenVDB ON 构建，确保该基线来自默认 OFF 轨道。

## 2. 真实模型结果

| Case | V/T/C | boundary | non-manifold | duplicate/opposite | degenerate | 状态 |
|---|---:|---:|---:|---:|---:|---|
| `nai_you_new` | 58924 / 117705 / 10 | 113 | 0 | 0 / 0 | 1 | `manual_repair_required` |
| `aishen_fudiao` | 42193 / 84533 / 10 | 3 | 59 | 2 / 2 | 1 | `manual_repair_required` |
| `meigui_fudiao` | 34722 / 76926 / 2 | 0 | 10940 | 7192 / 7192 | 0 | `manual_repair_required` |
| `three_mf_texture2d_checker` | 8 / 12 / 1 | 0 | 0 | 0 / 0 | 0 | `strict_pass_no_repair` |

三个 OBJ 的当前 robustness 检查都达到 `max_triangle_pair_checks` 采样上限，分别记录 11、32、13 个候选，
因此还包含 `MESH_SELF_INTERSECTION_SAMPLED/manual_only`。该状态表示完整自相交证据不足，不表示已确认
自相交。闭合 Texture2D 3MF 为 no-op strict PASS，证明本来闭合的输入不会被误判为 repair candidate。

## 3. 确定性证据

| Case | geometry SHA-256 | attribute SHA-256 | stable evidence SHA-256 |
|---|---|---|---|
| `nai_you_new` | `22cf335325afa1552140b9378acdc51262479ee0f577453da4beab5c8e22e4c1` | `1f2804cc5f804528fff778fa95a185fd1c743049e10e9336ec1928a7dc3dfa21` | `06833341b65995e2790571efcb85ecf6600543f4637c78a7411efc5a8595bca1` |
| `aishen_fudiao` | `376263881b0afd82e11e9b4e531cbf0338f2f454dee6d109405a90f69aa59316` | `b5b193f9997f825d3d1c314d9838e9417dcd63b6f99b626b9864b1320e4f3cc0` | `1b6db858238dc165d4478da4607dc2681612b06cf25bc41b345ef25d2878e774` |
| `meigui_fudiao` | `1466a9ecc300a07bf2b9976b0d2869dbb1670657b6aa16df3156d624037cf140` | `3b4edb9bf247072984425f411752fc0dbc91f9881ab2f2c40ee130f3305657c4` | `642d3680c3169fa418d5c9f214df0cd7d8f77b326c4a1632daede108dc4eebcb` |
| `three_mf_texture2d_checker` | `84ea358011f7a67683dc52807f66abf97d58e5f62b353e7c05b802f6b7f4ff53` | `aa827818918142ea32793ccdc1bb1667a506cd8989fd3dbf2bfa88c7a9ed3e5c` | `efcb22e35105c86bf8c10cf6b2f8bd2a661b6c8aa7d9052c8cf8b4ccb7a22f46` |

证据路径：`output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json`。`output` 为本地生成目录，
不作为源码提交；可由脚本重新生成。

## 4. R2/R3 范围复核

R2-01 可以开始，但必须保持以下收敛范围：

```text
nai_you_new / aishen_fudiao 的退化面已在 adapter 阶段被过滤，R2-01 必须把该过滤转为可追溯 cleanup，
不能把已拒绝退化面重新送入 downstream mesh；
aishen_fudiao 与 meigui_fudiao 的 duplicate 全部属于 opposite duplicate，R2-01 不得按同属性 exact duplicate 删除；
10/10/2 个组件要求 R2-02 保持 no implicit merge；
boundary 分别由 R2-03 分类；
大规模 non-manifold/opposite duplicate 继续由 R3 pattern classifier 处理。
```

真实 OBJ 均出现 sampled self-intersection evidence，原 R3 准备缺少完整证据原子任务。现新增
`12E-08C-R3-01A 完整自相交证据`，在真实模型 Repair Matrix 前完成确定性 broad-phase 和完整 candidate
验证；采样结果不得计为 post-strict PASS。

## 5. TDD 与验证

先新增引用尚不存在 `MeshRepairPreflight.h` 的测试，定向构建按预期以 `C1083` 失败；实现后实际执行：

```text
cmake --build build --config Debug --target mesh_repair_preflight_unit_tests：PASS；
.\build\Debug\mesh_repair_preflight_unit_tests.exe：PASS；
ctest --test-dir build -C Debug -R mesh_repair_preflight --output-on-failure：2/2 PASS；
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r1_pre_repair_baseline.ps1 -BuildDir build -Config Debug：4/4 case、每 case 2 次重复性 PASS；
cmake --build build --config Debug：PASS；
ctest --test-dir build -C Debug --output-on-failure：26/26 PASS；
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test：PASS；
scripts/run_ci_quick.ps1：未完成，外层命令在 240 秒达到超时，已完成 build 与部分 quick regression，不能记为 PASS。
```

## 6. 阶段结论

R1-01 至 R1-04 已完成，R1 Gate 关闭。下一允许原子任务为
`12E-08C-R2-01 Degenerate/Duplicate Cleanup`。R2-02..04、R3 和 12E-08D 继续按顺序阻断；本结果不代表
真实 OBJ 已修复或 global pipeline 已获得生产准入。

## 7. 安全边界

```text
不修改 legacy；
不执行 repair/post-strict；
不写 production TIFF/package；
OpenVDB 保持 optional/OFF；
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print 不变。
```
