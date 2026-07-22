# DOC_MATRIX_12E 全局纹理填充分区验收矩阵

> 文档状态：12E-08C-R3 COMPLETE / R3-04 NO-GO / R4-01..06 IMPLEMENTATION COMPLETE / FAMILY 0/3 BLOCKED
> 日期：2026-07-22

## 1. 使用方式

本矩阵把 12E-01 至 12E-10 的证据拆成配置、几何、分区、纹理、闭环、UI、协议和性能层。任务只有在对应行具有可复现 fixture、命令和结果后才能标记完成。

## 2. 契约与负向用例

| Case | 阶段 | 断言 | 失败条件 |
|---|---|---|---|
| legacy config omitted | 12E-01 | 旧字段默认和输出不变 | 自动迁移到 global shell |
| valid global config | 12E-01 | DTO 完整；backend unavailable 明确阻断 | 静默 fallback |
| invalid width | 12E-01 | 非有限/0/负数拒绝 | clamp 后继续写包 |
| invalid step | 12E-01 | 非 0.01 拒绝 | 接受任意步长 |
| invalid enum | 12E-01 | geometry/policy/scope 稳定错误码 | 仅自然语言且不可稳定断言 |
| mismatched pair | 12E-01 | texture/fill scope 必须成对 | 独立启用造成未分配模型 |
| model fill disabled | 12E-01 | 明确拒绝 | 用 disabled 冒充 allTexture |
| backend unavailable | 12E-01/02 | blocked/unavailable；不写 package | 输出 pass 或生产 TIFF |
| OpenVDB OFF | 12E-04 | stable `E_12E_OPENVDB_BACKEND_UNAVAILABLE`；CPU 独立 PASS | 强制链接 OpenVDB 或异常退出 |
| OpenVDB topology blocker | 12E-04 | level-set 构建前 stable blocker | 先构建后失败或静默 fallback |
| backend exception | 12E-02 | stable `E_12E_PARTITION_BACKEND_FAILED`；不越过 diagnostic 边界 | 异常退出或继续写包 |
| requested/backend grid mismatch | 12E-02 | stable mask-size error | 接受错位 mask |
| non-binary mask | 12E-02 | stable binary error | 非 0/1 数据参与统计 |
| missing CPU mesh / invalid grid | 12E-03 | stable CPU blocker | 异常退出或空 mask 伪 PASS |
| width below effective minimum | 12E-03 | stable width blocker | 静默 clamp 后标记原请求成功 |

## 3. Generated Geometry

| Fixture | 重点 | 必须断言 |
|---|---|---|
| closed box | 基本 inside/distance | 12E-03 PASS：union=model、overlap=0、unassigned=0 |
| sphere/sloped body | 三维距离 | 12E-03 generated sphere 与 octahedron PASS：最近三角形欧氏距离生效 |
| thin wall | 双侧壳层相遇 | 12E-03 PASS：fill=0、无重叠 |
| closed cavity | 内外闭合表面 | 12E-03 PASS：中心 cavity outside、壳体 inside |
| concave body | 凹面最近表面 | 不退化为逐层 morphology |
| multi-surface tie | 中轴 tie | 结果确定、tie 计数可报告 |
| open mesh | 拓扑门禁 | 12E-03 stable strict blocker |
| non-manifold | 拓扑门禁 | 12E-03 stable strict blocker |
| self-intersection | 拓扑门禁 | 12E-03 stable strict blocker |

## 4. Width Sweep

对每个可用闭合 fixture：

```text
w0 = effective minimum；
w1/w2/w3 = 中间采样；
w4 = allTexture threshold。
```

必须满足：

```text
texture[i+1] >= texture[i]；
fill[i+1] <= fill[i]；
model count 不变；
overlap=0；
unassigned=0；
w4: texture=model, fill=0, allTexture=true。
```

## 5. Backend Matrix

| Lane | 角色 | 必须结果 |
|---|---|---|
| `USE_OPENVDB=OFF` | 默认 CPU candidate | 可独立 build/test；不依赖 OpenVDB |
| `USE_OPENVDB=ON` | conformance candidate | 12E-04 PASS：同 request grid、8 个 fixture、差异 DTO；不自动 production admitted |
| backend unavailable | safety | stable error/report；不 fallback |

12E-05 已补充代表性宽度扫描和可选 full-step scan；默认 OFF 与 OpenVDB ON targeted CTest
均通过，成功报告只序列化 validated diagnostic result。

CPU 与 OpenVDB 比较 occupancy、partition count、threshold、distance error、runtime 和 peak memory，不要求位级一致。

## 6. Texture Transfer

| Input | 断言 |
|---|---|
| OBJ/MTL/PNG | 12E-06 PASS：closest surface UV 采样、outsideColored=0、nearestQueryCount=0 |
| OBJ missing UV | 12E-06 PASS：warn fallback 与 fail-fast 稳定错误 |
| OBJ missing texture | 12E-06 PASS：fallback、missingTexture 和 sample failure 统计 |
| 3MF Texture2D | 12E-06 PASS：与 OBJ 共享 AdaptedTriangleMesh service |
| 3MF ColorGroup | 12E-06 PASS：material diffuse，不误报 missing texture |
| multiple surface tie | 12E-06 PASS：稳定 triangle 选择与 tie count |
| diagnostic composer white/varnish/rgb | 12E-06 PASS：fill 仅写 W/V/RGB；S=255 |

## 7. 12D Closure 联动

| Case | 断言 |
|---|---|
| normal width | 12E-07 PASS：12D 读取 exact texture/fill mask；ColorFillGap=0 |
| all texture | 12E-07 PASS：fill=0；not_applicable(reason=all_texture_partition) |
| repair disabled | 12E-07 PASS：RIP strict；30 层 TIFF SHA-256 invariant |
| support/varnish boundary | 12E-08B PASS：真实 semantic sidecar、S/V 通道一致性和独立 closure status |
| internal void support | 12E-08B PASS：位于 model envelope 内、model 外并归属 support-required domain |
| full expected domain | 12E-08B PASS：model envelope/support required/outer varnish 应占用像素不得为空 |
| final priority | 12E-08B PASS：Model > OuterVarnishShell > Support > Empty 冲突 fail fast |

12E 不修改 12D repair 规则，12D-R3 是否完成不阻塞 12E R0/R1 原型，但 production admission 必须重新评估闭环证据。

## 8. UI 与 Effective Config

| Case | 断言 |
|---|---|
| no model | width 控件 pending/disabled，无虚假最大值 |
| model preflight | min/max/threshold 动态刷新 |
| slider/spinbox | 双向同步，步长 0.01 mm |
| model changed | requested 值 clamp 并记录 requested/effective |
| allTexture | fill material 配置保留，coverage=0 |
| backend status | 普通用户不选 backend；诊断区显示能力 |
| preview | Texture Surface / Model Fill / Partition 使用真实 layerIndex |

## 9. 真实模型矩阵

优先仓库内可复现模型：

| 模型 | 重点 |
|---|---|
| `model/obj/nai_you_new` | 标准甲片、宽度 sweep、支撑/闭环 |
| `model/obj/aishen_fudiao` | 高 Z 浮雕、凹面、性能 |
| `model/obj/meigui_fudiao` | 复杂纹理、最近表面传递 |
| 仓库内真实 3MF fixture | Texture2D/ColorGroup 兼容 |

每个模型记录 config/model/texture SHA-256、grid、layerCount、partition、closure、RIP、runtime 和 peak memory。

## 10. 协议与回归

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
legacy Profile 默认输出不变；
RIP strict PASS；
OpenVDB OFF build PASS；
未通过 production admission 时只允许 diagnostic result。
```

## 11. 阶段 Gate

| Gate | 状态条件 |
|---|---|
| 12E-01 -> 02 | Config/DTO/negative tests 完成 |
| 12E-02 -> 03 | COMPLETE：backend-neutral invariants 骨架完成 |
| 12E-03 -> 04 | COMPLETE：CPU generated fixture 正确性与 Debug 基线完成 |
| 12E-04 -> 05 | COMPLETE：OFF/ON conformance、cavity parity、差异 DTO 可复现 |
| 12E-05 -> 06 | COMPLETE：schema、monotonic sweep、endpoint 与 golden 已冻结 |
| 12E-06 -> 07 | COMPLETE：exact masks/texture transfer/diagnostic composer 与 report golden 完成 |
| 12E-07 -> 08 | COMPLETE：texture_model_fill_only exact closure 联动通过 |
| 12E-08A | COMPLETE：world-space raster center 映射、互补 mask、RGB、量化与 coverage 证据通过 |
| 12E-08B | COMPLETE：full-material sidecar、五类 12D gap、support/varnish 状态与通道一致性通过 |
| 12E-08C | COMPLETE：默认 OFF Release/回归证据已生成；3 个真实 OBJ topology BLOCKED，预算未冻结 |
| 12E-08C-R1/R2/R3 | COMPLETE / R3-04 NO-GO：显式 repair-then-strict、属性保持、post strict 和真实模型 Release Gate |
| 12E-08C-R4 | PREPARED：导入预检、模式准入、正常模型正向链和修复资产接收审计 |
| 12E-08D | BLOCKED：生产准入需前置证据和用户再次确认 |
| 12E-09 | PREPARED：09A READY；09B 被 08D 阻断 |
| 12E-10 | UI、真实模型、RIP 和报告收口 |

## 12. 双切片模式与输出矩阵

| 配置/状态 | Effective Mode | Production TIFF | Preview/Report | 断言 |
|---|---|---|---|---|
| `slicePipeline` 缺失 | legacy | 成功时必须完整 | 保留 | 历史兼容 |
| `mode=legacy` | legacy | 成功时必须完整 | 保留 | 现有结果不变 |
| `mode=global_surface_shell`，diagnostic | global_surface_shell | 禁止 | 允许诊断输出 | 不宣称生产成功 |
| global blocked/unavailable | global_surface_shell | 禁止 | 记录错误 | 无 silent fallback |
| global admitted | global_surface_shell | 成功时必须完整 | 保留 | 与 legacy 共用 writer/RIP |
| 非法 mode | 无 | 禁止 | config error | fail-fast |

两条 production PASS 行统一要求 `p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print`，并验证 manifest
layer list、TIFF 文件、同层 preview、通道统计和 RIP strict。12E-08D-01..04 在修复 Gate 通过后执行；
12E-09B 才开放 UI 双模式生产选择。

## 13. R4 模型预检与资产准入矩阵

| Case | legacyAdmission | globalAdmission | 切片动作 | 生产影响 |
|---|---|---|---|---|
| valid closed textured OBJ/3MF | PASS | PASS | 可进入各自当前能力 | global 08D 前仍诊断 |
| invalid/empty/non-finite | BLOCK | BLOCK | 停止 | 不写包 |
| confirmed self-intersection | WARNING | BLOCK | legacy 可兼容；global 停止 | 不解除 global Gate |
| boundary/non-manifold/winding ambiguity | WARNING/既有 fatal | BLOCK | 按模式 | 不 silent fallback |
| preflight missing/stale | BLOCK CURRENT ACTION | BLOCK CURRENT ACTION | 先重跑检测 | 不写包 |
| repaired required asset attribute mismatch | 不覆盖原证据 | BLOCK | 停止 intake | required 仍 FAIL |

正向 width/material 矩阵必须覆盖推荐 0.10mm、中间值和动态 allTextureThreshold。C/M/Y/K 作为
MaterialProcessProfile role 解析到 RGBWSV，不新增协议通道；正常模型结果不能替代 required OBJ 行。
