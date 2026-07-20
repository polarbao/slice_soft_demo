# DOC_EXEC_12E-08C-R1-02 Eligibility Policy 结果

> 文档状态：COMPLETE
> 日期：2026-07-20

## 1. 实现结果

新增纯 `MeshRepairEligibilityPolicy`，直接消费既有 `MeshTopologyReport` 与 `MeshRobustnessReport`，不重复
计算拓扑、不修改网格。策略固定输出：

```text
eligible：退化三角形、同向 exact duplicate；
conditional：boundary、local winding、opposite duplicate；
manual_only：未分类 non-manifold、zero-volume component、采样式自相交检查；
fail_fast：无 accepted triangle、confirmed self-intersection。
```

聚合优先级固定为 `fail_fast > manual_only > conditional > eligible`。只有全部 decision 均为 `eligible` 时
`automaticRepairAllowed=true`；conditional 仅表示后续可继续分类，不授权当前阶段自动修复。
exact duplicate 只有在调用方明确提供“属性已检查且无冲突”的 evidence 后才能进入 eligible；缺少属性证据时
保持 conditional，UV/材质冲突时转为 manual_only，计数不一致的 evidence 按输入契约错误 fail-fast。

本任务同时把 R1-01 内部枚举对应的稳定文本对齐正式设计：`E_12E_REPAIR_NOT_ELIGIBLE` 与
`E_12E_REPAIR_ATTRIBUTE_CONFLICT`。这是尚未接入生产调用方的契约更正，不改变现有生产协议。

## 2. 安全边界

```text
repairAttempted=false；
不创建 MeshRepairOperation；
不修改 admission；
不写 TIFF/package/report 文件；
confirmed self-intersection 保持 fail-fast；
non-manifold 未证明唯一 fan pattern 前保持 manual_only；
legacy 与 OpenVDB 默认设置不变。
```

## 3. TDD 与验证

测试先引用尚不存在的 `MeshRepairEligibilityPolicy.h`，定向构建按预期以 `C1083` 失败；实现后实际执行：

```text
cmake --build build --config Debug --target mesh_repair_eligibility_unit_tests：PASS；
build/Debug/mesh_repair_eligibility_unit_tests.exe：PASS；
ctest --test-dir build -C Debug -R mesh_repair_(contract|eligibility) --output-on-failure：2/2 PASS；
cmake --build build --config Debug：PASS；
ctest --test-dir build -C Debug --output-on-failure：23/23 PASS；
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test：PASS；
scripts/run_ci_quick.ps1：FAIL，既有 material_process_top2 golden 期望 widthPx=48、实际=226。
```

Quick CI 失败点与 R1-01 记录一致，本任务未修改模型姿态、尺寸、Profile 或生产切片路径；脚本生成的 3MF
时间戳改动已还原。该命令不记为通过。

## 4. 下一任务

`12E-08C-R1-03 Generated Fixtures 与 Golden` 已解除 R1-02 阻断。R1-03 仍只验证资格分类、hash 和报告
golden，不执行实际 repair，不写生产 package。
