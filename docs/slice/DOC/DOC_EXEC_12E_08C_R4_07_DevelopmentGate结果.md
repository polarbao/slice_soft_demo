# DOC_EXEC_12E-08C-R4-07 Development Gate 结果

> 文档状态：DEVELOPMENT IMPLEMENTATION COMPLETE / FINAL REQUIRED FAMILY 0/3  
> 日期：2026-07-22  
> 原子任务：12E-08C-R4-07 Development Gate

## 1. 结论

按 `DOC_DECISION_12E_08C_R4_07_开发准入放宽规则.md`，R4-07 已使用 `model` 目录中通过 R4-06 intake 的
clean OBJ 开始开发。两个 development candidate admitted，四 case global partition、texture transfer、
raster mapping、full closure、Release 三次测量和 legacy TIFF/RIP 回归全部通过。

该结果是开发 Gate PASS，不是最终 required-family PASS。真实族矩阵仍为 `0/3`，生产准入继续阻断。

## 2. 实现内容

```text
RepairedAssetIntakeService：
  新增 development_model_pool；
  只接收 model 目录中的 strict PASS 原始资产；
  保留完整审计、资源、hash 和 repeatability 条件。

R4-06 自动化：
  保留爱神/玫瑰/梯田 0/3 负向矩阵；
  新增 xiao_ma/yecan 正向 development intake；
  输出 development_gate_matrix.json。

R4-07 自动化：
  读取 development intake identity/hash；
  复用 R4-05 width sweep；
  执行 warm-up + 3 次 Release 测量；
  检查 partition/texture/raster/full closure 不变量；
  运行 repair-disabled TIFF SHA-256 与 RIP strict 回归；
  不写 global production package。
```

## 3. Development Intake

| Candidate | 模型 | Intake | 重复审计 | 生产输出 |
|---|---|---|---|---|
| `development_xiao_ma_damuzhi` | `xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | admitted | PASS | false |
| `development_yecan_3` | `yecan/3.obj` | admitted | PASS | false |

## 4. Four-case 结果

统一使用 Release、默认 OpenVDB OFF、`voxelMm=0.20`。`globalCoreMs` 包含分区、纹理传递、raster 和 full
closure，不包含 JSON/TIFF/PNG 写盘。

| Case | Width | 三次中位数 | 三次最大值 | 峰值工作集 | 结果 |
|---|---:|---:|---:|---:|---|
| `development_xiao_ma_minimum` | 0.40 mm | 279.5174 ms | 292.7601 ms | 26,574,848 B | PASS |
| `development_xiao_ma_all_texture` | 0.46 mm | 290.1907 ms | 308.8851 ms | 26,402,816 B | PASS |
| `development_yecan_intermediate` | 0.41 mm | 335.2982 ms | 351.4129 ms | 31,502,336 B | PASS |
| `texture2d_3mf_control` | 0.40 mm | 12.5352 ms | 13.7914 ms | 7,303,168 B | PASS |

四 case 均满足：

```text
TextureSurface ∩ ModelFill = empty；
TextureSurface ∪ ModelFill = Model；
overlap/unassigned/outside=0；
texture transfer sampledTextureCount>0；
raster partition PASS；
full closure expected-domain gap=0；
semantic channel mismatch=0；
productionOutputWritten=false。
```

## 5. Legacy 与协议回归

```text
Repair Disabled TIFF SHA-256 invariant：PASS；
RIP Reader strict：PASS；
schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
production package：仅 legacy 测试 fixture，未写 global production package。
```

## 6. 验证命令

```powershell
cmake --build build --config Release --target repaired_asset_intake repaired_asset_intake_unit_tests
ctest --test-dir build -C Release -R "^repaired_asset_intake_unit_tests$" --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_06_repaired_asset_intake.ps1 -BuildDir build -Config Release -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_development_gate.ps1 -BuildDir build -Config Release
```

实际结果：定向 CTest `3/3 PASS`，R4-06 development intake `2/2 admitted`，R4-07 four-case `4/4 PASS`，
legacy TIFF/RIP PASS。

## 7. 后续 Gate

```text
R4-07 development implementation：COMPLETE；
R4-07 final required-family acceptance：WAIT 3/3；
Release production budget：NOT FROZEN；
R4-08：WAIT FINAL REQUIRED-FAMILY GATE；
12E-08D：BLOCKED。
```

本地证据位于：

```text
output/benchmarks/12e_08c_r4_06_repaired_asset_intake/development_gate_matrix.json
output/benchmarks/12e_08c_r4_07_development_gate/four_case_development_summary.json
```
