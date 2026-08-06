# REPORT 14D-08-R3-02B-F1 生产修复 Facade 当前状态

> 更新日期：2026-08-06
>
> 状态：`COMPLETE / WORKER_NOT_REGISTERED`

## 1. 完成内容

- 加法扩展 engine 内部 `RepairRequest/RepairResult`，补齐 job、Profile、资源范围、格式、策略、
  pre/post strict 证据和耗时字段；
- 新增 `CreateProductionRepairFacade()`；
- 固定执行 source/Profile/path identity 校验、source hash、保守 cleanup、确定性 OBJ/MTL Writer、
  staged asset 重导入和完整 strict recheck；
- strict-PASS no-op 与允许的重复面/退化面 cleanup 均生成 job-owned staged OBJ，不返回 source 路径；
- 输出 `sha256:` source/asset digest、preflightBefore、preflightAfter 和结构化 repair evidence；
- 取消、scope escape、unsupported format/policy、资源缺失、保守修复阻断和 post-strict 失败均返回
 稳定 `PM-SLICER-*` 错误并清理本次 staging。

## 2. 当前边界

- Facade 成功结果的 `publicationState=job_staging`；它证明资产可重导入且 strict PASS，但尚未完成
  `14D-05` 定义的跨进程安全发布、崩溃恢复和模块二次清理；
- `geometry.repair` 仍未注册到生产 Worker registry，避免把未完成发布链路伪装成可用能力；
- 首版只接受/输出 OBJ，未接 Assimp exporter、3MF Writer 或复杂自相交重建。

## 3. 验证

```text
Debug stage14d08_r3_repair_facade_tests   PASS
Release stage14d08_r3_repair_facade_tests PASS
ValidateStage14BTargetGraph                PASS
ValidateCapabilityDtos                     PASS
ValidateFileContract                       PASS
ValidateThreeLaneContract                  PASS
```

用例覆盖重复面保守修复、完整 strict 证据、source 字节不变、取消和 output scope escape。

## 4. 下一入口

重新审计 `14D-05` 准备门。若 job identity、真实执行入口、模块二次清理和崩溃恢复合同已可闭合，
先完成安全发布，再实现 `14D-08-R3-02B-E1` Worker repair executor；否则必须保留显式阻断。
