# DOC_PREP_12E-08C-R4-02 Two-stage Preflight Service 准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-21
> 原子任务：12E-08C-R4-02
> 前置任务：R4-01 Model Preflight Contract COMPLETE

## 1. 任务目标

R4-02 只实现一个 backend-neutral、同步可测试的两阶段模型预检服务：先快速拒绝不可导入输入，再对最终
姿态后的几何执行完整审计，并形成可缓存、可判定过期、可取消且结果确定的诊断事实。

本任务不计算 legacy/global 的最终准入结论，不接 Qt，不启动任一切片 pipeline，不修复模型，不写 TIFF
或 package。模式准入和 pipeline gate 属于 R4-03。

## 2. 已确认代码边界

### 2.1 可复用入口

```text
load_slice_config / validate_slice_config：配置解析与基础校验；
load_model_report：OBJ/STL/3MF 导入、modelTransform 和 autoOrient；
AdaptSceneModelToTriangleMesh：最终姿态 SceneModel 到索引三角网格；
EvaluateMeshRepairPreflight：topology、robustness、eligibility 和完整自相交证据；
ComputeMeshRepairSha256：canonical SHA-256；
ModelPreflightCacheIdentity/Result：R4-01 已冻结合同；
BuildModelPreflightReport：R4-01 已冻结报告序列化。
```

`load_model_report` 返回的三角形已经应用 `modelTransform` 和 `autoOrient`。R4-02 不得再次变换顶点；
`finalTransform` 只用于 identity/report，避免重复旋转、缩放或落台。

### 2.2 当前缺口

```text
没有统一 ModelPreflightService；
importer 仍是同步整体导入，尚无独立轻量资源解析器；
完整自相交分析没有循环内取消接口；
没有 source/resource 双读变化检测；
没有 fast/full 结果合并、排序和 stale 规则；
没有跨 UI/pipeline 的预检缓存实现。
```

因此 R4-02 的“快速阶段”定义为文件/配置/格式/可读性/基础资源和导入结果检查，不承诺零解析成本；取消只
保证阶段边界及时生效。若后续需要中断完整 BVH 内部循环，应另立性能/响应性任务，不能在本任务扩大范围。

## 3. 冻结执行链

```text
Build request
  -> FastImportCheck
     1. config/path/format/readability
     2. source bytes hash
     3. load_model_report
     4. empty/non-finite/basic count/resource availability
  -> cancellation checkpoint
  -> FullTransformedPreflight
     1. AdaptSceneModelToTriangleMesh（不重复 transform）
     2. topology/robustness/eligibility
     3. complete self-intersection audit
     4. attribute/resource summary
  -> cancellation checkpoint
  -> re-hash source/resources
  -> stale check
  -> deterministic merge/sort
  -> cache eligible result
```

任何阶段异常必须转换为稳定 `E_12E_PREFLIGHT_*` issue；不得把 importer 异常文本作为唯一机器合同。

## 4. 计划接口与文件

计划新增：

```text
src/slicer_core/preflight/ModelPreflightService.h
src/slicer_core/preflight/ModelPreflightService.cpp
tests/unit/model_preflight_service/main.cpp
tests/fixtures/model_preflight/*（仅生成或小型可提交 fixture）
tests/golden/expected/12e_r4_model_preflight_service_projection.json
```

R4-01 合同尚无独立取消状态。R4-02 实施前先以兼容扩展方式在
`ModelPreflightTypes.h/.cpp` 增加 `ModelPreflightStatus::Cancelled` 和
`ModelPreflightErrorCode::Cancelled`（稳定名 `E_12E_PREFLIGHT_CANCELLED`），同步合同 unit；不删除或重命名
R4-01 字段和枚举值。

计划最小接口：

```cpp
struct ModelPreflightOptions;
struct ModelPreflightRequest;
struct ModelPreflightExecutionResult;

ModelPreflightExecutionResult RunModelPreflight(
    const ModelPreflightRequest& request);
```

`ModelPreflightRequest` 至少携带 config path、算法选项、请求 generation 和只读取消令牌；执行结果回显
generation、cache identity/key、fast/full 是否完成及 `ModelPreflightResult`。Public 接口必须使用 Doxygen，
core 不得出现 Qt 类型。

## 5. Fast/Full 合并规则

按以下优先级确定总状态：

```text
cancelled/stale > import fatal > non-finite/resource fatal > audit incomplete
  > complete diagnostic blocker > warning > passed
```

具体规则：

1. fast 阶段 fatal 后不启动 full；
2. 用户取消时返回 `cancelled + E_12E_PREFLIGHT_CANCELLED`，结果不得进入 cache；
3. source/resource 在运行期间变化时返回 `stale`，结果不得进入 cache；
4. 完整自相交审计预算不足、异常或未执行时返回 `blocked + E_12E_PREFLIGHT_AUDIT_INCOMPLETE`；
5. 完整审计完成且无 fatal issue 才允许 shared diagnostic 状态为 `passed/warning`；
6. issue 按 severity、code、category、context canonical JSON 排序，重复 code/context 合并 count；
7. R4-02 不解释 mode policy：两种 admission 保持 blocked，并携带
   `E_12E_PREFLIGHT_NOT_RUN`，由 R4-03 替换；
8. `productionOutputWritten` 始终为 false。

## 6. Identity、Cache 与 Stale

### 6.1 Identity 输入

```text
sourceHash：模型源文件字节；
resourceHash：按规范化相对路径排序后的 MTL/贴图/3MF 内嵌资源内容；
transformHash：unit、scale、rotation、translation、autoOrient 配置及最终选中姿态；
optionsHash：voxel/tolerance/self-intersection budget/adapter options；
algorithmVersion：R4-02 service 与诊断算法版本。
```

模式不进入共享几何诊断 cache key；R4-03 切换模式时复用同一 fresh 诊断事实并重算 admission。若未来某个
模式改变诊断选项，应把该选项放入 `optionsHash`，而不是隐式依赖 UI 状态。

### 6.2 Cache 行为

```text
同 identity：命中 immutable complete result；
任一 identity 字段变化：miss，并将旧 UI result 标记 stale；
cancelled/stale/audit-incomplete/import-fatal：不得写入 PASS cache；
cache 只保存在进程内；本任务不实现跨进程磁盘缓存；
cache value 不持有 Qt 对象、writer 或切片进程句柄。
```

### 6.3 运行期变化检测

source 和已解析 resource 在 full 前后各计算一次 hash。两次 identity 不一致时，当前结果只返回 stale，
不得以旧 hash 放行。文件时间戳只能作为快速提示，不能替代内容 hash。

## 7. Cancel 与生命周期

R4-02 使用调用方拥有的只读取消状态，在 source hash、import、adapt、full audit 和最终 re-hash 的阶段边界
检查。取消结果不抛出跨线程异常、不缓存、不生成 PASS report。generation 由调用方递增；调用方只接受
“generation 与当前请求一致且 cache key 仍匹配”的结果。

R4-02 不创建线程。R4-04 Qt controller 决定 worker/thread 生命周期，并消费本服务的同步接口。

## 8. Fixture 与验收矩阵

### 8.1 小型自动化 fixture

```text
generated closed mesh：完整 PASS；
empty/invalid input：IMPORT_INVALID；
non-finite vertex：NON_FINITE_GEOMETRY；
missing texture：RESOURCE_MISSING；
open/non-manifold/self-intersection：完整诊断 issue；
受限 candidate budget：AUDIT_INCOMPLETE，绝不 PASS；
运行中 source/resource/transform/options 改变：STALE/cache miss；
cancel before full / cancel after full：不缓存、不 PASS；
两次相同输入：结果 projection 与 cache key 相同。
```

### 8.2 真实正向输入

主验证模型：

```text
model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj
```

独立复核：

```text
model/obj/yecan/3.obj
samples/models/3mf/texture2d_checker_cube.3mf
```

扩展覆盖可使用 `xiao_ma_wu_yu_new` 其余四个 OBJ 与 `yecan/4.obj`。`yecan/4.obj` 及其资源当前为用户
未跟踪资产，测试或提交不得复制、覆盖或纳入 Git；本地存在时只允许只读复核。

`nai_you/aishen/meigui` 只作负向 blocker，不得在 R4-02 自动修复，也不得替代上述正向输入。

## 9. 验证命令

R4-02 实施时至少执行：

```powershell
cmake --build build --config Debug --target model_preflight_service_unit_tests
ctest --test-dir build -C Debug -R "^(model_preflight_contract_unit_tests|model_preflight_service_unit_tests|mesh_repair_preflight_unit_tests)$" --output-on-failure
cmake --build build --config Debug
git diff --check
git status --short
```

真实模型验证使用 Release `mesh_repair_preflight` 或新增 preflight CLI fixture runner，只读模型资产且输出到
ignored `output/benchmarks`。`scripts/run_ci_quick.ps1` 仍需运行并记录，但当前已知
`material_process_top2 widthPx expected=48 actual=226` baseline 不能被误报为本任务通过；若该 baseline 未被
独立修复，R4-02 只记录为外部 blocker。

## 10. 停止条件

```text
需要修改生产 writer/TIFF 协议：停止；
需要放宽 global strict 或 silent fallback：停止；
需要新增第三方库：停止并先完成候选比较；
需要在本任务实现 Qt 线程或 UI：停止，留给 R4-04；
完整审计无法给出 complete 证据：返回 blocked，不降级 sampled PASS；
真实模型需要复杂重建：只记录 blocker，不修改用户资产。
```

## 11. 准备结论

R4-02 的代码落点、输入边界、变换语义、fast/full 合并、cache/stale/cancel、fixture 和验证命令均已冻结。
该任务已达到 `READY FOR DEVELOPMENT`，但尚未开始代码实现；必须由用户明确下达 R4-02 开发指令后执行。
