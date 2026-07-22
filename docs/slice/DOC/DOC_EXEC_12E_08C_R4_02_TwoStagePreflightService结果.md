# DOC_EXEC_12E-08C-R4-02 Two-stage Preflight Service 结果

> 文档状态：COMPLETE
> 日期：2026-07-22
> 原子任务：12E-08C-R4-02

## 1. 完成范围

已新增 backend-neutral `ModelPreflightService`，执行链为：

```text
config/source fast check
-> OBJ 非有限值与资源可达性检查
-> load_model_report 最终姿态导入
-> cache identity/cache lookup
-> AdaptSceneModelToTriangleMesh
-> strict topology + complete self-intersection audit
-> source/resource/transform/options 二次身份检查
-> immutable complete result cache
```

服务保持同步，不创建线程；不接 Qt、不启动 legacy/global pipeline、不修复模型、不写 TIFF、preview 或
production package。

## 2. 合同扩展

R4-01 合同兼容增加：

```text
ModelPreflightStatus::Cancelled -> cancelled；
ModelPreflightErrorCode::Cancelled -> E_12E_PREFLIGHT_CANCELLED。
```

R4-02 只产出共享诊断事实。`legacyAdmission/globalAdmission` 都保持
`E_12E_PREFLIGHT_NOT_RUN`，模式相关准入由 R4-03 负责。

## 3. Cache 与生命周期

cache identity 覆盖：

```text
source bytes；
MTL/贴图/3MF 已解析纹理资源内容；
modelTransform、autoOrient 和最终 bbox/选中姿态；
voxel/tolerance/完整自相交预算；
missingTexturePolicy；
algorithmVersion=model_preflight_service.1。
```

source/resource/transform/options 任一变化均 cache miss。完整审计之后再次读取 identity；运行期变化返回
`stale`。取消可在 import 前、full 前和 full 后阶段边界生效；cancelled/stale/import fatal/resource
fail-fast/audit incomplete 均不进入 reusable cache。

`warn_and_fallback` 的缺失资源继续 full audit 并返回 warning；`fail_fast` 在 fast 阶段 blocked。策略参与
cache identity，不能跨策略复用结果。

## 4. 测试覆盖

新增 `model_preflight_service_unit_tests`，覆盖：

```text
generated closed box PASS 与 cache hit；
source/resource/transform/options 变化导致 cache miss；
missing resource fail-fast 与 warn-and-fallback；
invalid import 与 OBJ non-finite stable code；
complete self-intersection budget 不足不得 PASS；
cancel before import/before full/after full；
运行中 source 变化返回 stale；
真实 xiao_ma OBJ、yecan/3 OBJ、Texture2D checker 3MF 完整审计 PASS。
```

## 5. 实际验证

```text
TDD RED：首次构建因 ModelPreflightService.h 不存在而按预期失败；
model_preflight_service_unit_tests：1/1 PASS；
model_preflight_contract + service + mesh_repair_preflight：3/3 PASS；
Debug 全量构建：PASS；
真实正向输入：2 个 OBJ + 1 个 Texture2D 3MF PASS；
git diff --check：PASS（提交前执行）。
```

`scripts/run_ci_quick.ps1` 已运行，Debug build、support shape、基础 regression 均推进，但仍停在既有 golden：

```text
material_process_top2 widthPx expected=48 actual=226
```

该失败与 R4-02 preflight service 无直接修改关系，未在本原子任务扩大范围修复，也不得表述为 Quick CI
通过。

## 6. 下一任务

下一允许原子任务为 `12E-08C-R4-03 Mode Admission and Pipeline Gate`。开始代码前需详细冻结：

```text
legacy/global issue-to-admission matrix；
CLI/pipeline facade 插入点；
fail-closed 与 no-fallback；
global core/writer 未启动断言；
legacy 默认 TIFF 不变性验证。
```
