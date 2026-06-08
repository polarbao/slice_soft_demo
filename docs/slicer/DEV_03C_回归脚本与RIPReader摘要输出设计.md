# DEV_03C_回归脚本与RIPReader摘要输出设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：03C  
> 所属模块：scripts / rip_reader_test  
> 建议提交目录：`docs/slicer/`

## 1. 技术目标

03C 只修改测试工具与脚本，不修改核心切片输出语义。

目标：

```text
run_regression.ps1 支持 quick / full / heavy
rip_reader_test 支持 --summary / --quiet
bad package 与 MaterialPolicy 语义校验继续保留
输出更短、更适合 Codex 与人工阅读
```

## 2. run_regression.ps1 参数

建议：

```powershell
param(
  [ValidateSet("quick", "full", "heavy")]
  [string]$Mode = "quick",

  [switch]$SkipBuild
)
```

保留兼容：

```powershell
[switch]$SkipHeavyRelief
[switch]$SkipHeavyTexture
```

## 3. Case 分组

建议分组：

```text
basicCases
storageCases
supportCases
textureSmallCases
materialPolicyCases
badCases
heavyReliefCases
heavyTextureCases
```

quick：

```text
basic + storage + support small + texture fallback + materialPolicy + bad
```

full：

```text
quick + heavyRelief + heavyTexture
```

heavy：

```text
heavyRelief + heavyTexture
```

## 4. Step 输出格式

建议增加耗时统计：

```text
== slicer samples/configs/...
PASS 12.34s
```

失败时输出：

```text
FAIL 12.34s: step name
```

## 5. rip_reader_test 参数

新增：

```text
--summary
--quiet
```

建议新增：

```cpp
enum class OutputMode {
    Verbose,
    Summary,
    Quiet
};
```

CLI parsing：

```text
--summary -> OutputMode::Summary
--quiet -> OutputMode::Quiet
```

## 6. 不做内容

不修改：

```text
TiffImageSpec
MaterialPolicy
Texture sampler
Support generator
Manifest schema
TIFF writer
TIFF reader storage logic
```

除非为了 summary 输出读取已存在字段。
