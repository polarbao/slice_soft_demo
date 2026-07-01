# TASKS_00_08_历史阶段文档补齐任务清单

> 文档版本：v0.1
> 文档状态：Codex Task List / Historical Docs Supplement
> 生成日期：2026-07-01

---

## 1. 总规则

00-08 是历史阶段，不作为当前功能开发入口。

每次只执行用户明确指定的一个任务。

每个任务开始前：

```powershell
git status --short
```

每个任务完成前：

```powershell
git diff --check
```

---

## 2. 推荐任务

```text
00-08-0：盘点 archive 中 00-08 PRD / DEV / DEMO / REPORT / TASKS；
00-08-1：生成 DOC_AUDIT_00_08；
00-08-2：生成 PRD_00_08 功能基线汇总；
00-08-3：生成 DEV_00_08 技术基线汇总；
00-08-4：生成 DEMO_00_08 验证与回归基线；
00-08-5：更新 README / DOC_INDEX；
00-08-6：生成短期 / 中期 / 长期项目运行计划。
```

---

## 3. 非目标

```text
不恢复 docs/slicer；
不把 archive 文档重新设为当前入口；
不重新生成 00-08 每个子阶段的单独 PRD/DEV；
不修改源码；
不运行构建，除非用户明确要求验证当前代码。
```

