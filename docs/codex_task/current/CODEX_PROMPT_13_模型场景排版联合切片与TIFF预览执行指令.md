# CODEX_PROMPT 13 模型场景、排版联合切片与 TIFF 原生预览执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/SLICE_AI_SKILL_MASTER.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/DOC/DOC_DECISION_13_模型场景排版与TIFF原生预览专项拆分.md
docs/slice/DOC/DOC_MATRIX_13_模型场景专项依赖与准入矩阵.md
docs/slice/DOC/DOC_CHECKLIST_13_未决产品输入与阶段Gate.md
docs/slice/DOC/DOC_PREP_13A_01_ModelTransform与ModelInstance合同准备.md
docs/slice/DOC/DOC_PREP_13B_01_MultiModelScene与EffectiveConfig准备.md
docs/slice/DOC/DOC_PREP_13C_01_TIFFLayerSource与Cache准备.md
docs/slice/DOC/DOC_PREP_13_全阶段原子任务实施准备与文件所有权.md
docs/slice/ROADMAP/ROADMAP_13_模型场景排版联合切片与TIFF预览路线.md
docs/slice/PRD/PRD_13A_模型俯视工作区与实例变换.md
docs/slice/DEV/DEV_13A_模型俯视渲染与变换架构设计.md
docs/slice/DEMO/DEMO_13A_模型俯视与变换验证方案.md
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md
docs/slice/DEMO/DEMO_13B_多模型排版联合切片验证方案.md
docs/slice/PRD/PRD_13C_RGBWSV_TIFF原生统一预览.md
docs/slice/DEV/DEV_13C_TIFFLayerSource与统一材料合成设计.md
docs/slice/DEMO/DEMO_13C_TIFF原生统一预览验证方案.md
docs/codex_task/current/TASKS_12_13_后续开发计划总览清单.md
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
```

## 执行规则

```text
1. 只执行用户明确指定的一个原子任务；
2. 开始前检查 branch 和 dirty worktree；
3. 先读相关源文件，不根据文件名猜实现；
4. Qt 不进入 slicer_core；
5. 不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
6. 不改变 Legacy 默认和 Global 显式 opt-in；
7. 不允许模型变换绕过 geometry admission；
8. 不允许联合切片失败后静默拆分成单模型成功；
9. 不从 TIFF 猜测不存在的 Texture/Fill 语义；
10. 不未经 Spike 直接引入 VTK/Qt3D 或其他大型依赖；
11. 完成任务指定验证后更新对应 REPORT/索引/上下文；
12. 未经用户要求不提交、不 push。
```

## 当前跨阶段推荐入口

```text
13C-02 MaterialPreviewComposer
```

`13A-01..05`、`13B-01..07`、`13C-01` 和 scene-aware `12E-09A-02` 已完成。下一步按独立
`CODEX_PROMPT_13C_02_MaterialPreviewComposer执行指令.md` 执行 13C-02；不得夹带 13C-03
Widget 接线、13C-04 Preview IO 收口或 12E-09A Diagnostic UI。

## 代码规范

```text
类与函数 PascalCase；
局部变量 camelCase；
成员变量 m_xxx；
Qt 自定义槽 On...；
Qt 自定义信号 Sig...；
函数指针 connect；
C++ Allman；
文件 PascalCase；
Public API Doxygen；
第三方 API 保持第三方命名；
不使用 using namespace std。
```

## 每任务输出

```text
Current State；
Target State；
实际修改文件；
实际验证命令和结果；
未执行验证；
剩余风险；
下一任务状态；
git status --short。
```
