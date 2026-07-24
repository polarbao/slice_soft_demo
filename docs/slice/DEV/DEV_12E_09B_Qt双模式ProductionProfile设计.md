# DEV 12E-09B Qt 双模式 Production Profile 设计

> 状态：READY FOR IMPLEMENTATION
> 日期：2026-07-23

## 1. 架构边界

```text
Qt UI
  -> UI-owned ProductSliceModeState / ProfileCapabilityState
  -> EffectiveConfigGenerator
  -> SlicePreflightCoordinator / ProcessRunner
  -> slicer_cli
  -> SlicePipelineRouter
  -> Legacy or Global production layer DTO
  -> shared RGBWSV writer/package/report/preview
```

UI 不引用 OpenVDB 类型，不复制 topology/admission 规则，不直接构造 TIFF。

## 2. 建议模块

| 模块 | 职责 |
|---|---|
| `ProductionModeCatalog` | 定义中文模式名、稳定配置值和可选 Profile |
| `ProductionModeUiDto` | requested/effective/admission/output/fallback/resource 状态 |
| `EffectiveConfigGenerator` | 把模式、Profile 和 capability lock 写入 session config |
| `ConfigValidator` | UI 侧快速校验；核心 parser/validator 仍是最终真源 |
| `QuickConfigPanel`/配置页 | 中文模式选择、Profile 和禁用原因 |
| `MainWindow` | 一键切片读取当前选择并启动既有 preflight/process 流程 |
| `ProcessRunner`/现有 coordinator | 生命周期、取消、进程结果和 session identity |
| `PreviewWorkspace`/报告面板 | 只加载本次 package，展示 requested/effective 和材料结果 |

具体类名可按现有目录职责微调，但不得把业务规则塞进 `MainWindow.cpp`。

## 3. 配置合同

生产 Effective Config 至少包含：

```json
{
  "slicePipeline": {
    "mode": "legacy"
  },
  "materialProcessProfile": {
    "target": "uv_relief_obj_mtl_texture"
  }
}
```

Global 示例：

```json
{
  "slicePipeline": {
    "mode": "global_surface_shell"
  },
  "materialProcessProfile": {
    "target": "global_surface_shell_material_parity_candidate"
  }
}
```

实际字段必须以当前 parser 和 08D Profile fixture 为准，不为 UI 新造第二套核心 schema。生成路径固定为：

```text
output/ui_sessions/<session>/slice_config.effective.json
```

UI 只修改 session copy，不覆盖 `samples/configs` fixture。

## 4. 能力锁定

能力目录必须由稳定 Profile ID 映射，不按控件当前值猜测：

| Profile | RGB | W | S | V | 限制 |
|---|---:|---:|---:|---:|---|
| Legacy | 按现有配置 | 按现有配置 | 按现有配置 | 按现有配置 | 保持现有产品能力 |
| Global restricted | 是 | 是 | 否 | 否 | S/V 控件禁用并清除不生效 override |
| Global material parity | 是 | 是 | lower/internal void | surface/outer | 不开放 upper/both/full-vertical 和高级 shape/bridge |

禁用控件的历史值不能悄悄进入 Effective Config。模式/Profile 切换应产生可审计 diff。

## 5. 状态机

```text
Idle
  -> ConfigDirty
  -> PreflightRunning
  -> Blocked | Admitted
  -> SliceRunning
  -> Failed | PackageReady
```

规则：

```text
模型或关键配置变化：Admitted/PackageReady -> ConfigDirty；
Global blocked：不启动 slicer_cli writer；
进程失败：清空 pending package，不自动加载旧目录；
PackageReady：校验 manifest 的 requested/effective mode 和 productionOutputWritten；
requested != effective：稳定失败；
fallbackApplied != false：稳定失败。
```

## 6. 09A/09B 复用边界

```text
09A Effective Config：诊断 width/modelFill/派生阈值；
09B Effective Config：生产 mode/Profile/capability lock；
09A worker：当前模型 diagnostic analysis；
09B runner：生产 CLI 进程；
09A preview：同层语义分区；
09B preview：既有生产 package 的结果绑定。
```

共享 `ConfigDocument`、`EffectiveConfigGenerator` 和帮助系统，但字段所有权分开，避免重复控件。

## 7. 错误处理

应映射已有稳定核心错误：

```text
ModeUnsupported；
ConfigMismatch；
GlobalNotAdmitted；
GlobalTopologyBlocked；
ProductionTiffRequired；
SilentFallbackForbidden。
```

UI 中文说明不能吞掉稳定错误码。日志和诊断页保留 code/context。

## 8. 验证设计

```text
L1：ProductionModeCatalog/DTO/Effective Config 单测；
L2：Legacy/Global restricted/Global parity 配置生成和负向校验；
L3：MainWindow/QuickConfigPanel self-test；
L4：真实 xiao_ma/yecan 一键切片、TIFF/manifest/RIP strict；
L5：topology blocked/no-fallback/stale-session 负向 smoke；
L6：1280x720、1440x900、1920x1080 UI smoke。
```

默认构建保持 OpenVDB OFF。09B 不要求把 OpenVDB ON 作为普通 UI 的可用条件。

## 9. 回滚

通过 capability flag 隐藏 Global 产品入口并恢复 Legacy 默认，不删除 08D 核心能力，不改变 fixture 和协议。
