# DEV_05A_MaterialProcessProfile与工艺验证报告设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：PRD_05A  
> 所属模块：MaterialProcessProfile / Reports / Regression  
> 建议提交目录：`docs/slicer/`

## 1. 技术目标

05A 不重写材料合成链路，而是在现有基础上新增：

```text
MaterialProcessProfileConfig
MaterialProcessReport
MaterialProfileCompare
Validation checks
Regression samples
```

## 2. 推荐数据结构

```cpp
struct MaterialProcessWhiteConfig {
    bool enabled{false};
    std::string mode{"underbase"};
    std::string coverage{"all_model"};
    std::uint8_t value{0};
    int expand_px{0};
    int shrink_px{0};
};

struct MaterialProcessVarnishConfig {
    bool enabled{false};
    std::string mode{"top_n_layers"};
    int top_layers{2};
    std::uint8_t value{0};
    std::string coverage{"model_surface"};
};

struct MaterialProcessValidationConfig {
    bool require_rgb_pixels{false};
    bool require_white_pixels{false};
    bool require_varnish_pixels{false};
    bool require_support_pixels{false};
    std::uint64_t max_unexpected_overlap_pixels{0};
};

struct MaterialProcessProfileConfig {
    bool enabled{false};
    std::string name;
    std::string target;
    MaterialProcessWhiteConfig white;
    MaterialProcessVarnishConfig varnish;
    MaterialProcessValidationConfig validation;
};
```

## 3. 配置解析

新增：

```text
materialProcessProfile
```

兼容策略：

```text
如果 materialProcessProfile.enabled=false:
  不影响现有 MaterialPolicy。

如果 enabled=true:
  第一版只增加 report/validation。
```

## 4. Report 生成

新增：

```text
write_material_process_report(...)
```

输入：

```text
SliceReport
MaterialPolicyReport
MaterialRoleMappingReport
SupportReport
TextureReport
Config
```

输出：

```text
reports/material_process_report.json
```

## 5. 核心统计

需要统计：

```text
rgbPrintPixels
whitePrintPixels
varnishPrintPixels
supportPrintPixels
perLayer rgb/white/varnish/support
varnishActiveLayerIndices
varnishTopLayerCheck
whiteUnderbaseCoverage
validationFailures
```

## 6. Profile Compare

新增脚本：

```text
scripts/compare_material_profiles.ps1
```

比较：

```text
material_process_report.json
slice_report.json
material_policy_report.json
```

输出：

```text
reports/material_profile_compare_report.json
```

## 7. 与现有模块关系

```text
MaterialRoleMapping:
  输入材料 role

MaterialPolicy:
  实际材料组合执行

MaterialProcessProfile:
  工艺参数命名、验收、报告、对比
```

05A 不应该修改：

```text
3MF importer
OBJ importer
TextureSampler
TIFF writer
RIP reader
Support generator
```

除非为了 report 增补必要统计。

## 8. Regression

新增 case 分组：

```text
materialProcessCases
```

加入 quick：

```text
nail_rgb_white_varnish_top1
nail_rgb_white_varnish_top2
three_mf_texture_rgb_white_varnish
obj_mtl_texture_rgb_white_varnish
```

## 9. Warning

建议 warning：

```text
E_MATERIAL_PROCESS_PROFILE_EMPTY_RGB
E_MATERIAL_PROCESS_PROFILE_EMPTY_WHITE
E_MATERIAL_PROCESS_PROFILE_EMPTY_VARNISH
E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_VARNISH_LAYER
E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW
```

## 10. 实施顺序

```text
1. 新增 MaterialProcessProfileConfig；
2. 解析 materialProcessProfile；
3. 生成 material_process_report；
4. 增加 topLayers / underbase 基础校验；
5. 增加 profile 样例；
6. 增加 compare 脚本；
7. 接入 quick regression；
8. 生成 REPORT_05A。
```
