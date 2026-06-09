# PRD_07B_UI自动化SmokeTest与配置编辑器收口

> 文档版本：v0.1  
> 所属模块：Qt Debug UI / Smoke Test / Config Editor  
> 建议目录：`docs/slicer/`

## 1. 产品目标

07B 目标：

```text
让 slicer_debug_ui 的关键交互具备脚本化 smoke test；
让配置编辑器具备覆盖保护、字段枚举、差异预览；
让 preview overlay 的数据源 schema 更稳定。
```

## 2. 必须支持功能

### 2.1 UI Smoke Test Mode

新增命令行参数：

```text
--ui-smoke-test
--case <caseName>
--config <path>
--package <path>
--package-a <path>
--package-b <path>
--output <path>
--yes
```

第一版支持 cases：

```text
startup
load-package
save-as-config
chart-load
overlay-load
compare-profiles
```

### 2.2 Save 覆盖确认

必须补齐：

```text
Save 覆盖当前文件前弹确认
Save As 覆盖已有文件前弹确认
ui-smoke-test 模式可用 --yes 自动确认
```

### 2.3 Config Diff Preview

新增 `ConfigDiffPanel`：

```text
path | oldValue | newValue
```

至少覆盖：

```text
materialProcessProfile
materialPolicy
materialRoleMapping
support
preview
```

### 2.4 Enum ComboBox

枚举字段使用下拉：

```text
output.storageMode
materialRoleMapping.role
support.mode
materialPolicy.white.mode
materialPolicy.varnish.mode
```

### 2.5 Preview Report Schema 固化

支持标准：

```json
{
  "schema": "p0.preview_report.1",
  "files": [
    {
      "path": "preview/model_rgb_000001.png",
      "channel": "rgb",
      "layerIndex": 1,
      "kind": "single"
    }
  ]
}
```

同时兼容旧字段：

```text
files
generated
previewFiles
```

## 3. 验收标准

```text
--self-test pass
--ui-smoke-test startup pass
--ui-smoke-test save-as-config pass
--ui-smoke-test chart-load pass
--ui-smoke-test overlay-load pass
--ui-smoke-test compare-profiles pass
Save / Save As 覆盖确认可用
ConfigDiffPanel 可显示字段变更
PreviewOverlayPanel 优先使用 p0.preview_report.1
run_regression.ps1 -Mode quick pass
slicer_core 输出协议不变
```
