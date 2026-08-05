# REPORT 14B-04 SliceFacade 提交、进度与取消适配当前状态

> 阶段：Stage 14B-04  
> 日期：2026-08-05  
> 状态：✅ COMPLETE；已接入正式 CMake target/CTest，CLI 生产入口切换仍归 14B-05  
> 权威输入：14B PREP、TASKS_14、CODEX_PROMPT_14、DEV_14、`slicer_cancel_contract`、`slicer_capability_dtos`

## 1. 本任务边界

本任务只实现 engine 侧 `SliceFacade` 适配层：

1. 校验 `jobId`、`correlationId`、已提交 `sceneHash`、effective config 和调用方指定的 package 路径；
2. 复用既有 `RunMultiModelProductionService()` 生产入口，不复制切片、材料、TIFF 或发布规则；
3. 将既有同步 `SliceRunProgress` 转换为 `ProgressEvent`，保证对外百分比单调不回退；
4. 在既有阶段/逐层进度回调边界检查 `ICancelToken`，返回冻结错误码 `PM-SLICER-CANCELLED-0070`；
5. 不修改生产 Writer、默认 Legacy/OpenVDB 选择、RGBWSV 通道语义和生产包协议。

本任务不实现 14D-04 的深度取消贯穿、2000 ms Worker 取消上限、Windows Job Object 兜底、
staging 最终状态机或 CLI/Worker 接线。

## 2. 实现结果

### 2.1 提交合同

`ProductionSliceFacadeFactory` 通过 `ReadSceneEffectiveConfig()` 读取权威 effective config，并将其中的
`identity.sceneHash` 与 `sliceContract.outputPackageDir` 交给适配器。适配器只接受相同的已提交
`sceneHash` 和 package 路径：

- sceneHash 过期：`PM-SLICER-LAYOUT-0022`；
- 必填身份或路径缺失：`PM-SLICER-PROFILE-0030`；
- package 路径与 effective config 不同：`PM-SLICER-PROFILE-0031`；
- 生产结果返回其他 package：`PM-SLICER-CONTRACT-0060`。

### 2.2 进度适配

既有生产阶段名称、current/total 和 percent 原样进入 Facade 边界；适配器只执行 0..100 夹取和
单调化，不新增切片阶段，也不改变生产执行顺序。调用方进度观察器抛出的异常不会越过 Facade；
即使现有 package-write 捕获了回调异常，适配器也会恢复为 `PM-SLICER-INTERNAL-0099`。

### 2.3 协作取消

取消检查位于：

- 解析 effective config 前；
- 调用既有生产入口前；
- 现有阶段/逐层进度回调边界。

package-write 当前会把回调异常翻译为生产包失败，因此适配器在生产入口返回后再次检查 token，
将该路径恢复为 `PM-SLICER-CANCELLED-0070`。`completed` 表示既有入口已完成发布，此边界不再把
成功结果反转为取消，避免出现“包已发布但作业报告 cancelled”的不一致。

### 2.4 TIFF 不变量

本任务没有调用或修改任何 TIFF Writer，也没有重建六通道像素。默认工厂只调用既有
`RunMultiModelProductionService()`，完成后只读 `manifest.json` 形成 `SliceResult`。因此以下冻结项
保持不变：

```text
schema       p0.rgbwsv.2
channels     R G B W S V
bitDepth     uint8
polarity     black_is_print
default      既有 Writer / 既有 Legacy 默认 / OpenVDB 不静默切换
```

## 3. 正式构建与验证

### 3.1 14B-04 单元验证

正式 CMake target `slice_facade_14b04_unit_tests` 已在 Debug/Release 编译并运行，结果
`PASS`。覆盖：

- 成功提交及 effective config 路径透传；
- 进度单调化；
- 运行前取消、运行中既有进度边界取消；
- package-write 吞并取消异常后的错误恢复；
- sceneHash/package 身份 fail-closed；
- 进度观察器异常隔离；
- 成功结果 package 身份复核；
- 生产 runner 写入的 TIFF 字节在经过 Facade 后保持逐字节一致。

### 3.2 生产绑定验证

`slice_facade_14b04_production_binding_test` 已接入正式 CMake/CTest，并在 Debug/Release 与
`slicer_engine`、`slicer_base` 完成链接和运行，结果 `PASS`。

### 3.3 合同与既有生产回归

以下正式验证均通过：

```text
python tests/contracts/ValidateStage14B04SliceFacade.py
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py

ctest --test-dir build-slicesoft/main -C Debug \
  -R "^(slicer_stage14b04_slice_facade_contract_test|slice_facade_14b04_unit_tests|slice_facade_14b04_production_binding_test)$" \
  --output-on-failure

ctest --test-dir build-slicesoft/main -C Release \
  -R "^(slicer_stage14b04_slice_facade_contract_test|slice_facade_14b04_unit_tests|slice_facade_14b04_production_binding_test)$" \
  --output-on-failure

ctest --test-dir build-slicesoft/main -C Debug \
  -R "^(multi_model_package_writer_unit_tests|multi_model_production_service_unit_tests|rgbwsv_production_package_writer_unit_tests|production_package_result_unit_tests)$" \
  --output-on-failure

ctest --test-dir build-slicesoft/main -C Release \
  -R "^(multi_model_package_writer_unit_tests|multi_model_production_service_unit_tests|rgbwsv_production_package_writer_unit_tests|production_package_result_unit_tests)$" \
  --output-on-failure
```

14B-04 定向门禁在 Debug/Release 均为 7/7 PASS；既有生产回归在 Debug/Release 均为
4/4 PASS。

## 4. 文件清单

实现：

- `src/slicer_core/engine/SliceFacadeAdapter.h`
- `src/slicer_core/engine/SliceFacadeAdapter.cpp`
- `src/slicer_core/engine/ProductionSliceFacadeFactory.h`
- `src/slicer_core/engine/ProductionSliceFacadeFactory.cpp`

正式测试：

- `tests/contracts/Stage14B04SliceFacadeUnitTests.cpp`
- `tests/contracts/Stage14B04ProductionBindingLinkTest.cpp`
- `tests/contracts/ValidateStage14B04SliceFacade.py`

## 5. 未决风险与后续边界

1. CLI 尚未改走 Facade；该入口切换归 14B-05，本任务不提前改写调用方。
2. 当前取消粒度受既有进度回调密度限制。模型加载或没有现有进度回调的长循环不能保证 2 秒内退出；
   该能力必须在 14D-04 深度贯穿 token 后验收。
3. 本轮逐字节证据由“生产 Writer 零改动 + 同一生产入口 + 适配器字节防篡改单测 + 既有 Writer/
   Service 回归”构成；14B-05 CLI 切换后仍应补跑正式 golden 包的前后 SHA-256 对比。
4. staging 清理失败、Worker 强杀和取消/发布竞态的最终裁决仍由 14D-04/14D-05 负责，本任务不提前
   建立第二套状态机。
