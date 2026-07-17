# ROADMAP_12E 全局纹理壳层与模型填充分阶段路线

> 文档状态：ROADMAP / Stage 12E Planning
> 日期：2026-07-16
> 当前项目执行阶段：12D COMPLETE；12E-01/02/03/04 COMPLETE；12E-05 PREPARED

## 1. Goal

把 12A 已定义但未形成完整组合的 `Texture Surface Layer` 和 `Model Fill Layer` 收敛为全模型三维互补分区，并通过动态宽度、UI、report、closure 和 regression 形成可执行闭环。

## 2. Activation Gate

```text
1. 12D-R0/R1/R2/R3 已完成；
2. 12E-R0 的 Config/DTO、report schema、fixture matrix 和启动状态准备已完成；
3. 12E-01/02/03/04 已完成；当前没有 active code task，只有用户明确指定 12E-05 后才能执行；
4. 12E composer/production 接入需要已完成的 12D semantic_masks exact contract；
5. 不要求 12D repair R3 完成后才能做 12E R1 算法原型，但 production admission 必须重新确认；
6. 12E-01 已完成配置、DTO、稳定错误码和 unavailable report 骨架；未接入 production generation。
```

## 3. R0：契约与准入

输出：

```text
12E Decision/PRD/DEV/DEMO/ROADMAP/TASKS/CODEX_PROMPT；
global_surface_shell config schema；
texture/fill partition DTO；
slicesoft.texture_fill_partition.12e.1 report schema；
Config/DTO 准备文档和 fixture/验收矩阵；
minimum/dynamic maximum/allTexture contract；
backend role decision。
```

退出标准：

```text
不变量、配置兼容、fail policy、UI 字段和验证矩阵冻结；
global_surface_shell 在 backend 未实现时于写包前明确阻断；
默认 USE_OPENVDB=OFF 不改变。
```

## 4. R1：全局 3D 分类原型

输出：

```text
backend-neutral GlobalTextureFillPartitionService；
完整 3D occupancy/distance/closest-surface contract；
Legacy CPU production candidate prototype；
OpenVDB optional conformance adapter；
box/sphere/thin-wall/cavity/non-manifold fixtures；
性能和内存 baseline。
```

退出标准：

```text
partition invariants PASS；
width sweep 单调；
全纹理阈值成立；
global 3D 几何证据通过；
任何 backend 都不写 production TIFF。
```

## 5. R2：Composer 与 Closure 接入

输出：

```text
TextureSurfaceMask3D/ModelFillMask3D layer view；
最近表面纹理传递；
semantic composer exact masks；
texture_fill_partition_report；
12D material closure 联合诊断；
diagnostic-only package comparison。
```

退出标准：

```text
texture + fill = model；
overlap/unassigned = 0；
allTexture 合法 fill=0；
旧 Profile 默认输出保持兼容；
尚未通过 production gate 的结果不标记 production-safe。
```

## 6. R3：UI 与 Production Admission

输出：

```text
Qt 全局纹理策略；
0.01 mm slider/spinbox；
动态 min/max/allTexture threshold；
coverage/partition/diagnostics；
session effective config；
新 Profile candidate；
production admission decision。
```

退出标准：

```text
UI self-test/smoke 通过；
默认 OFF lane 通过；
RIP strict 和 RGBWSV 协议通过；
性能、内存、拓扑、纹理传递和 closure gate 全部满足；
未满足时保持 diagnostic-only。
```

## 7. R4：真实模型回归与收口

输出：

```text
真实 OBJ/3MF matrix；
minimum/intermediate/allTexture golden；
Release runtime/peak memory report；
用户手册同步；
REPORT_12E。
```

退出标准：

```text
典型甲片、薄壁、内腔、纹理缺失和 topology blocker 均有证据；
完成报告区分 Current/Target/Historical/Pending；
明确 production admitted 或 keep diagnostic。
```

## 8. Dependencies

```text
12A：ModelFill/TextureSurface/Support/Varnish 语义；
12B-R2：OpenVDB optional SDF utility 角色；
12C：Profile/effective config/Preview/Diagnostics UI 框架；
12D：semantic mask exact 和 material closure 诊断；
Architecture：core 不依赖 Qt，UI 不访问 OpenVDB 内部类型。
```

## 9. Stop Conditions

出现以下情况必须停止 production 推进并保留 diagnostic-only：

```text
没有默认 OFF production candidate；
partition overlap/unassigned 非 0；
global 3D 几何验证失败；
strict topology blocker 被绕过；
全纹理模式无法证明 model 全覆盖；
性能/内存未形成实际 baseline；
UI effective config 与核心实际值不一致；
RGBWSV 或 RIP strict 回归失败。
```

## 10. Rollback

```text
global_surface_shell 必须显式启用；
旧 Profile 和旧 config parser 默认行为保留；
新 report/preview 缺失不影响旧路径；
UI 新控件可按 capability 隐藏；
OpenVDB 保持 optional/OFF；
production gate 失败时不写生产包。
```
