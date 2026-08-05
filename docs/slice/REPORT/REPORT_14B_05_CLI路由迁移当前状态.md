# REPORT_14B-05 CLI 路由迁移当前状态

> 更新时间：2026-08-05
> 任务：Stage 14B-05
> 状态：`COMPLETE / DEBUG_RELEASE_GATES_PASS / FULL_REGRESSION_PASS`

## 1. 任务结论

`slicer_cli --scene-config` 已从直接调用生产场景服务迁移到 `CreateProductionSliceFacade()` 与
`SliceFacade::Run()`。该迁移只改变 CLI 的应用层路由，不改变既有场景有效配置、模型导入、排版、
切片、RGBWSV 合成、TIFF Writer、manifest 或包发布实现。

以下入口保持原路由：

- 单模型 `--config`；
- 模型检查与 preview-only；
- OpenVDB 诊断、候选切片和核心 benchmark；
- TIFF backend 与 OpenVDB capability 查询。

## 2. 兼容性处理

- Facade `PM-SLICER-*` 错误继续映射为既有 `SCENE_*` CLI 错误名；场景失败仍返回退出码 `2`。
- Facade `ProgressEvent` 适配到既有 `SLICE_PROGRESS` 文本格式，不改变 UI/脚本消费入口。
- 成功摘要继续读取已发布包中的 `reports/multimodel_scene_report.json`，不伪造 scene revision、
  visible instance 或 layer count。
- timing 只输出 Facade 实际 `totalMs` 与真实进程内存，不伪造 Facade 尚未承载的分阶段时间。
- CLI 仍使用同步不可取消 token；Worker 进程取消、2 秒上限和深层取消贯穿继续归 14D-04。

## 3. 新增门禁

新增 `ValidateStage14BCliFacadeRoute.py` 并接入 CTest，静态守门以下事实：

1. `--scene-config` 必须创建生产 `SliceFacade`；
2. 场景生产必须调用 `SliceFacade::Run()`；
3. CLI 不得直接调用 `RunMultiModelProductionService()`；
4. 场景生产不得回退到单模型 `RunSlicePipeline()`；
5. Facade 错误必须经过既有场景错误名映射。

该门禁与既有正向、负向 scene route fixture 互补：前者验证架构路由，后者验证真实运行行为。

## 4. 验证

已实际通过：

- Debug / Release `slicer_cli` 构建；
- Debug / Release 14B-05 路由合同及 SliceFacade、生产服务、scene route 共 `7/7 PASS`；
- 14B-03A-R1 后的 ViewData、base/engine 目标图和合同门禁；
- `scripts/run_regression.ps1 -Mode full -BuildDir build-slicesoft/main -Config Debug -SkipBuild`：
  `PASS`，耗时 `985.8 s`，包含正向切片/RIP 回归、重型浮雕与纹理样例、报告检查和 bad package
  负向错误码验证；
- `git diff --check`。

## 5. 保持不变的冻结边界

- `PM_SPI_VERSION=1`、11 个 `pm_*` 导出和 15 项能力不变；
- `p0.rgbwsv.2`、`R G B W S V`、uint8、`black_is_print` 不变；
- Writer backend、OpenVDB 默认轨道和单模型 CLI 不变；
- 不提前实现 Worker、DLL、14E Qt 参考宿主或打印侧业务。

## 6. 后续

14B-05 收口后，14B 核心 Facade 阶段完成。下一批可并行启动：

- `14C-01`：建立 SPI DLL 外壳与精确 11 符号导出；
- `14D-01`：从 CLI 演进独立 `slicer_worker.exe`。

共享合同、CMake 接入和最终集成提交必须串行审查，避免 DLL 与 Worker 同时修改同一构建入口。
