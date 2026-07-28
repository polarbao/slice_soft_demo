# CODEX PROMPT 13B-08-03 Qt 当前场景切片执行指令

先阅读 13B-08 的 Decision、PRD、DEV、DEMO、TASKS，以及：

```text
docs/slice/DOC/DOC_PREP_13B_08_03_Qt当前场景切片准备.md
13B-08-01/02 当前状态报告
SceneDocument、SceneTransformController、ProductionPackageResultValidator
MainWindow 当前动作可用性逻辑
```

现在只执行 `13B-08-03`。

1. 先增加 controller/UI Smoke 失败用例。
2. 新增 `SceneSliceActionController`，不要把状态机塞回 `MainWindow.cpp`。
3. 主动作消费当前冻结 SceneDocument，不得打开模型文件对话框。
4. 调用显式 `--scene-config` 路由，并校验进程和 Package identity。
5. 完成 stale、cancel、blocked、无 fallback 行为。
6. 成功后自动加载单一 Package 和生产 TIFF 预览。
7. 不重排整个 MainWindow，不修改 TIFF 协议或 OpenVDB 默认。
8. 遵循 Qt 信号槽、命名、Doxygen 和 Allman 规范。
9. 运行 PREP 中 Qt/core/Quick CI 验证。
10. 生成 `REPORT_13B_08_03_Qt当前场景切片当前状态.md`。
11. 按中文 `【模块】` 风格原子提交。
