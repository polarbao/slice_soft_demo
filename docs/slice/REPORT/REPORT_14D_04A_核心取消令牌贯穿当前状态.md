# REPORT_14D-04A 核心取消令牌贯穿当前状态

> 日期：2026-08-06
> 状态：`COMPLETE`

## 1. 已完成范围

- `ICancelToken` 已从 SliceFacade 生产绑定贯穿到场景生产服务、Legacy 适配器、编排器、
  图层合成器和 RGBWSV 包 Writer；
- 模型加载、场景准入、逐实例、逐层、长像素循环、TIFF 前后、预览、报告和发布前均有
  协作取消检查；
- 核心取消统一映射为 `PM-SLICER-CANCELLED-0070`；
- 写包期间取消会删除本次 staging，不发布半成品，也不覆盖已有有效包；
- 未请求取消时，带令牌与不带令牌的生产包文件集合和字节保持一致。

## 2. 验证结果

Debug 与 Release 均通过：

- 14D-04A 取消单元测试和静态传播合同；
- 14B-04 SliceFacade 单测及生产绑定回归；
- 多模型图层合成、包 Writer、生产服务和 RGBWSV Writer 回归；
- 取消前置、长循环中途取消、staging 清理、既有包保护和正常字节不变性；
- source-size guard 与 `git diff --check`。

`scene_layer_adapters_unit_tests` 中
`legacy_adapter_applies_admitted_instance_transform` 仍失败；以 HEAD 原适配器复测同样失败，
属于既有平移后图层字节不变性问题，不是本卡引入的回归，未伪造为 PASS。

## 3. 冻结边界

- 本卡只关闭进程内核心链路的协作取消，不代表 Worker E2E 取消完成；
- 单次 TIFF 库调用目前只能在调用前后检查，不能中断库内部正在执行的写入；
- Worker `Cancelling -> Cancelled`、退出码 8、进程退出、两秒上限和跨进程 staging 证据仍归
  14D-04B；
- 14D-04B 必须等待 14D-05 安全发布和 14D-08 独立请求入口完成。

## 4. 后续

14D-04A 已解除 14D-04B 的核心令牌前置，但不单独放行 04B。下一批可并行实施
14C-07 DLL 初始化红线，并审计 14D-05 安全发布准备；14D-08 当前准备门仍需关闭请求映射缺口。
