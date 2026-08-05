# REPORT_03E-02 TIFF 生产压缩协议与 RIP 兼容当前状态

> 状态：INTERNAL COMPLETE / EXTERNAL RIP SUPPORT CONFIRMED / **GO_ON_DEMAND**（2026-08-04 由 `NO_GO_DEFAULT` 转入，见 §5.1）
> 日期：2026-08-03
> 前置：03E-01 PackBits 原型与读写性能矩阵 COMPLETE

## 1. 本阶段目标

03E-02 把 03E-01 的 PackBits 原型接入可审计的生产配置链路，同时保持默认未压缩：

```text
Config -> Legacy / Global / Scene Pipeline -> shared RGBWSV Writer
       -> manifest compression -> strict Reader -> TIFF native preview
```

本阶段不引入 Deflate，不改变 RGBWSV 通道、位深、极性或生产默认 Writer。

## 2. 已实现合同

### 2.1 配置

```json
"output": {
  "tiffCompression": {
    "algorithm": "none"
  }
}
```

支持 `none` 与 `packbits`。字段缺省时保持 `none`；其他值在配置加载或 Qt 生效配置生成时失败关闭。

### 2.2 Package 与 Reader

- 新 package 显式写入 `manifest.tiff.compression`；
- TIFF `Compression(259)` 必须与 manifest 一致；
- 项目严格 Reader 摘要输出 `compression`；
- `E_TIFF_COMPRESSION_INVALID`：manifest 算法未知或类型错误；
- `E_TIFF_COMPRESSION_MISMATCH`：manifest 与实际 TIFF 不一致；
- 历史 `p0.rgbwsv.2` 未声明 compression 时按 `none` 读取。

### 2.3 Pipeline 覆盖

已接入并验证：

```text
Legacy 单模型
Global Surface Shell 显式候选
Multi-model Scene 共享写包
OpenVDB Candidate 显式非默认路径
TIFF native Layer Preview
```

OpenVDB 仍默认关闭；本阶段没有改变其生产准入规则。

### 2.4 Qt UI

配置页 TIFF 输出区域新增：

```text
不压缩（默认） -> none
PackBits（实验） -> packbits
```

PackBits 是无损固定算法，没有压缩等级。UI 不提供“压缩比例”或 `compressionLevel`。

## 3. 自动验证

### 3.1 双 Writer 合同

handwritten 与 LibTIFF Release 构建均通过：

```text
output_resolution_config_unit_tests
rgbwsv_production_package_writer_unit_tests
tiff_layer_source_unit_tests
non_square_raster_pipeline_unit_tests
global_surface_shell_production_pipeline_unit_tests
multi_model_production_service_unit_tests
```

覆盖配置缺省、PackBits exact bytes、manifest/tag 一致性、Legacy/Global/Scene 传播、历史省略字段、
未知算法和压缩不一致错误码。

### 3.2 Qt

```text
generated-effective-config: PASS
compression=packbits
```

该 Smoke 验证 UI 选项、文档写回、生效配置传播，以及 `deflate` 失败关闭。

### 3.3 03E-02 生产 Gate

入口：

```powershell
.\scripts\Run03EProductionCompressionGate.ps1 -Config Release
```

Gate 对 deterministic fixture 和真实 `model/obj/meigui_fudiao/04.obj` 分别生成
handwritten/LibTIFF × none/PackBits package，检查 TIFF tag、manifest、项目严格 RIP 和坏包错误码。

证据：

```text
output/benchmarks/03e_02/20260803_131354_875/production_compression_gate.json
```

| 用例 | Writer | none TIFF 字节 | PackBits TIFF 字节 | 体积缩减 | 完整写包 wall 变化 |
|---|---|---:|---:|---:|---:|
| deterministic_small | handwritten | 143000 | 142080 | 0.643% | -5.952% |
| deterministic_small | LibTIFF | 143020 | 142100 | 0.643% | -1.059% |
| real_meigui_04 | handwritten | 236061644 | 92350141 | 60.879% | -29.083% |
| real_meigui_04 | LibTIFF | 236061826 | 92349666 | 60.879% | +27.334% |

真实模型说明 PackBits 可显著减少以空白为主的 RGBWSV TIFF 体积，但不能稳定缩短完整切片写包时间。
完整流程 wall 为单次观测，不作为替代 03E-01 p50 矩阵的精密性能结论。

### 3.4 03D 回归

`Run03DTiffCompatibilityGate.ps1 -Config Release` 通过，none、stripped/tiled、双 Writer、共享 Package、
项目严格 RIP 和历史坏包矩阵未回归。

## 4. 未完成项

以下外部验证未在本机会话执行：

```text
Photoshop 打开 PackBits 六通道 TIFF
目标 RIP 读取完整 PackBits package
打印控制软件导入与打印前检查
```

因此不能声称目标 RIP 兼容，也不能默认启用 PackBits。

## 5. 当前结论（v2 · 2026-08-04 更新）

```text
内部生产配置与协议：PASS
项目严格 Reader/RIP：PASS
真实 OBJ package：PASS
外部目标 RIP 声明支持：CONFIRMED   ← 2026-08-04 新增
外部目标 RIP 实机互操作：PENDING   （由 Stage 14 的 14F 三方联调关闭）
默认压缩：none（未改变）
默认 Writer：handwritten（未改变）
决策：GO_ON_DEMAND —— 按需显式开启已获授权；默认值维持 none
```

### 5.1 由 NO_GO_DEFAULT 转 GO_ON_DEMAND 的依据（2026-08-04）

RIP 侧在 `DOC_CHECKLIST_14` 第一轮回复中明确确认：

```text
Q4.1  支持 Compression = PackBits (32773)   ✅
Q4.2  优先使用 libtiff 处理，并采用压缩
Q4.3  建议默认开启
```

第二轮（R5）就落地节奏达成一致，**我方取「分两步」**：

| 步骤 | 内容 | 状态 |
|---|---|---|
| 第一步（本次） | 保持 `output.tiffCompression` 默认 `none`，**按需显式开启已获授权** | ✅ 生效 |
| 第二步（独立决策） | 是否改为默认开启 | ⬜ 未启动 |

**为什么不直接默认开启**：改默认会使**全部既有 golden TIFF 的 SHA-256 失效**，需重新固化基线，
且很可能需同期把 LibTIFF 由 `GO_OPTIONAL` 切为默认后端。对方已确认两种节奏均可接受
（并接受我方为此所需的一个独立验收轮次），故取风险较低者。

第二步若启动，应与 `tiff_io.cpp` 字对齐缺陷、LibTIFF 默认后端切换合并为同一决策，避免多次重固化基线。

### 5.2 残留边界

`CONFIRMED` 仅指对方**声明**支持 PackBits，不等同实机互操作已验证。
实机 Gate 由 Stage 14 的 14F 三方联调关闭；在此之前不得宣称外部互操作已通过。

> 权威条款见 `docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md` §1.5。
