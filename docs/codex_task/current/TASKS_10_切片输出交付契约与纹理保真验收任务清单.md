# TASKS_10_切片输出交付契约与纹理保真验收任务清单

> 文档版本：v0.1
> 文档状态：Codex Task List / Stage 10
> 生成日期：2026-07-01

---

## 1. 总规则

每次只执行用户明确指定的一个任务。

每个任务开始前：

```powershell
git status --short
```

每个任务完成前：

```powershell
git status --short
git diff --check
```

阶段边界：

```text
不实现 RIP 半色调；
不实现设备通信；
不生成喷头 bitstream；
不把 RIP SDK 引入 slicer_core；
不修改 p0.rgbwsv.2；
不改变 RGBWSV channel order；
不默认启用 OpenVDB。
```

---

## 2. 推荐任务顺序

```text
10-0：阶段文档入口同步；
10-1：定义 output contract 字段；
10-2：定义 layer summary / channel summary；
10-3：定义 texture fidelity 指标；
10-4：建立真实模型验收集；
10-5：生成 downstream handoff checklist；
10-6：建立 output contract golden / schema 验证；
10-7：生成 REPORT_10。
```

---

## 3. Task 10-0：阶段文档入口同步

状态：本轮完成，提交见 `docs(10): 同步 Stage 10 阶段入口`。

目标：

```text
新增 PRD / DEV / DEMO / DOC_DECISION / TASKS / CODEX_PROMPT；
同步 README / DOC_INDEX / roadmap；
明确 10 阶段不是 RIP 实现。
```

验证：

```powershell
git diff --check
```

---

## 4. Task 10-1：Output contract 字段

状态：本轮完成，提交见 `docs(10): 定义输出契约字段矩阵`。

目标：

```text
定义 package / manifest / report / layer summary 中哪些字段可作为下游稳定契约。
```

建议新增：

```text
docs/slice/DEV/DEV_10_OutputContract_FieldMatrix.md
```

已新增：

```text
docs/slice/DEV/DEV_10_OutputContract_FieldMatrix.md
```

验证：

```powershell
git diff --check
```

---

## 5. Task 10-2：Layer summary / channel summary

目标：

```text
定义每层和每通道统计字段；
明确 RGB / W / S / V 的统计方式；
明确哪些统计可作为 golden 比较。
```

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

---

## 6. Task 10-3：Texture fidelity 指标

目标：

```text
定义 textureResolvedRate、uvCoverageRate、fallbackPixelRate 等指标；
明确 OBJ/MTL、3MF ColorGroup、Texture2DGroup 的字段来源。
```

验证：

```powershell
git diff --check
```

---

## 7. Task 10-4：真实模型验收集

目标：

```text
建立真实模型集合清单；
定义每个模型的期望输出摘要；
标记不可 production-safe 的原因。
```

验证：

```powershell
git diff --check
```

---

## 8. Task 10-5：Downstream handoff checklist

目标：

```text
定义交付给下游 RIP 工程师的 package / manifest / report / limitation 清单；
记录下游反馈入口。
```

验证：

```powershell
git diff --check
```

---

## 9. Task 10-6：Output contract golden / schema

目标：

```text
建立 output contract schema；
建立 golden summary；
必要时新增 run_10_output_contract_tests.ps1。
```

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_golden_tests.ps1
git diff --check
```

---

## 10. Task 10-7：REPORT_10

目标：

```text
生成 docs/slice/REPORT/REPORT_10_切片输出交付契约与纹理保真验收当前状态.md；
记录已完成字段、验证结果、下游待确认项、是否进入 11。
```

验证：

```powershell
git status --short
git diff --check
```
