# R2_BOUNDARY_配置报告测试CI工程化边界说明

> 文档版本：v0.1  
> 文档状态：Boundary / 非执行任务  
> 适用阶段：R1 前置说明  
> 建议提交目录：`docs/slicer/`

---

## 1. 为什么现在不生成 R2 详细任务

R2 的具体任务依赖 R1 的实际拆分结果。

当前可以定义 R2 的边界，但不建议写死源码级任务。

---

## 2. R2 目标边界

R2 聚焦：

```text
config schema version
config migration
report schema version
diagnostics/error code 统一
unit/golden/schema/regression/ui smoke test 分层
CI 入口
文档与 sample fixture 固化
```

---

## 3. R2 不做

```text
不做大型新功能；
不实现 compensated_varnish；
不引入 OpenVDB；
不做设备通信；
不做 RIP 半色调。
```

---

## 4. R2 进入条件

R1 完成后，如果满足：

```text
1. 模块边界初步拆分完成；
2. quick regression 通过；
3. legacy 文件职责清单明确；
4. config/report 写出路径稳定；
5. UI smoke test 仍可用；
```

再生成 R2 详细 PRD/DEV/TASK/CODEX_PROMPT。
