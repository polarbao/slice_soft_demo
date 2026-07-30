# CODEX_PROMPT_13G 支撑投影铺底与层间连续性执行指令

请按顺序阅读：

```text
AGENTS.md
.agents/AGENTS.md
docs/slice/DOC/DOC_AUDIT_13G_Reality模型朝向与内部支撑连续性.md
docs/slice/DOC/DOC_DECISION_13G_支撑投影铺底与层间连续性专项.md
docs/slice/PRD/PRD_13G_支撑投影铺底与层间连续性.md
docs/slice/DEV/DEV_13G_支撑投影铺底与层间连续性设计.md
docs/slice/DEMO/DEMO_13G_支撑投影铺底与层间连续性验证方案.md
docs/codex_task/current/TASKS_13G_支撑投影铺底与层间连续性任务清单.md
```

执行约束：

```text
1. 先完成 13G-00B/00C，不得先用铺底掩盖反向摆放；
2. 每次只改变一个业务语义并运行对应验证；
3. Reality 每次最多完整切一个模型；
4. 正反面和支撑不得按文件名特判；
5. 历史 fixture 缺省行为保持不变；
6. 新建生产场景默认 baseProjection=true/layerCount=30；
7. SupportType 只进入内部/report，不改变 TIFF 通道；
8. 不修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
9. OpenVDB 保持默认关闭；
10. 未实际运行的验证不得写为 PASS。
```

实现完成后生成：

```text
docs/slice/REPORT/REPORT_13G_支撑投影铺底与层间连续性当前状态.md
```
