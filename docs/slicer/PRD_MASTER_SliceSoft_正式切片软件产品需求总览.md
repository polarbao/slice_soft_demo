# PRD_MASTER_SliceSoft_正式切片软件产品需求总览

> 文档版本：v0.1
> 文档类型：产品级总 PRD
> 适用项目：Slice Soft / UV 彩色多材料切片软件
> 当前基线：`spike/09B-R1-real-model-shell-texture`
> 当前阶段判断：09B-R2 进行中，尚未进入 production OpenVDB 接入
> 建议提交目录：`docs/slicer/`

---

## 1. 文档目的

本 PRD 是当前切片软件项目的产品级总需求文档，用于统一此前 P0、03、04、05、06、07、08、09、R0/R1/R2 等阶段文档中分散的需求。

本文件回答：

```text
1. 当前切片软件的产品目标是什么；
2. 面向哪些角色和使用场景；
3. 已完成哪些产品能力；
4. 当前处于哪一阶段；
5. 后续能力如何拆分；
6. 哪些是 production 必需能力；
7. 哪些仍是 prototype / research；
8. RGBWSV、OpenVDB、纹理、支撑、光油、材料策略、UI、CI 如何统一规划。
```

---

## 2. 产品定位

本软件定位为：

```text
面向 UV / 彩色 / 多材料 3D 打印设备的桌面级切片与调试软件。
```

它不是单纯的模型查看器，也不是只输出普通几何层图的传统切片器，而是面向以下目标：

```text
1. 输入多来源模型：OBJ / MTL / PNG / 3MF / Texture2D / ColorGroup；
2. 生成可供 RIP 或设备前处理使用的 RGBWSV 多通道 TIFF 包；
3. 支持彩色纹理、白墨、支撑、光油、材料策略和工艺参数；
4. 支持 OpenVDB / SDF 几何内核逐步进入生产切片流程；
5. 提供 CLI、Qt Debug UI、配置/Profile、报告、回归测试和 CI 能力；
6. 支持后续设备联调、RIP 链路、喷头/通道/材料工艺验证。
```

---

## 3. 当前阶段总览

截至当前基线，项目已经完成：

```text
P0：单材料体素切片与 RGBWSV 前置数据生成
03/03B/03C：RGBWSV TIFF 协议、存储模式兼容、RIP Reader、回归脚本收口
04/04A：OBJ/MTL/PNG 彩色纹理与 fallback 收口
05/05A：材料策略与真实材料工艺参数验证
06/06A/06B：3MF、OBJ_MTL 多材料输入、Texture2D、ColorGroup
07/07A/07B：Qt Debug UI、参数编辑、Profile 可视化、UI smoke
R0/R1/R2：正式项目架构审查、核心模块边界重构、配置报告测试 CI 工程化
08/08A：支撑形态、桥接 fixture、真实 profile 收口
09/09A：OpenVDB / SDF 几何内核预研、依赖锁定、真实 smoke
09B：OpenVDB generated-box 表面壳层纹理原型
09B-R1：真实 OBJ/3MF 壳层纹理验证与 UV/material transfer
```

当前处于：

```text
09B-R2：真实模型鲁棒性、性能/内存与多材质策略收口
```

尚未进入：

```text
09P：OpenVDB production pipeline 接入设计
```

---

## 4. 产品用户角色

### 4.1 切片算法开发者

关注：

```text
几何内核
模型导入
层数据生成
OpenVDB/SDF
RGBWSV 输出
诊断报告
回归测试
```

需要：

```text
1. 能快速运行 CLI demo 和单元测试；
2. 能查看中间 mask、shell、support、varnish、texture transfer；
3. 能定位拓扑错误、纹理采样错误、材料映射错误；
4. 能通过 report 和 golden 判断改动是否破坏协议。
```

### 4.2 工艺工程师

关注：

```text
白墨
光油
支撑
材料工艺参数
壳层厚度
纹理策略
打印极性
```

需要：

```text
1. 能通过 Profile 调整材料参数；
2. 能看到每层/每通道输出；
3. 能看到白墨/光油/支撑/颜色之间的优先级；
4. 能通过 report 判断模型是否适合打印。
```

### 4.3 测试/QA

关注：

```text
样例覆盖
回归稳定性
schema
golden
CI
错误用例
```

需要：

```text
1. 能执行 quick / extended / benchmark 测试；
2. 能判断输出包是否兼容 RIP Reader；
3. 能验证错误模型和缺失纹理行为；
4. 能追踪每个阶段新增能力。
```

### 4.4 UI/应用层使用者

关注：

```text
模型导入
参数编辑
Profile 管理
预览
报告查看
任务执行
```

需要：

```text
1. Qt Debug UI 能显示配置、模型、预览、报告；
2. 能选择 Profile；
3. 能运行 smoke；
4. 能查看错误、warning、输出路径。
```

---

## 5. 总体产品能力地图

### 5.1 输入能力

| 能力 | 当前状态 | 后续目标 |
|---|---|---|
| STL 基础导入 | 已有历史基础 | 保持兼容 |
| OBJ 几何导入 | 已有 | 生产路径稳定化 |
| MTL 材质导入 | 已有 | 多材质策略收口 |
| PNG 纹理读取 | 已有 | 纹理采样策略收口 |
| 3MF 基础导入 | 已有 | 真实复杂模型验证 |
| 3MF ColorGroup | 已有 | production 材料映射 |
| 3MF Texture2D | 已有 | production 纹理壳层接入 |
| 多对象/多组件 | 基础支持 | 多对象优先级与报告收口 |

### 5.2 几何能力

| 能力 | 当前状态 | 后续目标 |
|---|---|---|
| Legacy layer mask | 已有 | 继续保留 |
| OpenVDB smoke | 已通过 | 保持依赖稳定 |
| OpenVDB meshToLevelSet | 已在 09B 使用 | 09B-R2 扩展真实模型 |
| SDF inside/shell/interior | 已在 09B 使用 | production 策略设计 |
| Mesh topology diagnostics | 基础版 | 09B-R2 扩展 |
| Nearest triangle BVH | 已有 | 统计与稳定 tie-break |
| Voxel/thickness matrix | 未完整 | 09B-R2 |
| Self-intersection/duplicate/local winding | 未完整 | 09B-R2 |

### 5.3 纹理能力

| 能力 | 当前状态 | 后续目标 |
|---|---|---|
| Full-volume texture | 已有 legacy | 保持兼容 |
| Texture fallback | 已有 | 策略收口 |
| 3MF Texture2D sampling | 已有基础 | 与壳层统一 |
| Shell texture prototype | 已完成 generated-box 与真实基础 | 09B-R2 鲁棒性 |
| UV seam 策略 | 未完整 | 09B-R2 |
| Multi material / multi texture | 未完整 | 09B-R2 |
| Production surface_shell_texture | 未接入 | 09P-R1/R2 |

### 5.4 材料能力

| 能力 | 当前状态 | 后续目标 |
|---|---|---|
| MaterialPolicy | 已有基础 | production compositing |
| MaterialRoleMapping | 已有 | 多材料映射 |
| MaterialProcessProfile | 已有 | 工艺参数联调 |
| 白墨 W | 已纳入 RGBWSV | 与真实工艺关联 |
| 支撑 S | 已纳入 RGBWSV | 支撑几何和 clearance |
| 光油 V | 已纳入 RGBWSV | SDF compensated varnish |
| Base / Shell / Support / Varnish role | 部分已有 | 统一 role 体系 |

### 5.5 输出能力

| 能力 | 当前状态 | 后续目标 |
|---|---|---|
| RGBWSV TIFF 输出 | 已有 | 保持协议冻结 |
| TIFF storage mode | 已有兼容 | 持续回归 |
| manifest | 已有 | production schema 扩展 |
| RIP Reader | 已有 | 持续兼容 |
| preview | 已有多类 | UI 整合 |
| report | 已有多 schema | Master report 统一 |
| golden | 已有基础 | OpenVDB real/golden 扩展 |

### 5.6 UI 与工程能力

| 能力 | 当前状态 | 后续目标 |
|---|---|---|
| Qt Debug UI | 已有基础 | production profile 预览 |
| 参数编辑 | 已有 | surface_shell 参数展示 |
| Profile 可视化 | 已有 | 材料工艺增强 |
| UI smoke | 已有 | OpenVDB report/preview 集成 |
| CI quick | 已有 | OpenVDB ON 扩展 |
| Benchmark | 初步规划 | 09B-R2 / 后续 |

---

## 6. 冻结协议与不可变要求

当前 RGBWSV 输出协议冻结为：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
priority = Model > Support > Empty
SupportType 不进入 TIFF channel
```

任何后续阶段不得在未建立协议升级阶段的情况下修改这些约束。

OpenVDB、支撑、光油、纹理壳层等新能力都必须映射到该协议，而不是隐式改协议。

---

## 7. 需求分层

### 7.1 Must Have：进入 production 前必须具备

```text
1. 输入：OBJ/MTL/PNG 与 3MF Texture2D/ColorGroup 可稳定导入；
2. 几何：OpenVDB/SDF 在真实模型上可生成可解释壳层；
3. 纹理：shell voxel 能稳定映射到源 triangle / UV / material；
4. 材料：RGB/W/S/V role 关系明确；
5. 输出：RGBWSV package 与 RIP Reader 兼容；
6. 配置：surface_shell_texture 配置可验证；
7. 报告：错误、fallback、topology、performance 可解释；
8. CI：legacy OFF 与 OpenVDB ON 均有测试；
9. UI：至少 Debug UI 可查看关键结果；
10. 失败策略：fail_fast / fallback / diagnostic_only 明确。
```

### 7.2 Should Have：production 初期应具备

```text
1. 多材质多纹理 seam 策略；
2. Release 性能/内存基线；
3. 真实业务模型 golden；
4. profile 管理；
5. UI overlay；
6. benchmark 非阻塞 CI；
7. report schema validator。
```

### 7.3 Could Have：后续增强

```text
1. 自动 mesh repair；
2. 更复杂的 seam blending；
3. 多材料真实混色模型；
4. 设备联动调试；
5. 工艺参数推荐；
6. 更完整的色彩管理。
```

---

## 8. 当前阶段与后续阶段

### 8.1 当前阶段：09B-R2

目标：

```text
真实模型鲁棒性、性能/内存与多材质策略收口。
```

退出条件：

```text
真实指甲 OBJ/3MF golden 通过；
复杂浮雕 fixture 通过；
多 material/texture 和 UV seam 策略稳定；
duplicate/self-intersection/local reversed face 有诊断；
10k+ triangle Release benchmark 完成；
主要内存对象可统计；
voxel/thickness matrix 完成；
OFF run_ci_quick 通过；
production RGBWSV 未修改。
```

### 8.2 下一阶段：09P

目标：

```text
OpenVDB production pipeline 接入设计。
```

输出：

```text
PRD_09P_OpenVDB生产Pipeline接入.md
DEV_09P_OpenVDB与LegacyPipeline融合设计.md
TASKS_09P
CODEX_PROMPT_09P
```

### 8.3 09P-R1：Experimental production path

目标：

```text
通过显式 feature flag，把 surface_shell_texture 接入 slicer_cli experimental path。
```

### 8.4 09P-R2：Production 收口

目标：

```text
配置、报告、golden、Qt UI、CI、RIP 兼容全部收口。
```

### 8.5 09C / 09D / 10

```text
09C：SDF compensated varnish prototype
09D：SDF support clearance / overhang diagnostics
10：RIP/设备/工艺联调阶段
```

---

## 9. Production 接入前置条件

OpenVDB production 接入必须满足：

```text
1. 09B-R2 通过；
2. 真实业务模型 golden 建立；
3. OpenVDB ON 构建稳定；
4. OFF legacy 构建稳定；
5. report schema 稳定；
6. texture/diffuse/fallback 统计可解释；
7. topology error 有 fail-fast 策略；
8. fallback 策略可配置；
9. RGBWSV package golden 可比较；
10. Qt Debug UI 至少能读取 report 与 preview。
```

---

## 10. 配置需求总览

未来 SliceConfig 建议增加但不默认启用：

```json
{
  "geometry_kernel": {
    "enabled": false,
    "engine": "openvdb",
    "voxel_size_mm": 0.05,
    "mesh_policy": "strict_closed",
    "failure_policy": "fail_fast"
  },
  "texture": {
    "enabled": true,
    "apply_mode": "surface_shell",
    "shell_thickness_mm": 0.10,
    "shell_region": "outer_surface",
    "fill_role": "base",
    "sampler": "bilinear",
    "uv_address_mode": "clamp",
    "fallback_rgb": [255, 255, 255],
    "max_transfer_distance_mm": 0.0
  }
}
```

旧配置必须继续可用：

```text
solid_volume_from_top_surface
top_surface_only
top_surface_band
```

---

## 11. 报告需求总览

最终 production report 应至少包含：

```text
input summary
config summary
model hash
config hash
mesh diagnostics
OpenVDB status
level set stats
shell/interior stats
texture transfer stats
material composition stats
support stats
varnish stats
fallback stats
performance
memory
warnings/errors
output package info
RIP compatibility info
```

---

## 12. UI 需求总览

Qt Debug UI 后续应支持：

```text
1. 选择模型/config/profile；
2. 运行 legacy 或 OpenVDB experimental path；
3. 查看输入模型摘要；
4. 查看 topology diagnostics；
5. 查看 shell/interior/support/varnish overlay；
6. 查看 texture transfer source counts；
7. 查看 report warnings/errors；
8. 查看 output package 和 RIP reader 结果；
9. 导出报告。
```

---

## 13. 测试与 CI 需求总览

建议分为：

```text
ci_quick_off：
  USE_OPENVDB=OFF
  legacy pipeline
  config/report/schema/golden
  rip reader

ci_openvdb_smoke：
  USE_OPENVDB=ON
  openvdb-smoke
  generated-box shell

ci_openvdb_real：
  real OBJ/3MF surface shell
  texture transfer
  negative fixtures

ci_openvdb_benchmark：
  Release performance
  memory
  non-blocking 或 scheduled
```

---

## 14. 非目标

当前产品路线短期不覆盖：

```text
设备实时控制
喷头波形
RIP 半色调
ICC 色彩管理
自动 mesh repair 生产方案
复杂多材料混色物理模型
云端任务管理
商业授权/用户系统
```

---

## 15. 总体结论

当前项目的阶段级 PRD/DEV 已经足够支撑各阶段开发，但 production 前缺少总控文档。

本 PRD 明确：

```text
09B-R2 是 production 接入资格验证阶段；
09P 是 production 接入设计阶段；
09P-R1/R2 才是 production experimental 与收口实现阶段。
```

在 09B-R2 完成前，不建议将 OpenVDB 壳层纹理直接写入 production RGBWSV。
