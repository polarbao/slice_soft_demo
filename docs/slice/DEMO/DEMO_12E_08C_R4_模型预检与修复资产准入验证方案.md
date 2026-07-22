# DEMO_12E-08C-R4 模型预检与修复资产准入验证方案

> 文档版本：v0.2
> 文档状态：DEMO / PREPARED
> 日期：2026-07-21

## 1. 验证目标

证明模型导入预检不会被一键入口绕过、模式准入符合能力边界、正常模型可推进 12E 分区，以及修复资产
只有在身份、属性和 post-strict 全部通过后才可解除 required blocker。

## 2. Fixture 矩阵

| Case | 类型 | legacy | global | 用途 |
|---|---|---|---|---|
| generated_clean_textured_obj | 闭合彩色 OBJ | PASS | PASS | 正向 width/fill |
| three_mf_texture2d_checker | 闭合 3MF | PASS | PASS | 已有正向基线 |
| open_boundary_obj | 开放边 | WARN/按 fatal 规则 | BLOCK | 模式差异 |
| non_manifold_obj | 非流形 | WARN/按 fatal 规则 | BLOCK | 模式差异 |
| self_intersection_obj | 自相交 | WARN | BLOCK | fail-closed |
| missing_texture_obj | 资源缺失 | 按 fallback 规则 | 按 fallback 规则 | 资源策略 |
| non_finite_obj | 非有限坐标 | BLOCK | BLOCK | 导入 fatal |
| nai_you original | 历史负向 OBJ | WARN | BLOCK | 保留历史 blocker 回归 |
| aishen/meigui/titian family original | required 真实 OBJ 族 | WARN | BLOCK | 当前 family blocker |
| admitted required family candidates | 原始 PASS/外部修复/独立重建 | 待审计 | 必须 strict PASS | R4-06 后输入 |

正常 fixture 只增加正向覆盖，不能替代 required family。

## 3. 一键 UI 流程

### Case A：传统切片

```text
导入 self-intersection OBJ；
等待 preflight 完成；
UI 显示传统模式警告和全局模式阻断；
确认传统切片动作只在 legacyAdmission 非 blocked 时启动；
输出记录 effective mode=legacy。
```

### Case B：全局切片

```text
导入相同模型；
选择 global_surface_shell；
UI 显示稳定 blocker；
切片进程不启动；
productionOutputWritten=false；
不存在 legacy fallback package。
```

### Case C：结果过期

修改姿态、缩放、模型/MTL/贴图或预检选项后，旧结果立即变为 stale；再次点击必须先重跑预检。

## 4. Width 与全纹理验证

对两个 clean cases 至少验证：

```text
requested=0.10mm；
requested=minimum 与 allTextureThreshold 的中点；
requested=allTextureThresholdMm。
```

断言：

```text
effectiveMinimum=max(0.10, 2 * classification resolution)；
texture coverage 单调非递减；
fill coverage 单调非递增；
texture ∩ fill = empty；
texture ∪ fill = model；
allTexture 时 fill=0、texture=model、unassigned=0。
```

## 5. Model Fill 材料验证

对非 allTexture 中点分别选择：

```text
white -> W；
varnish -> V；
rgb/custom -> RGB；
C/M/Y/K material role -> active profile resolved RGB/W/V；
未配置 role -> UI 禁用或 validation blocked，并显示原因。
```

断言 TIFF 协议仍为 R G B W S V，不产生 C/M/Y/K 新通道。R4 阶段 global 只验证 diagnostic composer；
生产 TIFF 验证留给 08D。

## 6. 修复资产审计

每个 required family candidate 必须验证：

```text
familyId 和 candidateKind 保留；
原/新 source hash 同时存在；
修复来源和工具版本存在；
bounds/scale/component 差异在批准阈值；
UV/material/texture provenance PASS；
完整自相交为 complete_no_intersection；
post-strict PASS；
两次运行 stable projection 一致。
```

任一断言失败时 `admittedForGlobal=false`。

## 7. Release 与回归

```text
Fast check、full preflight、repair audit、global core、writer 分开计时；
legacy repair OFF TIFF SHA-256 invariant；
RIP strict；
默认 OpenVDB OFF；
OpenVDB ON 作为 conformance lane，不作为产品模式；
Quick CI baseline 必须有明确 PASS 或批准记录。
```

## 8. 完成判定

R4-01..05 可在没有 admitted required family assets 时完成。R4-06 服务合同可先完成；R4-07..08 只有在
爱神、玫瑰、梯田三个 family 各至少一个候选通过上述验证后完成。否则阶段状态为
`INTAKE IMPLEMENTED / REAL FAMILY BLOCKED`，不能输出 08D GO。
