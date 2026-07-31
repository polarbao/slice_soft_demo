# CODEX_PROMPT_03D LibTIFF 兼容迁移执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/DOC/DOC_DECISION_03D_LibTIFFWriter兼容迁移与性能Gate.md
docs/slice/PRD/PRD_03D_LibTIFF_RGBWSV兼容写入与性能优化.md
docs/slice/DEV/DEV_03D_LibTIFFWriter双后端迁移设计.md
docs/slice/DEMO/DEMO_03D_LibTIFF兼容与性能验证方案.md
docs/codex_task/current/TASKS_03D_LibTIFF兼容迁移任务清单.md
```

执行规则：

```text
1. 每次只执行用户明确授权的一个 03D 原子任务。
2. 03D-01 不得安装依赖或改变生产 Writer。
3. R1-R4 保持 handwritten 默认，LibTIFF 显式 opt-in。
4. 不修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print。
5. 不实现压缩、BigTIFF、多 IFD 或 planar separate。
6. 不修改共享 VCPKG_ROOT 中其他项目的 classic 依赖状态。
7. 每个任务运行定向测试、git diff --check，并按任务范围提交。
8. 没有 Writer-only 性能证据时不得宣称 LibTIFF 更快。
9. 03D-07 必须再次获得用户明确授权。
```

当前 `03D-01/02` 已完成；下一原子任务从 `03D-03` 开始。R1-R4 仍必须保持
handwritten 为默认生产 Writer。
