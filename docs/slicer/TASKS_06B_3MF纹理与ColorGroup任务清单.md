# TASKS_06B_3MF纹理与ColorGroup任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：PRD_06B / DEV_06B  
> 建议提交目录：`docs/slicer/`

---

## Milestone 06B-0：阅读确认

- [x] 阅读 `REPORT_06A_3MF兼容性增强与负向测试当前实现状态.md`
- [x] 阅读 `DOC_DECISION_06B_REPORT06A后进入3MF纹理与ColorGroup基础支持.md`
- [x] 阅读 `PRD_06B / DEV_06B / DEMO_06B`
- [x] 确认不改变 p0.rgbwsv.2 输出协议
- [x] 确认不改 MaterialRoleMapping 基础语义
- [x] 确认本阶段不做 OpenVDB / Qt / RIP 半色调

---

## Milestone 06B-1：XML Parser 强化

- [x] 决定 tinyxml2 或 ThreeMfXmlReader v2
- [x] 支持 namespace/local-name
- [x] 支持 self-closing tag
- [x] 支持嵌套 resource 节点
- [x] 保持 DOCTYPE / ENTITY 拒绝
- [x] 保持无外部资源访问

---

## Milestone 06B-2：ColorGroup

- [x] 解析 colorgroup
- [x] 解析 color
- [x] 建立 group id → color table
- [x] triangle pid/p1/p2/p3 resolve
- [x] 支持三色平均 fallback
- [x] report 记录 colorGroup stats

---

## Milestone 06B-3：Texture2D / Texture2DGroup

- [x] 解析 texture2d
- [x] 解析 texture2dgroup
- [x] 解析 tex2coord
- [x] 建立 texid → internal texture path
- [x] 读取 3MF 内部 texture resource
- [x] 复用 TextureSampler
- [x] triangle pid/p1/p2/p3 → UV resolve
- [x] report 记录 texture stats

---

## Milestone 06B-4：3MF Internal Texture Loading

- [x] 处理 /3D/Textures/xxx.png 路径
- [x] 处理 3D/Textures/xxx.png 路径
- [x] 拒绝 path traversal
- [x] 选择临时文件或内存解码策略
- [x] texture_report 记录 source=3mf_internal

---

## Milestone 06B-5：Composition 集成

- [x] ColorGroup RGB source 接入 layer compose
- [x] Texture2DGroup RGB source 接入 layer compose
- [x] MaterialPolicy overlay 仍可用
- [x] Support S 仍独立
- [x] Model > Support > Empty 不变

---

## Milestone 06B-6：Bad Packages

- [x] bad_3mf_texture_path_missing
- [x] bad_3mf_texture_decode_failed
- [x] bad_3mf_texture2dgroup_missing_texid
- [x] bad_3mf_tex2coord_index_out_of_range
- [x] bad_3mf_colorgroup_index_out_of_range
- [x] bad_3mf_unsupported_compositematerials
- [x] bad_3mf_unsupported_multiproperties

---

## Milestone 06B-7：样例与回归

- [x] 3MF ColorGroup sample
- [x] 3MF Texture2DGroup checker sample
- [x] Mixed basematerial/color/texture sample
- [x] quick regression 增加正向样例
- [x] full regression 增加更多 bad cases

---

## Milestone 06B-8：状态报告

- [x] 生成 `REPORT_06B_3MF纹理与ColorGroup当前实现状态.md`
- [x] 记录 XML parser 方案
- [x] 记录 ColorGroup 支持范围
- [x] 记录 Texture2DGroup 支持范围
- [x] 记录未支持 PBR / Composite / MultiProperties
- [x] 记录是否建议进入 05A / 07 / 06C
