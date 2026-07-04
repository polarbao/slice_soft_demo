# DEMO_12B_切片引擎性能验证方案

> 文档版本：v0.1
> 文档状态：DEMO / Stage 12B
> 生成日期：2026-07-05

---

## 1. 验证目标

用可复现 benchmark 判断：

```text
1. 切片慢到底慢在 core 还是 I/O；
2. OpenVDB 是否真的有提速；
3. legacy 优化或 heightfield fast path 是否更适合甲片模型；
4. OpenVDB 是否只能作为 SDF 工具模块。
```

---

## 2. 验证 Case

| Case | 模型 | 目标 |
|---|---|---|
| 12B-01 | `model/obj/nai_you_new` | 标准彩色纹理甲片 |
| 12B-02 | `model/obj/aishen_fudiao` | 不规则浮雕与高 Z 局部 |
| 12B-03 | 单材料 relief | 单材料基线 |
| 12B-04 | 大尺寸层数 fixture | I/O 与 core 分离 |
| 12B-05 | OpenVDB closed mesh fixture | OpenVDB 最理想输入 |

---

## 3. 指标

必须记录：

```text
importMs
coreComputeMs
materialComposeMs
ioWriteMs
previewWriteMs
reportWriteMs
endToEndMs
peakMemoryMb
modelPixels
supportPixels
semanticHash
outputSemanticsComparable
replacementPass
```

---

## 4. 验证矩阵

| Build | Engine | Image Write | 用途 |
|---|---|---|---|
| Debug | legacy | off | 功能调试 |
| Debug | openvdb | off | 功能调试 |
| Release | legacy | off | core 性能基准 |
| Release | openvdb | off | OpenVDB core 性能 |
| Release | legacy | on | 端到端用户耗时 |
| Release | openvdb | on | candidate 端到端耗时 |

---

## 5. 通过标准

OpenVDB 替代 legacy 需满足：

```text
outputSemanticsComparable=true
core ratio <= 1.2
支撑/纹理/填充/光油语义一致
真实模型至少 3 个 case 通过
OpenVDB OFF 构建不受影响
```

如果不满足：

```text
OpenVDB 保持 candidate / SDF utility；
legacy 或 heightfield fast path 继续作为生产优化主线。
```
