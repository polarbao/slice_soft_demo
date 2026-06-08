# TASKS_06_v0.2_3MF与OBJ_MTL多材料任务清单

> 文档版本：v0.2  
> 文档状态：Codex Task List  
> 建议提交目录：`docs/slicer/`

## Milestone 06-0：阅读确认

- [x] 阅读 `REPORT_03C_回归与RIP输出收口当前实现状态.md`
- [x] 阅读 06 v0.2 文档
- [x] 确认不做 OpenVDB / Qt / RIP 半色调
- [x] 确认不改变 p0.rgbwsv.2 输出协议
- [x] 确认 06 同时包含 3MF 与 OBJ/MTL 多材料映射

## Milestone 06-1：MaterialRoleMapping

- [x] 新增 materialRoleMapping 配置
- [x] 新增 MaterialRole enum
- [x] 支持 role = rgb / white / varnish / ignore / support_candidate
- [x] 默认 role = rgb
- [x] 默认 allowInputSupportMaterial=false
- [x] 支持规则 matchNameContains
- [x] 输出 material_role_mapping_report.json

## Milestone 06-2：OBJ/MTL Material Mapping

- [x] 读取 OBJ usemtl
- [x] 确认 face material assignment
- [x] 读取 MTL newmtl / Kd / map_Kd
- [x] 将 MaterialInfo 映射到 MaterialRole
- [x] RGB role 使用 texture / Kd / fallback
- [x] white role 写 W
- [x] varnish role 写 V
- [x] ignore role 不输出
- [x] 输出 obj_mtl_material_report.json

## Milestone 06-3：3MF Package 读取

- [x] 确认 ZIP 解析方案
- [x] 确认 XML 解析方案
- [x] 更新 CMake：已确认无需新增 target 或源文件清单
- [x] 打开 3MF zip
- [x] 读取 `[Content_Types].xml`
- [x] 读取 `_rels/.rels`
- [x] 定位 `3D/3dmodel.model`
- [x] 防 path traversal
- [x] 限制解压大小与文件数量

## Milestone 06-4：3MF XML 解析

- [x] 解析 model unit
- [x] 解析 resources
- [x] 解析 object
- [x] 解析 mesh vertices
- [x] 解析 mesh triangles
- [x] 解析 build items
- [x] 解析 components
- [x] 解析 component transform
- [x] 解析 basematerials / color
- [x] 调用 MaterialRoleMapping

## Milestone 06-5：转换到现有 Pipeline

- [x] 转为 ModelReport.triangles
- [x] 转为 material_infos
- [x] 转为 triangle_textures.material_name
- [x] 复用 slicing modes
- [x] 复用 TextureSampler
- [x] 复用 MaterialPolicy
- [x] 复用 Support
- [x] 复用 p0.rgbwsv.2 writer/reader

## Milestone 06-6：Reports

- [x] 新增 material_role_mapping_report.json
- [x] 新增 obj_mtl_material_report.json
- [x] 新增 three_mf_report.json
- [x] manifest 增加 source.format
- [x] model_report.format 正确记录 obj / 3mf

## Milestone 06-7：样例与回归

- [x] 新增 3MF single RGB 样例
- [x] 新增 3MF multi object transform 样例
- [x] 新增 3MF multi material RGB/W/V 样例
- [x] 新增 OBJ/MTL RGB/W/V material mapping 样例
- [x] 新增 OBJ/MTL ignore role 样例
- [x] 新增 OBJ/MTL texture + role mapping 样例
- [x] quick regression 增加小型 cases

## Milestone 06-8：状态报告

- [x] 生成 `REPORT_06_3MF与OBJ_MTL多材料输入当前实现状态.md`
- [x] 记录 3MF 支持范围
- [x] 记录 OBJ/MTL material mapping 支持范围
- [x] 记录未支持 extension / PBR / alpha
- [x] 记录是否建议进入 06A / 05A / 07
