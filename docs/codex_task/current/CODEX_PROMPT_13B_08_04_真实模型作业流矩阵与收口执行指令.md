# CODEX PROMPT 13B-08-04 真实模型作业流矩阵与收口执行指令

先阅读 13B-08 全部正式文档、01/02/03 当前状态报告，以及：

```text
docs/slice/DOC/DOC_PREP_13B_08_04_真实模型作业流矩阵与收口准备.md
docs/slice/REPORT/REPORT_12E_08C_R4_模型资产预检清单.md
现有 Stage 13B 矩阵脚本和 RIP strict 入口
```

现在只执行 `13B-08-04`。

1. 建立 1/3/11/12/22 的真实资产作业流矩阵。
2. 覆盖 OBJ、OBJ/MTL texture、3MF、部分导入失败、容量、碰撞和越界。
3. 验证 Qt 与 `--scene-config` 产生一致的单 Package 合同。
4. 所有正向 Package 执行 RIP strict。
5. Global 未准入时验证显式阻断，不得回退 Legacy。
6. 增加可重复运行的 `run_13b_08_scene_workflow.ps1`。
7. 更新用户操作说明、Stage 13 总览、任务清单和上下文。
8. 设备输入未关闭时只记录 FUNCTIONAL PASS。
9. 运行 Debug、Release targeted、Quick CI 和 `git diff --check`。
10. 生成 `REPORT_13B_08_批量导入与当前场景切片当前状态.md`。
11. 按中文 `【模块】` 风格原子提交。
