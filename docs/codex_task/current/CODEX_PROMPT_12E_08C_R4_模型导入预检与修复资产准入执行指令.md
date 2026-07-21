# CODEX_PROMPT_12E-08C-R4 模型导入预检与修复资产准入执行指令

## 1. Mandatory Read Order

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/DOC/DOC_DECISION_12E_08C_R3_04_08D_GO_NO_GO.md
docs/slice/DOC/DOC_DECISION_12E_08C_R4_模型导入预检与修复资产准入插入专项.md
docs/slice/PRD/PRD_12E_08C_R4_模型导入预检与修复资产准入.md
docs/slice/DEV/DEV_12E_08C_R4_ModelPreflight与RepairAssetAdmission设计.md
docs/slice/DEMO/DEMO_12E_08C_R4_模型预检与修复资产准入验证方案.md
docs/slice/ROADMAP/ROADMAP_12E_08C_R4_模型预检与修复资产准入路线.md
docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md
```

## 2. Execution Rule

只执行用户明确指定的一个 R4 原子任务。开始前检查分支和 dirty state；不得覆盖 `model/obj` 用户资产；
完成定向验证和独立提交后停止。

## 3. Product Truth

```text
R3-04 NO-GO 不取消 12E 产品目标；
正常模型可用于正向开发，不能替代 required 真实模型；
global 必须 strict/fail-closed；
legacy 保持兼容默认，但必须显示拓扑风险；
导入/一键切片必须经过 fresh preflight；
global blocker 不得 silent fallback；
R4 不实现通用复杂自相交重建。
```

## 4. Texture/Fill Truth

```text
base minimum=0.10mm，step=0.01mm；
effective minimum=max(0.10mm, 2 * classification resolution)；
width 到动态 threshold 时 texture=model、fill=0；
Model Fill 材料支持 white/varnish/RGB/custom/material_role；
C/M/Y/K 是 MaterialProcessProfile role，不是新 TIFF channel；
RGBWSV/uint8/black_is_print 不变。
```

## 5. Required Verification

每个任务至少运行其 task 条目要求的定向测试和：

```powershell
git diff --check
git status --short
```

C++/Qt/CMake 修改按 `.agents/docs/build-and-test.md` 选择 Debug/Release/Qt smoke。不能把计划命令写成已通过。

## 6. Stop Conditions

```text
发现 production writer/protocol 需要修改：停止并请求确认；
需要新增第三方几何库：比较至少两个候选后停止并请求确认；
自相交修复无唯一属性映射：返回 manual/external repair required；
required repaired assets 缺失：R4-06 及以后保持 blocked；
任何 global admission failure：不启动 08D。
```

