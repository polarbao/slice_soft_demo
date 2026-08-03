# TASKS_12E-09D 生产纹理厚度与单材料材质任务清单

> 文档状态：09D-01..06 COMPLETE
> 日期：2026-08-03

## 12E-09D-01 合同与配置映射

状态：COMPLETE

```text
[x] 新增 ProductionTextureControlState DTO；
[x] 冻结稳定策略、分区模式和 E_PRODUCTION_TEXTURE_* 错误码；
[x] 冻结 Legacy topSurfaceLayers、Global widthMm/mode 字段映射；
[x] 诊断映射不暴露任何生产配置写入路径；
[x] 定向合同单测与既有 Effective Config 单测通过；
[x] 不改变生产配置、切片输出或 TIFF 协议。
```

验证：

```powershell
cmake --build build-slicesoft/main --config Debug --target production_texture_settings_contract_unit_tests
ctest --test-dir build-slicesoft/main -C Debug -R "^(production_texture_settings_contract_unit_tests|production_effective_config_unit_tests)$" --output-on-failure
```

## 12E-09D-02 Production Texture Settings

状态：COMPLETE

```text
[x] 实现 requested/effective/backend 和 stale 状态；
[x] Legacy topSurfaceLayers 与有效 Z 厚度；
[x] Global widthMm 的 0.01 mm 量化与显式 partial_shell/all_texture；
[x] Profile 锁定、Global 未准入和非法请求 fail closed；
[x] session effective config 及 uiAudit.production.texture 同源；
[x] 未开启 override 的旧 Profile 保持原字段。
```

验证：

```powershell
cmake --build build-slicesoft/main --config Debug --target production_texture_settings_model_unit_tests production_effective_config_unit_tests
ctest --test-dir build-slicesoft/main -C Debug -R "^(production_texture_settings_(contract|model)_unit_tests|production_effective_config_unit_tests)$" --output-on-failure
cmake --build build-slicesoft/main --config Debug --target slicer_debug_ui
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test
```

## 12E-09D-03 Single Material Relief Resolver

状态：COMPLETE

```text
[x] W/V 原子字段组；
[x] modelMaterial、modelFill、materialProcessProfile、validation、preview 同步；
[x] 配置一致性校验与 fail-closed；
[x] E_SINGLE_MATERIAL_RELIEF_* 稳定负向错误；
[x] session effective config 与 uiAudit 同源；
[x] 单元、集成与 Qt self-test 通过。
```

验证：

```powershell
cmake --build build-slicesoft/main --config Debug --target single_material_relief_resolver_unit_tests production_effective_config_unit_tests
ctest --test-dir build-slicesoft/main -C Debug -R "^(single_material_relief_resolver_unit_tests|production_effective_config_unit_tests)$" --output-on-failure
cmake --build build-slicesoft/main --config Debug --target slicer_debug_ui
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test
```

## 12E-09D-04 Qt 生产控件

状态：COMPLETE

```text
[x] 右侧切片设置条件化控件；
[x] 诊断/生产视觉分离；
[x] stale、保存、回读、Profile 锁定；
[x] Legacy、Global、单材料按 Profile 条件显示；
[x] production-texture-controls UI Smoke PASS。
```

## 12E-09D-05 一键切片与状态证据

状态：COMPLETE

```text
[x] 单模型/scene effective config；
[x] 运行摘要和生产设置报告；
[x] UI Smoke；
[x] TIFF 原生预览保持生产真源；
[x] 一键切片 Effective Config 与 UI 值同源。
```

## 12E-09D-06 Release 矩阵与收口

状态：COMPLETE

```text
[x] Legacy 1/3/10；
[x] Global min/mid/allTexture；
[x] single relief W/V；
[x] RIP strict；
[x] REPORT、总览、上下文；
[x] Release 统一矩阵脚本与可复现摘要。
```

验证：

```powershell
.\scripts\run_12e_09d_production_texture_material_matrix.ps1 `
  -BuildDir build-slicesoft/main -Config Release
```

结果：6 个定向测试、Qt self-test、2 个 UI Smoke、Legacy/Global/W/V 8 个生产 case 与 RIP strict 全部 PASS。

## 停止条件

```text
不得在 03D 优先任务前擅自开始；
不得把诊断值直接写成生产值；
不得把 Legacy 层数描述成 Global 宽度；
不得补丁式修改纯白 RGB；
不得修改固定 TIFF 协议。
```
