# REPORT_14B-01 Facade 与 DTO 合同当前状态

> 文档状态：COMPLETE
> 日期：2026-08-05
> 对应任务：`14B-01`
> 权威输入：`contracts/slicer_capability_dtos.json` v1.2、`contracts/slicer_capability_dtos.md` v1.2、`DOC_SCHEMA_14_SceneViewData网格DTO规格.md` v1.2

## 1. 任务结论

`src/slicer_core/api/` 已建立 Qt-free C++ Facade 合同。接口只使用 C++20 STL 与项目 DTO，
不包含 Qt 类型、C ABI 宏或 `print_module_spi.h`，错误通过 `ApiResult<T>` 返回，耗时操作显式接受
`ICancelToken`。本任务只冻结接口，不接入实现，不改变既有 CLI、生产切片算法或 RGBWSV 包。

## 2. 已落地接口

| 接口 | 目标分层 | 当前职责 |
|---|---|---|
| `ModelFacade` | base | 导入、元数据查询、句柄释放 |
| `PackageQueryFacade` | base | 包摘要、层描述、生产 TIFF 预览、严格校验、报告读取 |
| `SceneFacade` | base | Commit、snapshot、ViewData、权威碰撞查询 |
| `PreflightFacade` | base | 快速非权威预检 |
| `SliceFacade` | engine | 生产切片、进度和取消 |
| `PreflightFullFacade` | engine | 完整权威预检 |
| `RepairFacade` | engine | 显式修复与证据返回 |

## 3. ViewData v1.2 对齐

- `top` 与 `three_d` 共用 `SceneViewDataRequest`，按 `expected_scene_revision` 和 `max_bytes` 查询。
- 实例使用 `local_bounds_mm + world_matrix`，并分别携带 `mesh_identity`、
  `appearance_identity`、`preview_identity`。
- `top` 可返回实例级 `SurfacePreview`；`three_d` 可返回位置、法线、UV、索引、submesh、
  material 和 texture 数据。
- 多模型外观通过 `ViewInstance.appearance_identity` 引用 `SceneViewData.appearances`。
- 预算降级使用 `truncated + truncation_reason` 明示；纹理缺失、解码失败或无有效 UV 的
  fail-closed 行为由后续 `14B-03A` Provider 实现和测试，不在本任务伪造成功数据。

## 4. 自动门禁

- `FacadeContractUnitTests.cpp`：编译全部 Facade/DTO，验证 `ApiResult`、默认 DTO 与取消令牌。
- `ValidateFacadeHeaders.py`：检查 API 目录存在、单头文件不超过 200 行，并禁止 Qt/C ABI 类型渗入。
- `ValidateStage14BLayeringFeasibility.py`：把 `src/slicer_core/api/` 固定归入 base。
- CMake/CTest 已登记 `facade_contract_unit_tests` 与 `slicer_facade_header_contract_test`。

## 5. 边界

- `PM_SPI_VERSION=1`、11 个 `pm_*` 导出和 15 项能力不变。
- `p0.rgbwsv.2`、`R G B W S V`、uint8、`black_is_print` 不变。
- 不引入 Qt、OpenVDB 或第三方依赖；OpenVDB 默认关闭。
- 不修改生产 TIFF、RIP、Worker、DLL 或 UI。
- `14B-01A` 才落地 `slicer_base` / `slicer_engine` 构建拆分；`14B-02..04` 才提供实现。

## 6. 后续任务

下一任务为 `14B-01A`。完成两库拆分后，`14B-02`、`14B-03`、`14B-04` 可按文件所有权
并行实现；`14B-03A` 必须等待 `14B-02` 与 `14B-03` 完成。
