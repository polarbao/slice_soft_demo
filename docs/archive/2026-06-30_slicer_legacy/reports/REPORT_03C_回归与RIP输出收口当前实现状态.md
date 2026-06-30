# REPORT_03C_回归与RIP输出收口当前实现状态

> 日期：2026-06-08  
> 阶段：03C / REPORT_03B 后回归与 RIP 输出收口  
> 状态：已完成实现与验证

---

## 1. 本阶段目标

03C 是测试工具和工程效率收口阶段，不新增切片能力，不修改 03B TIFF storage 语义，也不修改 05 MaterialPolicy 语义。

本阶段完成：

- `run_regression.ps1` 支持 `-Mode quick|full|heavy`。
- `run_regression.ps1` 保留 `-SkipHeavyRelief` / `-SkipHeavyTexture`。
- `run_regression.ps1` 增加 `-SkipBuild`。
- regression case 拆分为 basic / storage / support / texture small / material policy / heavy relief / heavy texture。
- `rip_reader_test` 支持 `--summary`。
- `rip_reader_test` 支持 `--quiet`。
- 新增 RIP TIFF 兼容性检查清单。

---

## 2. run_regression 分层状态

### quick

默认模式，命令：

```powershell
.\scripts\run_regression.ps1 -Mode quick
```

覆盖：

- P0 basic。
- storage stripped default。
- storage tiled compatibility。
- support small cases。
- texture fallback small cases。
- MaterialPolicy 六个样例。
- bad packages。

不覆盖：

- heavy relief。
- heavy texture。

### full

命令：

```powershell
.\scripts\run_regression.ps1 -Mode full
```

覆盖：

- quick 全部内容。
- heavy relief。
- heavy texture。

### heavy

命令：

```powershell
.\scripts\run_regression.ps1 -Mode heavy
```

覆盖：

- `relief_nail_varnish_support`
- `relief_nail_white_support`
- `relief_rgb_gray`
- `textured_relief_rgb`

---

## 3. RIP Reader 输出模式

### 默认 verbose

默认行为保持逐层 checksum 输出，用于需要详细排查层数据时使用。

### summary

命令：

```powershell
build\Debug\rip_reader_test.exe --package output\StorageStrippedDefault --summary
```

输出示例：

```text
rip_reader_test: PASS
  packageDir: output\StorageStrippedDefault
  schema: p0.rgbwsv.2
  storageMode: stripped
  grid: 48 x 24 x 25
  bitDepth: 8
  channelOrder: R G B W S V
  channelPrintPixels: R=22560 G=22560 B=22560 W=0 S=5640 V=0
  warnings: 0
```

### quiet

命令：

```powershell
build\Debug\rip_reader_test.exe --package output\StorageStrippedDefault --quiet
```

输出示例：

```text
PASS output\StorageStrippedDefault
```

bad package 期望错误在 quiet 下输出示例：

```text
PASS expected-error E_TIFF_STORAGE_MISMATCH
```

---

## 4. RIP Compatibility Checklist

已新增：

```text
docs/slicer/RIP_COMPATIBILITY_CHECKLIST_RGBWSV_TIFF.md
```

覆盖内容：

- schema。
- channel protocol。
- pixel polarity。
- TIFF sample format。
- photometric / compression。
- stripped storage。
- tiled storage。
- layer list。
- reader error handling。
- 当前 P0 限制。

---

## 5. 验证结果

已运行并通过：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_regression.ps1 -Mode heavy -SkipBuild
.\scripts\run_regression.ps1 -Mode full -SkipBuild
.\scripts\run_regression.ps1 -SkipBuild
build\Debug\rip_reader_test.exe --package output\StorageStrippedDefault --summary
build\Debug\rip_reader_test.exe --package output\StorageStrippedDefault --quiet
```

耗时记录：

- build Debug：约 7 秒。
- quick：约 13 秒。
- heavy：约 873 秒，约 14 分 33 秒。
- full：约 966 秒，约 16 分 6 秒；该次使用 `-SkipBuild`，因为构建已单独验证。
- 默认不传 `-Mode`：等价 quick，`-SkipBuild` 下约 11 秒。

---

## 6. 与 03B / 05 基线关系

保持不变：

- `schema = p0.rgbwsv.2`。
- `storageMode = stripped / tiled`。
- `channelOrder = R G B W S V`。
- `bitDepth = 8`。
- `black_is_print`。
- `0=打印，255=不打印`。
- MaterialPolicy RGB/W/V 语义。
- Texture sampling 语义。
- Support generation 语义。

quick regression 继续覆盖：

- stripped 默认包。
- tiled compatibility 包。
- MaterialPolicy 六个样例。
- storage bad packages。

---

## 7. 当前限制

- heavy 模式仍主要耗时在大包 RIP 逐层读取，03C 只缩短日志，不优化 reader 性能。
- `rip_reader_test --summary` 当前 warning count 为工具字段，现阶段没有独立 warning 收集链路，输出为 `0`。
- 默认 verbose 仍保留逐层 checksum，适合调试但不适合常规回归日志。

---

## 8. 下一阶段建议

03C 已完成回归与 RIP 输出收口，建议可以进入：

```text
06：3MF 与多材料输入基础版
```

进入 06 前建议先确认：

- 目标 3MF 输入需要映射到当前 RGB/W/V/S 通道的规则。
- 是否继续沿用 `p0.rgbwsv.2`，还是为 3MF 输入新增 manifest source metadata。
- 真实 RIP 是否优先使用 stripped TIFF，并确认 `RowsPerStrip=64` 的兼容性。
