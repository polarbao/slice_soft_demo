# DEMO_09P_R2_CI_Matrix验证方案

> 文档版本：v0.1
> 文档状态：Formal DEMO / Stage 09P-R2-8
> 生成日期：2026-07-01
> 适用范围：OpenVDB OFF / ON / Benchmark 分层 CI matrix

---

## 1. 目标

09P-R2 CI matrix 用于把默认生产安全轨道和 OpenVDB experimental 轨道分开。

默认轨道必须不依赖 OpenVDB、不依赖 vcpkg OpenVDB 目录、不写 production experimental package。

---

## 2. Matrix

| Lane | 默认执行 | 依赖 | 验证内容 |
|---|---:|---|---|
| OpenVDB OFF/default | 是 | `build`，`USE_OPENVDB=OFF` | build、ctest、run_ci_quick、09P CLI smoke、schema、golden |
| OpenVDB ON | 否 | 显式 `-RunOpenVdbOn -OpenVdbBuildDir <dir>` | OpenVDB smoke、surface shell、真实模型、experimental CLI |
| Benchmark | 否 | 显式 `-RunBenchmarks -OpenVdbBuildDir <dir>` | Release benchmark，手动/可选 |

---

## 3. 默认命令

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_r2_ci_matrix.ps1
```

默认命令应完成：

```text
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
scripts/run_ci_quick.ps1
scripts/run_09p_cli_experimental_tests.ps1
scripts/run_09p_schema_tests.ps1
scripts/run_09p_golden_tests.ps1
```

---

## 4. OpenVDB ON 命令

OpenVDB ON 轨道必须显式提供已配置的 OpenVDB build dir：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_r2_ci_matrix.ps1 `
  -RunOpenVdbOn `
  -OpenVdbBuildDir build-openvdb
```

本机推荐 OpenVDB 依赖根：

```text
D:\vcpkg-openvdb
```

不建议在 OpenVDB ON 轨道使用带空格的 vcpkg 根目录，除非后续依赖验证证明其稳定。

---

## 5. Benchmark 命令

Benchmark 为手动轨道：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_r2_ci_matrix.ps1 `
  -RunBenchmarks `
  -OpenVdbBuildDir build-openvdb
```

Benchmark 结果只做趋势判断，不作为默认 Debug CI 的通过条件。

---

## 6. 通过标准

默认 OFF lane 通过时必须满足：

```text
1. Debug build 通过；
2. ctest 通过；
3. run_ci_quick 通过；
4. 09P experimental CLI report 保持 nonProduction；
5. schema/golden contract 通过；
6. OpenVDB ON 和 Benchmark 明确记录为 skipped，而不是伪装成已验证。
```

OpenVDB ON lane 只有在传入 build dir 并完成 smoke/experimental pipeline 后，才能记录为已验证。

---

## 7. 生产边界

CI matrix 不改变：

```text
OpenVDB 默认关闭；
legacy slicer_cli production path；
p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
warn_and_attempt 非 production-safe。
```
