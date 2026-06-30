# DEMO_03C_回归分层与RIP摘要验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：03C  
> 建议提交目录：`docs/slicer/`

## 1. Demo 目标

验证：

```text
quick regression 可快速完成
full regression 可完整验证
heavy regression 可单独运行
rip_reader_test summary / quiet 输出可用
不破坏 03B storageMode 和 05 MaterialPolicy 基线
```

## 2. 验证命令

```powershell
cmake --build build --config Debug

.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_regression.ps1 -Mode full
.\scripts\run_regression.ps1 -Mode heavy

build\Debug\rip_reader_test.exe --package output\StorageStrippedDefault --summary
build\Debug\rip_reader_test.exe --package output\StorageStrippedDefault --quiet
```

## 3. 验收 Checklist

- [ ] `run_regression.ps1 -Mode quick` 通过。
- [ ] `run_regression.ps1 -Mode full` 通过。
- [ ] `run_regression.ps1 -Mode heavy` 可独立运行。
- [ ] `--summary` 输出 schema/storage/layers/channel stats。
- [ ] `--quiet` 只输出 PASS/FAIL。
- [ ] MaterialPolicy 六样例仍在 quick 或 full 中被验证。
- [ ] StorageMode stripped/tiled 仍被验证。
- [ ] Bad packages 仍被验证。
- [ ] 生成 RIP compatibility checklist。
- [ ] 生成 `REPORT_03C_回归与RIP输出收口当前实现状态.md`。
