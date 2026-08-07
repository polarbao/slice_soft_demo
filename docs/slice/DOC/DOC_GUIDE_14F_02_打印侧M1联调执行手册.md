# DOC_GUIDE_14F-02 打印侧 M1 联调执行手册

> 状态：SLICER-SIDE READY / PRINT-SIDE ACK PENDING
> 日期：2026-08-07
> 范围：打印宿主装载、能力清单和 `pm_self_test`；不包含单模型 S1、RIP S2 或实物打印

## 1. 交付目录

执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/Prepare14F02PrintM1Handoff.ps1
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/Test14F02PrintM1Handoff.ps1
```

生成目录：

```text
output/handoff/stage14f02/
  modules/slicer/          # Release DLL、Worker、manifest、运行时依赖和许可证
  contracts/               # SPI v1、DTO 1.2、三车道、取消、错误码和文件合同
  tools/slicer_host_sim.exe
  handoff_manifest.json
  handoff_checksums.sha256
  INTEGRATION_GUIDE.md
```

打印侧必须保持 `modules/slicer/` 的相对目录结构，禁止只复制 DLL 而遗漏 Worker、MSVC Runtime、
许可证、依赖 inventory 或校验文件。

## 2. M1 接入顺序

1. 扫描 `modules/*/module.json`，在加载 DLL 前校验 manifest schema、`id=slicer`、`spi=1`、
   `runtime=MSVC-x64-MD`、`buildConfig=Release` 和 15 项能力。
2. 使用 `LoadLibraryEx(..., LOAD_WITH_ALTERED_SEARCH_PATH)` 运行时装载；打印软件不得链接
   `slicer_module.lib`，其静态导入表不得出现切片模块。
3. 精确解析 `print_module_spi.h` 中冻结的 11 个 `pm_*` 符号；缺任一符号立即拒绝模块。
4. 校验 `pm_spi_version()==PM_SPI_VERSION`，读取 `pm_module_info()` 并再次核对运行时、构建配置和
   15 项能力。
5. 执行 `pm_create()`、`pm_self_test()`、`pm_destroy()`；自检失败必须显示稳定错误码并卸载模块。
6. 在未进入前处理/切片页面的纯打印路径采集进程模块列表，确认没有装载 `slicer_module.dll`。
7. 进入前处理页面后再次采集模块列表，记录实际加载路径必须指向部署目录
   `modules/slicer/slicer_module.dll`。

本包内的独立探针可先验证切片侧交付：

```powershell
.\tools\slicer_host_sim.exe --m1-self-test `
  .\modules\slicer\slicer_module.dll
```

该探针 PASS 不能替代打印软件自身的 M1 证据。

## 3. 必需回签证据

打印侧 ACK 至少包含：

| 证据 | 通过条件 |
|---|---|
| manifest 前置校验 | 非法 schema/SPI/runtime/buildConfig 在装载前被拒绝 |
| ABI 装载 | 11 个符号全部解析，缺符号负例 fail-closed |
| 能力清单 | `pm_module_info()` 精确包含冻结的 15 项能力 |
| 模块自检 | `pm_self_test()` 返回成功，并完成实例销毁和卸载 |
| 纯打印路径 | 进程模块列表不含 `slicer_module.dll` |
| 前处理路径 | 模块列表记录的 DLL 绝对路径指向部署目录 |
| 缺包提示 | 删除/改名 `modules/slicer` 后，UI 给出可操作错误且打印主流程不崩溃 |

建议回签机器记录操作系统、MSVC Runtime、打印软件 commit、模块 SHA-256、时间和完整命令输出。

## 4. 阶段边界

- 上述打印侧证据齐备前，14F-02 只能标记为 `SLICER-SIDE READY / PRINT-SIDE ACK PENDING`。
- 14F-02 通过后才能进入 14F-03 的单模型导入、切片和 S1 正负例。
- 本任务不改变 `p0.rgbwsv.2`、`R G B W S V`、8-bit、`black_is_print`、默认手写 TIFF Writer
  或 Legacy 默认切片路径。
