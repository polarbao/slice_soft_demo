# REPORT_14F-02 打印侧 M1 联调准备当前状态

> 状态：SLICER-SIDE READY / PRINT-SIDE M1 ACK PENDING
> 日期：2026-08-07
> 范围：14F-02 的切片侧交付、独立 M1 探针和接收门禁；不冒充打印软件联调结果

## 1. 当前结论

14F-02 的切片侧可执行准备已经闭合。现可生成一个自包含交付目录，独立验证：

- `module.json`、SPI v1、Release runtime/buildConfig 和 15 项能力；
- 精确 11 个运行时解析入口中的装载、自述、实例、自检和错误读取路径；
- `pm_self_test` 成功；
- 缺少 DLL 时 fail-closed；
- 能力包、公开合同、许可证和工具的全文件 SHA-256。

但打印软件当前尚未提供 M1 `ModuleRegistry`/运行时模块装载实现及外部 ACK，因此 14F-02 仍不能标记
`COMPLETE`，14F-03 也未放行。

## 2. 本轮实现

### 2.1 独立 M1 探针

`slicer_host_sim` 新增：

```text
--m1-self-test <slicer_module.dll>
```

该模式只执行 DLL 装载、SPI/能力清单、`pm_create`、`pm_self_test`、未知能力 fail-closed、
`pm_destroy` 和卸载，不提交 Worker 切片任务。原 14E-01 导入到 RGBWSV 的端到端入口保持不变。

### 2.2 打印侧交付目录

脚本：

```powershell
scripts/Prepare14F02PrintM1Handoff.ps1
scripts/Test14F02PrintM1Handoff.ps1
```

输出：

```text
output/handoff/stage14f02/
  modules/slicer/
  contracts/
  tools/slicer_host_sim.exe
  handoff_manifest.json
  handoff_checksums.sha256
  INTEGRATION_GUIDE.md
```

`handoff_manifest.json` 固定 7 项打印侧回签证据。接收方可先运行包内探针，但最终仍须由打印软件自身
记录 manifest 前置校验、模块装载路径、自检、纯打印路径未装载切片 DLL 和缺包 UI 提示。

### 2.3 VSCode 入口

- `SliceSoft 14F: Prepare Print M1 Handoff (Release)`
- `SliceSoft 14F: Validate Print M1 Handoff (Release)`

## 3. 已执行验证

| 验证 | 结果 |
|---|:---:|
| Release `slicer_host_sim`、模块和 Worker 增量构建 | PASS |
| `stage14e01_c_host_end_to_end` | PASS |
| `stage14f02_m1_intake_self_test` | PASS |
| `slicer_source_size_guard_self_test` | PASS |
| PowerShell 两份脚本语法解析 | PASS |
| VSCode `tasks.json` JSON 解析 | PASS |
| 14F-01 Package 重建与本地隔离验证 | PASS |
| 14F-02 Handoff 全文件哈希、M1 探针和缺 DLL 负例 | PASS |

本地关键输出：

```text
STAGE14F02_M1_INTAKE_PASS spi=1 capabilities=15
STAGE14F02_HANDOFF_PREPARED
STAGE14F02_SLICER_HANDOFF_PASS ... PRINT_SIDE_M1_ACK_PENDING
```

## 4. 外部阻断与下一步

对相邻打印软件仓库的只读审计未发现计划中 `business/platform` 的 `ModuleRegistry` 实现，当前只有
M1 设计和任务文档。因此还需打印侧完成：

1. manifest 扫描和装载前校验；
2. RAII `ModuleHandle` 与 11 个符号的运行时解析；
3. SPI/runtime/buildConfig/能力清单拒绝策略；
4. `pm_self_test` 与错误 UI；
5. 纯打印路径和前处理路径的进程模块清单证据。

这些证据回签后才可把 14F-02 改为 `COMPLETE`，随后进入 14F-03 单模型到 S1 联调。

## 5. 安全边界

- 未修改 `p0.rgbwsv.2`、`R G B W S V`、8-bit 或 `black_is_print`；
- 未修改默认手写 TIFF Writer、Legacy 默认路径或 OpenVDB 默认状态；
- 未在切片仓库伪造打印软件 M1、干净机、目标 RIP 或实物验证证据。
