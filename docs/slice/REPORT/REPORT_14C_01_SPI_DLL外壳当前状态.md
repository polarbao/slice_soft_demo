# REPORT_14C-01 SPI DLL 外壳当前状态

> 更新时间：2026-08-05
> 任务：Stage 14C-01
> 状态：`COMPLETE / EXACT_11_EXPORTS_PASS`

## 1. 任务结论

已建立 `slicer_module.dll` 的最小 C ABI 外壳：

- 复用唯一跨边界头 `contracts/print_module_spi.h`；
- 使用 `PM_API`、`PM_CALL __cdecl` 和 `.def` 固定导出面；
- Debug 与 Release 均精确导出 11 个无 C++ 修饰的 `pm_*` 符号；
- DLL 只链接 `slicer_base`，不链接 `slicer_engine`、Qt 或 PrintSDK；
- 所有导出定义均封闭 C++ 异常。

## 2. 本卡边界

本卡只建立可装载、可链接、可审计的 ABI 外壳。除 `pm_spi_version()` 返回
`PM_SPI_VERSION=1` 外，其余未实现能力保持安全失败或空操作，不伪造成功结果。

以下内容继续由后续任务实现：

- 14C-02：缓冲三态 `WriteOut()`；
- 14C-03：句柄注册表和 TLS `pm_last_error`；
- 14C-04：15 项能力分派；
- 14C-05：`pm_module_info` 与 `module.json`；
- 14C-07：`DllMain` 红线与 `std::call_once` 初始化。

## 3. 验证证据

- Debug / Release `slicer_module` 构建：PASS；
- Debug / Release `dumpbin /EXPORTS`：`11 number of functions`、
  `11 number of names`，符号均无 C++ 修饰：PASS；
- `ValidateStage14C01ModuleShell.py` 独立运行及 Debug / Release CTest：PASS；
- source-size guard：PASS，33 条均为既存全树告警；
- `git diff --check`：PASS。

## 4. 冻结边界

- `PM_SPI_VERSION=1` 不变；
- 导出数量保持 11，不增加第 12 个导出；
- 15 项能力、ViewData v1.2、三车道合同和 RGBWSV/TIFF 协议不变；
- 14C-06 前不把 C-SPI-01..18 写成全部 PASS。
