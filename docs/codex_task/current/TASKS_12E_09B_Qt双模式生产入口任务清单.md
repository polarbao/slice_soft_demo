# TASKS 12E-09B Qt 双模式生产入口任务清单

> 状态：09B-04 COMPLETE / 09B-05 READY
> 日期：2026-07-24
> 规则：每次只执行用户明确授权的原子任务

## 1. 固定边界

```text
Legacy 默认；
Global 显式 opt-in；
产品模式仅 legacy/global_surface_shell；
OpenVDB 不作为第三模式；
无静默 fallback；
repair 默认关闭；
共享 p0.rgbwsv.2 RGBWSV writer；
不改变 R G B W S V / uint8 / black_is_print。
```

## 2. 09B-01 能力目录与 UI DTO

状态：COMPLETE

目标：

```text
建立产品模式和 Profile 能力目录；
建立 requested/effective/admission/output/fallback/resource UI DTO；
不新增可见控件；
不运行生产切片。
```

完成标准：

```text
Legacy、Global restricted、Global material parity 能力单测；
未知 Profile fail-closed；
Global 不是默认；
DTO 不包含 OpenVDB 类型。
```

实际落点：

```text
apps/slicer_debug_ui/services/ProductionModeCatalog.h/.cpp；
tests/unit/production_mode_catalog/Main.cpp；
能力锁定版本 = slicesoft.ui.production_capability.12e_09b.1；
Legacy 默认；两个 Global Profile 仅显式选择；未知 Profile fail-closed；
DTO 默认 productionOutputWritten=false、fallbackApplied=false、resourceCost=not_evaluated。
```

验证：

```text
production_mode_catalog_unit_tests PASS；
slice_pipeline_router_unit_tests PASS；
global_surface_shell_production_pipeline_unit_tests PASS；
slicer_debug_ui --self-test PASS。
```

## 3. 09B-02 Production Effective Config

状态：COMPLETE

目标：

```text
把 mode/Profile/capability lock 写入 session Effective Config；
清除不支持的 stale override；
保留 requested/effective audit；
不覆盖 samples/configs fixture。
```

完成标准：正向/负向 config 单测、atomic session write、diff 可审计。

实际落点：

```text
EffectiveConfigRequest 增加 production selection；
Legacy 显式写 mode 并保留现有 Profile；
Global 按只读源 Profile 锁定合同并清除 stale override；
uiAudit.production 记录 requested/effective、能力版本、disabledOverrides 和 session identity；
session 文件固定为 slice_config.effective.json；
QSaveFile 原子提交，负向校验不替换已有文件；
新增 production_effective_config_unit_tests。
```

验证：

```text
production_effective_config_unit_tests PASS；
production_mode_catalog_unit_tests PASS；
slice_pipeline_router_unit_tests PASS；
global_surface_shell_production_pipeline_unit_tests PASS；
generated-effective-config UI smoke PASS；
slicer_debug_ui --self-test PASS。
```

## 4. 09B-03 中文选择器与状态

状态：COMPLETE

目标：

```text
增加“传统切片/全局纹理壳层”选择器；
增加 Global Profile、准入、阻断和资源提示；
按能力禁用不支持控件并显示 tooltip；
普通页不暴露 backend。
```

完成标准：self-test、最长中文和三窗口尺寸 smoke。

实际落点：

```text
新增 ProductionModePanel，默认传统切片，Global 必须显式选择；
Global 仅显示 restricted/material-parity 两个获准 Profile；
能力范围、准入/过期、阻断和资源开销均使用中文显示；
Global Profile 锁定普通材料、支撑和光油控件并显示禁用原因；
普通配置页隐藏 OpenVDB backend 开关，独立诊断按钮不变；
新增 production-mode-selector UI smoke。
```

验证：

```text
slicer_debug_ui Debug build PASS；
production-mode-selector 1280x720、1440x900、1920x1080 PASS；
slicer_debug_ui --self-test PASS；
production_mode_catalog_unit_tests PASS；
production_effective_config_unit_tests PASS。
```

## 5. 09B-04 一键切片路由与 no-fallback

状态：COMPLETE

目标：

```text
现有一键切片使用当前模式和 session Effective Config；
复用 preflight/coordinator/ProcessRunner；
模式或关键输入变化使 admission stale；
失败不加载旧 package；
Global blocked 不自动执行 Legacy。
```

完成标准：Legacy、Global、blocked、invalid、retry 进程级测试。

实际落点：

```text
一键切片和“运行切片”均读取当前 Legacy/Global 模式与 Global Profile；
Global 从能力目录映射的只读源 Profile 生成 session Effective Config；
模型、输出、DPI、缩放、姿态和 preview 等允许项投影到 session copy，源 fixture 不修改；
Legacy/Global production 共用 ModelPreflightController、SlicePreflightCoordinator、ProcessRunner 和 slicer_cli；
Global production 使用普通 slicer_cli 中已准入的 08D pipeline，不误绑独立 OpenVDB backend 探针；
ProductionSliceRunSession 固定 session/config/package 身份，失败、阻断、stale 或进程启动失败均清空；
Global blocked 不启动 writer，不自动切换 Legacy；失败不加载旧 package；
新增 production_slice_route_process_tests。
```

验证：

```text
Debug 全量构建 PASS；
Debug CTest 53/53 PASS；
production_slice_route_process_tests：
  Legacy success/failure/retry PASS；
  Global success/no-fallback PASS；
  invalid Profile/旧 package 隔离 PASS；
model-preflight-one-click-gate：
  Global production clean admitted；
  Global topology blocked；
  process-start-before-admission=0；
model-preflight-lifecycle PASS；
production-mode-selector PASS；
slicer_debug_ui --self-test PASS；
Quick CI PASS。
```

## 6. 09B-05 生产结果与资源提示

状态：READY

目标：

```text
加载当前 session package；
显示 requested/effective、productionOutputWritten 和 fallback=false；
绑定现有 preview/report/timing；
显示本次实际时间/内存和 Global 高开销提示。
```

完成标准：package identity、manifest mode、preview/report 同源测试。

## 7. 09B-06 阶段收口

状态：PREPARED / WAIT 09B-05

目标：

```text
运行 Debug/Release、self-test、UI smoke、Quick CI；
运行 xiao_ma/yecan Legacy/Global 真实模型矩阵；
验证 TIFF、manifest、preview/report、RIP strict；
更新用户手册、状态报告、索引和上下文。
```

状态报告固定为：

```text
docs/slice/REPORT/REPORT_12E_09B_Qt双模式生产入口当前状态.md
```

完成标准：所有验收通过，或明确记录 NO-GO 与回滚状态。

## 8. 09A 边界

```text
09A-02..06 不因 09B 启动而自动完成；
09B 不重复实现诊断 width/modelFill/派生阈值；
09A-05 仍是 12E-10A 的前置；
09B-06 不能用生产 smoke 冒充 09A diagnostic smoke。
```

## 9. 每任务验证

开始和结束至少运行：

```powershell
git branch --show-current
git status --short
git diff --check
```

C++/Qt 修改按 `.agents/docs/build-and-test.md` 运行定向 build/test；最终任务运行完整 09B DEMO 矩阵。
