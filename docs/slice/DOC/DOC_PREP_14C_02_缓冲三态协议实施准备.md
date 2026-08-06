# DOC_PREP_14C-02 缓冲三态协议实施准备

> 日期：2026-08-06
> 状态：`PREPARATION_GATE = PASS`
> 对应任务：`14C-02`

## 1. 目标与真源

本卡只为全部 `char*` ABI 出参提供唯一的 `WriteOut()` 实现，不接入能力分派、句柄、
Worker 或模块自述。行为真源按优先级为：

1. `contracts/print_module_spi.h` 的缓冲区协议；
2. `DEMO_14` 的 `C-SPI-05a/05b/05c`；
3. `DEV_14` 第 3.1 节。

## 2. 冻结行为

内部接口接收 UTF-8 字节串、`out`、`cap` 和可空的 `outRequired`，并遵守：

| 输入 | 返回 | 写入 |
|---|---:|---|
| `out == nullptr` 或 `cap == 0` | `PM_ERR_BUFFER_SMALL` | 不写；若 `outRequired` 非空则写内容字节数 |
| `0 < cap < required + 1` | `PM_ERR_BUFFER_SMALL` | 完全不写，哨兵保持不变 |
| `cap >= required + 1` | `required` | 写完整内容并追加一个 `\0` |
| `cap < 0` | `PM_ERR_INVALID_ARG` | 不写 |

`required` 不包含末尾 NUL。空字符串成功所需容量为 1，成功返回 0。长度超出 `int`
可表示范围时必须安全失败，不发生截断或部分写。`outRequired == nullptr` 在所有状态均合法。

## 3. 文件所有权与测试

```text
src/slicer_module/BufferApi.h
src/slicer_module/BufferApi.cpp
tests/stage14c_02/BufferApiTests.cpp
```

测试必须覆盖探测、差 1、恰好足够、过量容量、空串、可空 `outRequired`、负容量和哨兵不变。
实现不得依赖 Qt、`slicer_engine` 或第三方 JSON 库。

## 4. 出口

- 单一实现可被后续 `pm_module_info`、`pm_poll`、`pm_result`、`pm_self_test`、
  `pm_last_error` 复用；
- C-SPI-05a/b/c 独立单测通过；
- 不改变 SPI v1、11 个导出、15 项能力或 RGBWSV/TIFF 协议。
