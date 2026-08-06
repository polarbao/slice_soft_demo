# REPORT_14C-07 DLL 初始化与依赖红线当前状态

> 更新时间：2026-08-06
> 任务：Stage 14C-07
> 状态：`COMPLETE / DLL_INITIALIZATION_GATE_PASS`

## 1. 已完成范围

- 新增 Windows 最小 `DllMain`，入口仅消费系统参数并返回 `TRUE`；
- 新增内部 `ModuleInitialization`，使用进程内 `std::once_flag` / `std::call_once`；
- `pm_create` 在创建每实例句柄前执行进程级一次性初始化，失败时返回空句柄并写入
  `PM-SLICER-INTERNAL-0099` TLS 错误；
- `pm_spi_version`、`pm_module_info`、`pm_last_error` 在首次 `pm_create` 前保持可用，且不触发初始化；
- 一次性初始化不创建 Worker、引擎、线程、文件、目录、模块句柄或作业；
- 新增成功、失败、并发 `call_once`、并发 `pm_create`、Loader Lock 静态红线和二进制依赖检查；
- CMake 仍保持 `slicer_module -> slicer_base` 单向链接，不新增 Qt、PrintSDK、Engine、OpenVDB
  或 LibTIFF 依赖。

## 2. 验证结果

### 2.1 Debug / Release 构建与专项回归

以下目标在 `build-slicesoft/main` 的 Debug 与 Release 中均构建通过：

- `slicer_module`；
- `stage14c07_dll_initialization_tests`；
- `stage14c03_module_abi_tests`；
- `stage14c05_module_info_tests`。

Debug、Release 定向 CTest 均为 `5/5 PASS`：

- 14C-07 初始化源码/二进制合同；
- 14C-07 并发初始化与并发模块实例；
- 14C-03 ABI 生命周期回归；
- 14C-05 模块自述源码合同与运行时回归。

专项测试证明：32 个并发初始化调用只执行一次受保护 action；失败 action 只执行一次并稳定保持
`Failed`；32 个并发 `pm_create` 均返回独立模块实例。

### 2.2 ABI、能力与依赖

- `ValidateCapabilityDtos.py`：15 项能力与双视图合同 PASS；
- `ValidateThreeLaneContract.py`：三车道合同 PASS；
- `ValidateStage14C01ModuleShell.py`：SPI v1 与模块薄壳合同 PASS；
- source-size guard：PASS，仅报告既有文件警告；
- Debug/Release `dumpbin /EXPORTS`：均为 11 functions / 11 names，且恰为冻结的 11 个
  `pm_*` 符号；
- Debug `dumpbin /DEPENDENTS`：仅 `ole32`、Kernel32 和 Debug CRT；
- Release `dumpbin /DEPENDENTS`：仅 `ole32`、Kernel32 和 Release CRT；
- 两种配置均不含 Qt、PrintSDK、`slicer_engine`、OpenVDB 或 LibTIFF。

本机 `dumpbin.exe` 不在 `PATH`，本轮通过 MSVC 14.51 工具链的绝对路径真实执行，未使用伪造
输出或静态文本替代 PE 验收。

## 3. 冻结边界

- `PM_SPI_VERSION=1` 不变；
- 导出集合仍为 11 个，能力集合仍为 15 项；
- `pm_module_info` 和 `module.json` 内容不变；
- `p0.rgbwsv.2`、RGBWSV、TIFF、Profile、Worker、core 和 Qt UI 均未修改；
- 初始化失败合同只复用既有 `PM-SLICER-INTERNAL-0099`，不新增 ABI 错误类型；
- 14C-07 不替代 14C-06 的完整 C-SPI-01..18 一致性套件。

## 4. 实施中的修正记录

首次运行专项静态合同测试时，校验脚本只读取 `ModuleInitialization.cpp`，未读取持有
`std::once_flag` 的头文件，因而产生测试自身的假失败。脚本已改为合并读取头文件与实现文件；
修正后 Debug/Release 全部定向测试重新运行并通过。

## 5. 后续

14C-07 已完成 DLL Loader Lock 与依赖红线收口。共享 `TASKS_14`、`REPORT_14` 和 DOC 索引
已同步本任务状态及后续准备审计结论；14C 总出口仍需由 14C-06A/06B 汇总完整 SPI 一致性证据。
