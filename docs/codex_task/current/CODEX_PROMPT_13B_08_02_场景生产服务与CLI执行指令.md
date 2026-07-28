# CODEX PROMPT 13B-08-02 场景生产服务与 CLI 执行指令

先阅读 13B-08 的 Decision、PRD、DEV、DEMO、TASKS，以及：

```text
docs/slice/DOC/DOC_PREP_13B_08_02_场景生产服务与CLI准备.md
docs/slice/REPORT/REPORT_13B_联合切片当前状态.md（若存在）
src/slicer_core/pipeline 下 13B-05..07 实现
apps/multi_model_scene_matrix/Main.cpp
apps/slicer_cli/main.cpp
```

现在只执行 `13B-08-02`。

1. 先写服务与 CLI 负向测试，再写实现。
2. 新增无 Qt `MultiModelProductionService`，复用正式编排器、合成器和 writer。
3. 新增显式 `slicer_cli --scene-config`，保持单模型 `--config` 兼容。
4. 校验 schema/hash/revision、可见实例、资源、Profile、buildVolume 和模式准入。
5. Global 未准入必须 fail-closed，不得回退 Legacy。
6. 只产生一个场景 Package，每层一个 RGBWSV TIFF。
7. 保持 `p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print`。
8. 不把 fixture runner 当产品入口，不把 Qt 类型引入 core。
9. 运行 PREP 的 core/CLI/RIP/Quick CI 验证。
10. 生成 `REPORT_13B_08_02_场景生产服务与CLI当前状态.md`。
11. 按中文 `【模块】` 风格原子提交。
