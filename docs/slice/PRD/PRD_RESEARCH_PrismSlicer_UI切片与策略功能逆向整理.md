# PrismSlicer UI、切片与处理策略功能 PRD（公开信息逆向整理）

> 文档版本：v0.1  
> 文档状态：Research PRD / External Product Reconstruction / 非官方产品真源  
> 生成日期：2026-07-12  
> 调研对象：Additive Appearance PrismSlicer 全彩、多材料版本  
> 适用范围：UI、模型准备、体积材料设计、切片、切片检查、外观预览、项目与输出交付  
> 明确排除：RIP 半色调算法、喷头 bitstream、设备运动控制、固化时序、设备通信和 GCVF 内部编码实现  
> 前置研究：`docs/slice/DOC/DOC_RESEARCH_PrismSlicer功能处理策略与SliceSoft对照.md`

---

## 1. 文档说明

本文不是 Additive Appearance 发布的官方 PRD，也不是对 PrismSlicer 闭源软件的源码分析。本文基于官网、产品商店、公开教程、演示视频、行业报道、合作厂商公告和团队论文，对其可见产品能力进行结构化重建。

文中需求分为：

| 标记 | 含义 |
|---|---|
| Confirmed | 公开产品页面或教程明确展示/说明 |
| Inferred | 根据多个公开功能可以高可信推导，但没有源码验证 |
| Proposed | 为形成完整 PRD 而补充的合理产品要求，不能宣称 PrismSlicer 已实现 |

本文重点回答：

```text
软件面向谁；
用户如何完成模型准备和切片；
UI 应有哪些工作区、控件和状态；
切片器如何解释几何、纹理、材料和装配关系；
用户可以配置哪些切片策略；
切片后如何逐层检查和预测打印外观；
哪些能力属于下游 RIP，不纳入本 PRD。
```

---

## 2. 产品定位

### 2.1 产品定义

PrismSlicer 是一款面向 UV Inkjet、Material Jetting、PolyJet 全彩/多材料 3D 打印的桌面切片与 DfAM 软件。

其核心产品目标不是生成传统 FFF G-code，而是：

```text
把带颜色、纹理和材料意图的 3D 模型
转换为可检查、可预测、可交付的逐层材料体素数据。
```

### 2.2 核心价值

1. 降低全彩、多材料打印的试错成本。
2. 让用户在打印前检查每层、每个区域使用什么材料。
3. 支持表面纹理之外的体积材料设计。
4. 通过打印机/材料 profile 提高颜色和纹理可预测性。
5. 通过照片级软打样预估真实打印后的颜色、透明度和表面效果。
6. 以设备可消费的预切片数据连接设计软件与厂商打印工作流。

### 2.3 非目标

本 PRD 不包含：

```text
RIP 半色调和抖动算法的内部实现；
喷头 firing bitstream；
打印头运动、铺层、固化和维护时序；
设备网络连接和作业队列控制；
厂商固件协议；
GCVF 文件内部结构；
自动生成 Stratasys 支撑的具体算法；
SLA 版整桶树脂配方功能。
```

---

## 3. 目标用户

### 3.1 全彩打印操作员

主要任务：

- 导入客户模型。
- 选择目标打印机和材料组合。
- 检查模型尺寸、方向和打印床位置。
- 快速切片并逐层检查。
- 导出预切片作业给厂商打印软件。

关注点：稳定、快速、少出错，不需要理解复杂光学算法。

### 3.2 3D 设计师与艺术家

主要任务：

- 保留 OBJ/3MF 的纹理、顶点色和材质颜色。
- 创建颜色/材料渐变。
- 组合多个模型和数字材料。
- 预览薄件、透明件和复杂纹理的打印效果。

关注点：设计意图是否能转化为真实打印效果。

### 3.3 材料与工艺工程师

主要任务：

- 管理打印机/树脂 profile。
- 检查逐层材料分布。
- 对比目标外观、预测外观和真实样件。
- 定位颜色、透明度、模糊或材料覆盖问题。

关注点：材料可解释性、颜色一致性和问题可复现性。

### 3.4 打印服务商

主要任务：

- 批量接收不同来源的模型。
- 使用稳定 profile 生成一致输出。
- 在没有立即占用打印机的情况下向客户提供软打样。
- 通过 API/CLI 接入报价、审核和生产流程。

关注点：吞吐量、自动化、版本一致性和失败隔离。

### 3.5 设备制造商或集成商

主要任务：

- 为自有设备接入输出适配器。
- 维护材料集合、分辨率和设备标定数据。
- 将切片、预览和设备工作流集成到产品中。

关注点：SDK/API、定制 profile、跨平台和离线部署。

---

## 4. 典型用户流程

### 4.1 标准全彩切片流程

```mermaid
flowchart LR
  A["创建或打开项目"] --> B["选择打印机与材料 Profile"]
  B --> C["导入 3MF / OBJ / STL"]
  C --> D["排布、旋转、缩放"]
  D --> E["检查颜色、纹理与材料"]
  E --> F["配置装配、覆盖与梯度"]
  F --> G["执行自动切片"]
  G --> H["逐层检查材料体素"]
  H --> I["渲染打印外观预览"]
  I --> J["修正设计或确认"]
  J --> K["导出预切片数据"]
```

### 4.2 设计迭代流程

```text
导入模型
-> 生成初次切片
-> 查看目标纹理与预测打印外观差异
-> 调整纹理、材料、梯度、厚度或表面效果
-> 重新切片和预览
-> 满足外观目标后导出
```

### 4.3 故障定位流程

```text
真实打印异常
-> 打开原项目和对应 Profile
-> 在 Slice Viewer 定位异常高度
-> 对照 3D 几何与材料体素
-> 判断是输入/装配/切片错误还是设备执行问题
-> 导出截图、项目和切片证据
```

### 4.4 外部打印服务流程

```text
设计师在无打印机环境中导入模型
-> 选择目标服务商提供的设备/材料 Profile
-> 软打样并修改设计
-> 导出项目或预切片文件
-> 提交给打印服务商
```

---

## 5. 信息架构与主界面

### 5.1 顶层工作区

公开界面可归纳为三个主工作区：

| 工作区 | 主要目的 | 核心对象 |
|---|---|---|
| Design | 模型、场景和体积材料设计 | 模型、装配、材料、梯度 |
| Slice | 执行切片并检查实际材料分配 | 层、体素、材料网格 |
| Preview | 预测真实打印外观 | 切片结果、光学 profile、光照和表面 finish |

建议的全局布局：

```text
顶部：文件、撤销/重做、模型工具、工作区切换、切片/导出主操作
左侧：项目对象树、装配层级、可见性和选择
中央：3D 打印床/模型视口或层切片视口
右侧：当前对象、变换、材料、梯度、切片或预览参数
底部：状态、进度、警告、内存/耗时和后台任务
```

### 5.2 全局导航要求

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| UI-NAV-001 | 用户应能在 Design、Slice、Preview 间切换，并保持当前项目和选择状态 | P0 | Confirmed |
| UI-NAV-002 | 未完成切片时进入 Preview，应提示先生成切片，不显示伪造结果 | P0 | Inferred |
| UI-NAV-003 | 模型或材料修改后，应将旧切片/旧预览标记为过期 | P0 | Proposed |
| UI-NAV-004 | 切片、渲染和导出应显示独立进度与取消入口 | P0 | Proposed |
| UI-NAV-005 | 用户应能识别当前打印机、材料 Profile 和输出目标 | P0 | Inferred |
| UI-NAV-006 | 专业参数应收进高级区域，标准流程只显示必要设置 | P1 | Proposed |

### 5.3 视觉和交互原则

1. 设计视图显示“用户意图”，Slice 视图显示“实际材料分配”，Preview 显示“预测物理结果”，三者不得混淆。
2. 所有材料均应同时用颜色和文字/图例识别，不能只靠色相。
3. 当前层、模型选择、装配选择和预览对象应跨工作区同步。
4. 任何会改变输出的设置都应使切片状态失效并要求重新切片。
5. 几何、材料或格式问题必须在导出前给出可行动的错误提示。

---

## 6. 项目与文件管理

### 6.1 新建、打开与保存

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| PRJ-001 | 支持新建空白项目 | P0 | Confirmed |
| PRJ-002 | 支持以 3MF 打开和保存项目 | P0 | Confirmed |
| PRJ-003 | 项目应保存模型、变换、装配、材料和梯度设置 | P0 | Inferred |
| PRJ-004 | 项目应记录打印机/材料 Profile 标识与版本 | P0 | Proposed |
| PRJ-005 | 项目关闭前如有未保存修改，应请求确认 | P0 | Proposed |
| PRJ-006 | 项目加载后如缺少纹理或外部资源，应列出缺失项和定位入口 | P0 | Proposed |
| PRJ-007 | 项目应区分源模型数据与生成的切片缓存 | P1 | Proposed |

### 6.2 模型导入

标准支持：

```text
3MF
OBJ + MTL + PNG/其他纹理
STL
GCVF 预切片数据
```

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| IMP-001 | 支持文件选择和拖放导入 | P0 | Confirmed |
| IMP-002 | 支持一次导入多个模型 | P0 | Confirmed |
| IMP-003 | 导入 3MF 时保留纹理、顶点色、面颜色和材质色 | P0 | Confirmed |
| IMP-004 | 导入 OBJ 时保留纹理、顶点色和材质漫反射色 | P0 | Confirmed |
| IMP-005 | STL 按无颜色几何处理，并要求选择默认材料 | P0 | Inferred |
| IMP-006 | 缺失纹理时不得静默使用错误颜色，应提示并允许重定位 | P0 | Proposed |
| IMP-007 | 导入 GCVF 后应标记为不可编辑或受限编辑的预切片对象 | P0 | Inferred |
| IMP-008 | 不支持的颜色属性应明确降级，不得无提示丢失 | P1 | Proposed |

### 6.3 对象树

对象树至少应表达：

```text
项目
  模型
  装配
    组件
  预切片对象
  材料/数字材料引用
  3D 梯度
```

功能要求：选择、重命名、删除、复制、显示/隐藏、锁定、调整覆盖顺序、定位到视口。

---

## 7. 打印机与材料 Profile

### 7.1 Profile 选择

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| PROF-001 | 新项目必须选择目标打印机或兼容设备 Profile | P0 | Inferred |
| PROF-002 | Profile 应定义构建体积和 XYZ 分辨率 | P0 | Inferred |
| PROF-003 | Profile 应定义可用离散材料集合 | P0 | Confirmed |
| PROF-004 | Profile 应关联颜色和光学标定数据 | P0 | Confirmed |
| PROF-005 | Profile 应定义可用表面模式，如 Matte/Glossy | P1 | Confirmed |
| PROF-006 | 更换 Profile 后应重新验证模型尺寸、材料和切片兼容性 | P0 | Proposed |
| PROF-007 | 用户应能查看 Profile 名称、设备、材料和更新时间 | P1 | Proposed |
| PROF-008 | 自定义设备或树脂标定应通过受控导入/服务接入 | P1 | Confirmed |

### 7.2 Profile 不兼容状态

系统应识别：

- 项目引用的材料在当前设备不可用。
- 模型超出打印体积。
- 预切片数据分辨率与目标设备不一致。
- Profile 缺少颜色或光学标定。
- Profile 版本变化导致已有切片过期。

缺少光学标定时，可以允许普通几何/材料切片，但照片级 Preview 必须明确降级或禁用。

---

## 8. Design 工作区

### 8.1 打印床和相机

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| DSG-001 | 显示与当前 Profile 一致的打印床和构建边界 | P0 | Confirmed |
| DSG-002 | 支持旋转、平移、缩放相机 | P0 | Confirmed |
| DSG-003 | 提供重置视图、适配选择和适配全部对象 | P1 | Proposed |
| DSG-004 | 超出构建体积的区域应高亮并阻止导出 | P0 | Proposed |
| DSG-005 | 支持切换透视/正交视图 | P2 | Proposed |

### 8.2 模型变换

公开确认支持 Gizmo、工具栏和数值面板三种控制方式。

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| XFM-001 | 支持移动、旋转和缩放 | P0 | Confirmed |
| XFM-002 | 支持在右侧 Transform 面板输入精确数值 | P0 | Confirmed |
| XFM-003 | 支持等比缩放锁定 | P1 | Proposed |
| XFM-004 | 支持恢复原始变换 | P1 | Proposed |
| XFM-005 | 支持多选统一移动和缩放 | P1 | Inferred |
| XFM-006 | 变换后应立即更新尺寸和打印床越界状态 | P0 | Proposed |

### 8.3 对象排布

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| LAYOUT-001 | 支持同一打印床放置多个对象 | P0 | Confirmed |
| LAYOUT-002 | 支持对象重叠和嵌套，不强制碰撞分离 | P0 | Confirmed |
| LAYOUT-003 | 重叠对象必须有明确覆盖顺序 | P0 | Confirmed |
| LAYOUT-004 | 对无意重叠提供诊断提示 | P1 | Proposed |
| LAYOUT-005 | 支持复制和阵列常用对象 | P2 | Proposed |

### 8.4 颜色与材料检查

用户选择对象后，应能看到：

```text
颜色来源：纹理 / 顶点色 / 面颜色 / 材质色 / 默认材料
纹理资源及状态
单材料或数字材料
颜色/材料覆盖规则
目标打印机是否支持所需材料
```

颜色来源发生冲突时，应显示解析优先级，并允许用户选择或确认。

---

## 9. 体积材料与装配设计

### 9.1 单材料对象

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| MAT-001 | 无颜色模型可绑定一种打印材料 | P0 | Confirmed |
| MAT-002 | 材料列表只显示当前设备可用材料 | P0 | Inferred |
| MAT-003 | 材料变更后应使现有切片和 Preview 失效 | P0 | Proposed |

### 9.2 数字材料/材料混合

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| MAT-010 | 支持由多个基础材料定义数字材料 | P0 | Confirmed |
| MAT-011 | 混合比例应归一化并提供数值输入 | P0 | Inferred |
| MAT-012 | UI 应预览材料混合的目标颜色/属性 | P1 | Inferred |
| MAT-013 | 无法由设备材料实现的混合应提示可实现范围 | P1 | Inferred |
| MAT-014 | 数字材料应可复用到多个模型或梯度停止面 | P1 | Confirmed |

### 9.3 装配、嵌套与覆盖

PrismSlicer 的装配允许把单材料、数字材料、彩色模型、预切片模型和梯度模型放进同一体积关系中。

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| ASM-001 | 支持将多个对象组成装配 | P0 | Confirmed |
| ASM-002 | 支持组件相互重叠和嵌套 | P0 | Confirmed |
| ASM-003 | 支持控制重叠区域的覆盖优先级 | P0 | Confirmed |
| ASM-004 | 覆盖顺序应在对象树和属性面板中可见 | P0 | Proposed |
| ASM-005 | 切片后可在 Slice Viewer 检查重叠区域 | P0 | Confirmed |
| ASM-006 | 预切片对象与网格对象冲突时应显示受限规则 | P1 | Proposed |

### 9.4 3D Gradient Editor

#### 9.4.1 功能定义

3D 梯度是体积材料场，而不是表面贴图。每个梯度停止面绑定一种单材料或数字材料，停止面之间进行线性插值。

#### 9.4.2 功能需求

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| GRAD-001 | 支持创建线性 3D 材料梯度 | P0 | Confirmed |
| GRAD-002 | 支持添加、删除和选择多个停止平面 | P0 | Confirmed |
| GRAD-003 | 支持用 Gizmo 移动和旋转停止平面 | P0 | Confirmed |
| GRAD-004 | 支持精确输入停止面变换 | P0 | Confirmed |
| GRAD-005 | 每个停止面可绑定单材料或材料混合 | P0 | Confirmed |
| GRAD-006 | 相邻停止面之间应做线性材料插值 | P0 | Confirmed |
| GRAD-007 | 梯度范围外使用最近停止面的材料 | P0 | Confirmed |
| GRAD-008 | 设计视口应实时显示梯度方向和近似效果 | P1 | Inferred |
| GRAD-009 | 切片后应逐层检查实际离散材料过渡 | P0 | Confirmed |
| GRAD-010 | 停止面重合或顺序不明确时应提示 | P1 | Proposed |

---

## 10. 切片功能

### 10.1 切片输入

切片器的输入应由以下内容共同确定：

```text
目标打印机和材料 Profile
模型几何与变换
纹理/顶点色/面颜色/材质色
单材料与数字材料
装配层级与覆盖顺序
3D 梯度
导入的预切片数据
切片质量/预览相关策略
```

### 10.2 切片输出概念

本 PRD 将输出抽象为：

```text
LayerStack
  Layer[z]
    MaterialVoxel[x, y]
```

每个材料体素表示打印位置的最终材料选择或材料角色。本文不定义其下游半色调编码、压缩和设备 bitstream。

### 10.3 自动切片

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| SLC-001 | 用户可从 Design 工作区启动自动切片 | P0 | Confirmed |
| SLC-002 | 切片应使用当前打印机分辨率和材料集合 | P0 | Inferred |
| SLC-003 | 切片应处理多对象、嵌套和覆盖关系 | P0 | Confirmed |
| SLC-004 | 切片应处理表面颜色与体积材料设计 | P0 | Confirmed |
| SLC-005 | 切片应处理导入的预切片组件 | P0 | Confirmed |
| SLC-006 | 切片应显示进度、耗时和可取消状态 | P0 | Proposed |
| SLC-007 | 取消或失败不得覆盖上一次有效切片 | P0 | Proposed |
| SLC-008 | 输入改变后应自动标记需要重新切片 | P0 | Proposed |
| SLC-009 | 切片完成后自动进入或提示打开 Slice Viewer | P1 | Proposed |

### 10.4 几何解释策略

基于公开功能，切片器需要满足：

1. 把网格转换为逐层体积占据关系。
2. 对模型内部和表面进行一致材料求值。
3. 支持任意 3D 方向的梯度，不限于 Z 层渐变。
4. 按装配优先级解决重叠体素。
5. 对预切片对象保留其离散材料信息。
6. 对复杂或有问题的几何提供失败/警告，而不是产生不可解释空洞。

公开资料没有说明其 non-manifold、自交和修网策略，因此以下要求属于 Proposed：

| ID | 需求 | 优先级 |
|---|---|---:|
| GEO-001 | 切片前检查空网格、零尺寸和不可解析文件 | P0 |
| GEO-002 | 对非流形、自交、重复面和开放边给出分类提示 | P1 |
| GEO-003 | 自动修复不得静默改变模型，应允许查看修复摘要 | P1 |
| GEO-004 | 无法可靠切片时应阻止导出 | P0 |

### 10.5 颜色与纹理应用策略

不讨论半色调内部实现时，产品仍应暴露以下高层策略：

```text
颜色来源选择
纹理采样与缺失纹理处理
颜色/材料 Profile
颜色可实现范围检查
纹理细节增强开关或质量级别
透明/薄壁预测
目标颜色与预测颜色对照
```

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| COL-001 | 切片必须保留模型的目标颜色和纹理空间关系 | P0 | Confirmed |
| COL-002 | 颜色处理应使用当前设备/树脂标定 | P0 | Confirmed |
| COL-003 | 薄壁和半透明材料应纳入颜色预测 | P1 | Confirmed |
| COL-004 | 纹理增强应以打印能力为边界，不承诺恢复不可打印细节 | P1 | Confirmed |
| COL-005 | 超色域颜色应提示或显示可实现近似 | P1 | Inferred |
| COL-006 | 颜色/纹理优化后仍应允许查看实际切片材料分配 | P0 | Confirmed |

### 10.6 局部厚度和几何相关策略

团队公开研究表明，薄壁、尖角、凹面和对侧表面会影响可实现颜色。作为产品级策略，建议：

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| THIN-001 | 对低于推荐厚度的区域显示风险提示 | P1 | Proposed |
| THIN-002 | Preview 应表现厚度导致的透光和色偏 | P0 | Confirmed |
| THIN-003 | 纹理增强应考虑局部几何，而不是只做全局对比度 | P1 | Inferred |
| THIN-004 | 用户应能定位高风险薄壁区域 | P2 | Proposed |

### 10.7 支撑边界

公开 Stratasys 工作流显示 GCVF 导入 GrabCAD Print 后由下游添加支撑。因此本 PRD 对支撑仅要求：

```text
导出文件必须保留正确模型形状和材料体素；
UI 必须说明当前工作流的支撑归属；
PrismSlicer Slice Viewer 不得把未生成的支撑显示为最终数据；
导出后由 GrabCAD Print 生成的支撑不属于 PrismSlicer 当前切片预览真源。
```

如果面向自定义设备的企业版本由 PrismSlicer 负责支撑，则应作为设备 Profile 的可插拔策略，不与通用颜色/材料切片逻辑耦合。

---

## 11. Slice Viewer

### 11.1 产品目的

Slice Viewer 用于检查“实际生成的材料体素”，不是重复显示输入模型。

主要问题：

- 纹理是否正确投射。
- 多组件重叠区由谁覆盖。
- 梯度方向和材料过渡是否正确。
- 内部材料是否符合设计意图。
- 异常来自切片软件还是打印设备。

### 11.2 功能需求

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| VIEW-001 | 支持用垂直滑块选择层 | P0 | Confirmed |
| VIEW-002 | 显示当前层索引和物理 Z 高度 | P0 | Proposed |
| VIEW-003 | 在 3D 几何上下文中显示切片平面 | P0 | Confirmed |
| VIEW-004 | 用材料图例显示每个体素的材料归属 | P0 | Confirmed |
| VIEW-005 | 支持显示/隐藏组件网格，便于对照 | P0 | Confirmed |
| VIEW-006 | 支持检查装配重叠区 | P0 | Confirmed |
| VIEW-007 | 支持检查 3D 梯度的离散结果 | P0 | Confirmed |
| VIEW-008 | 支持查看体素位置、材料和来源对象 | P1 | Proposed |
| VIEW-009 | 支持跳转到首个警告层 | P1 | Proposed |
| VIEW-010 | 支持导出当前层截图和诊断摘要 | P2 | Proposed |
| VIEW-011 | 大模型切层切换不应触发完整重新切片 | P0 | Inferred |

### 11.3 视图模式

建议至少包含：

```text
材料模式：离散材料体素
组件来源模式：体素来自哪个模型/装配组件
目标颜色模式：设计颜色
几何对照模式：模型网格 + 当前层
差异模式：目标颜色与预测/切片结果差异
```

前三项中只有材料模式和几何对照有明确公开证据，其余属于完善工作流的 Proposed。

---

## 12. Preview 工作区

### 12.1 产品定义

Preview 使用实际切片材料分布和打印机/树脂光学 Profile，预测模型打印、后处理并放在特定光照环境下的外观。

Preview 不等同于：

```text
输入纹理贴图预览；
普通 PBR 网格渲染；
最终设备运动仿真；
真实打印质量认证。
```

### 12.2 预览设置

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| PRE-001 | Preview 必须基于当前有效切片 | P0 | Confirmed |
| PRE-002 | 支持设置图像分辨率 | P0 | Confirmed |
| PRE-003 | 支持设置相机 FoV | P1 | Confirmed |
| PRE-004 | 支持设置 sample count/质量 | P0 | Confirmed |
| PRE-005 | 支持多个对象同时渲染 | P0 | Confirmed |
| PRE-006 | 支持选择 360 度环境 | P0 | Confirmed |
| PRE-007 | 支持导入 OpenEXR HDRI | P1 | Confirmed |
| PRE-008 | 支持连续 Surface Roughness | P0 | Confirmed |
| PRE-009 | Roughness 应覆盖粗糙、Matte、Glossy、抛光等效果范围 | P0 | Confirmed |
| PRE-010 | 缺少材料光学标定时应明确降级 | P0 | Proposed |

### 12.3 渲染交互

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| RENDER-001 | 点击 Render Preview 后打开或进入渐进式渲染视图 | P0 | Confirmed |
| RENDER-002 | 图像应从低样本噪声状态持续收敛 | P0 | Confirmed |
| RENDER-003 | 每轮更新提供去噪图像 | P1 | Confirmed |
| RENDER-004 | 用户可在满意时停止渲染 | P0 | Confirmed |
| RENDER-005 | 渲染过程中显示进度、样本数和耗时 | P1 | Proposed |
| RENDER-006 | 更改场景、切片、环境或 roughness 后旧结果应标记过期 | P0 | Proposed |
| RENDER-007 | 支持保存预览图及其设置摘要 | P1 | Inferred |

### 12.4 预测内容

Preview 至少应表现：

- 树脂导致的颜色变化。
- 薄件和不同颜色导致的透明度差异。
- 内部散射导致的纹理模糊和对比度下降。
- 打印分辨率形成的打印线/层纹。
- Matte、Glossy、抛光和粗糙后处理差异。
- 不同环境光源对最终观察颜色的影响。

### 12.5 设计对比

建议提供：

```text
输入设计外观
预测打印外观
并排 / 滑块 / 闪烁对比
高风险区域标记
```

公开教程已有输入、预测和真实照片对比，但产品 UI 是否提供交互式对比尚未确认，因此属于 Proposed。

---

## 13. 导出与下游交付

### 13.1 导出功能

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| EXP-001 | 支持将当前有效切片导出为 GCVF | P0 | Confirmed |
| EXP-002 | 导出前必须验证切片与当前项目/Profile 一致 | P0 | Proposed |
| EXP-003 | 导出时显示目标格式、设备和路径 | P0 | Proposed |
| EXP-004 | 导出失败不得损坏已有文件 | P0 | Proposed |
| EXP-005 | 导出成功后提供打开目录或进入下游软件的入口 | P1 | Proposed |
| EXP-006 | 支持 3MF 项目保存，与 GCVF 生产交付分离 | P0 | Confirmed |

### 13.2 下游边界提示

对于 Stratasys 工作流，UI 应明确：

```text
GCVF 包含预切片体素材料数据；
GrabCAD Print 中表面颜色可能不显示；
GrabCAD Print 仍负责支撑、Matte/Glossy 选择和打印提交；
GCVF 导出成功不等同于打印成功；
```

### 13.3 导出前检查

建议检查项：

- 目标设备 Profile 已选择。
- 所有模型位于构建体积内。
- 所有纹理和材料资源可用。
- 当前切片不是过期状态。
- Slice Viewer 没有未确认的严重警告。
- GCVF 路径可写且磁盘空间足够。

---

## 14. 状态与错误处理

### 14.1 项目状态

```text
Empty
DesignReady
SliceRequired
Slicing
SliceReady
PreviewRendering
PreviewReady
Exporting
ExportReady
Error
```

状态转换要求：

1. 模型、材料、梯度、装配、变换或 Profile 改变后进入 `SliceRequired`。
2. `Slicing` 失败后保留上一次有效结果，但标记它与当前设计不一致。
3. 新切片完成后旧 Preview 过期。
4. 导出只允许使用当前 `SliceReady` 数据。

### 14.2 错误分类

| 类别 | 示例 | UI 行为 |
|---|---|---|
| Import | 文件损坏、纹理缺失、不支持的颜色属性 | 定位文件和缺失资源 |
| Geometry | 空网格、尺寸异常、不可可靠切片 | 高亮对象，必要时阻止切片 |
| Profile | 材料缺失、设备不兼容、标定缺失 | 提供切换 Profile 或降级说明 |
| Assembly | 重叠覆盖不明确、预切片冲突 | 定位组件和覆盖规则 |
| Slice | 内存不足、取消、算法失败 | 保留旧结果，提供重试 |
| Preview | 光学数据缺失、渲染器不可用 | 允许切片但禁用/降级 Preview |
| Export | 路径不可写、格式不兼容 | 不破坏原文件，提供更换路径 |

### 14.3 警告确认

警告应分为：

```text
Info：不影响输出，仅提示状态
Warning：可以继续，但需要用户确认
Blocking：必须修复后才能切片或导出
```

公开 PrismSlicer 是否已经采用上述等级未知，此处属于 Proposed。

---

## 15. 性能与可用性要求

公开商店宣传“快速加载和切片，基本不受模型尺寸或三角形数量影响”，但没有公开统一 benchmark。PRD 不应直接承诺绝对数值，应先定义可测指标：

| ID | 指标 | 目标方向 |
|---|---|---|
| PERF-001 | 首次模型显示时间 | 导入后快速出现可交互代理 |
| PERF-002 | 视口帧率 | 常见模型下保持可交互 |
| PERF-003 | 切片耗时 | 随输出体素数量可预测增长 |
| PERF-004 | 峰值内存 | 大模型失败前给出预估和提示 |
| PERF-005 | 层切换延迟 | Slice Viewer 层切换不重新切片 |
| PERF-006 | 首张 Preview 时间 | 渐进渲染尽快提供可判断图像 |
| PERF-007 | 取消响应 | 切片和渲染可在合理时间内停止 |

### 15.1 后台任务

切片和 Preview 应作为独立后台任务运行，UI 在任务期间仍可查看项目和日志，但会改变输出的操作应被禁用或要求先取消任务。

### 15.2 离线要求

标准桌面切片和 Preview 应可离线运行。许可证激活、更新、在线商店或云端服务不在本 PRD 讨论范围。

---

## 16. 跨平台与企业集成

### 16.1 桌面平台

```text
Windows x64
macOS Apple Silicon/受支持架构
Linux x64/AppImage 或等效分发
```

### 16.2 企业 API

公开确认企业版本提供 Python API/CLI 以导入对象和渲染预览。完整 PRD 可定义：

| ID | 需求 | 优先级 | 证据 |
|---|---|---:|---|
| API-001 | 脚本化打开项目和导入模型 | P1 | Confirmed |
| API-002 | 脚本化选择 Profile | P1 | Proposed |
| API-003 | 脚本化执行切片 | P1 | 未确认/Proposed |
| API-004 | 脚本化渲染 Preview | P1 | Confirmed |
| API-005 | 脚本化导出生产文件 | P1 | 未确认/Proposed |
| API-006 | 返回机器可读的错误、警告和任务进度 | P1 | Proposed |

API 必须复用桌面软件同一项目、Profile 和切片语义，不能形成另一套不可验证输出。

---

## 17. 功能优先级

### 17.1 P0：完整可用的主流程

```text
项目新建/打开/保存
打印机与材料 Profile
3MF/OBJ/STL 导入
多对象排布与精确变换
颜色、纹理、单材料和数字材料
装配、嵌套和覆盖优先级
线性 3D 材料梯度
自动切片
逐层材料 Slice Viewer
基于实际切片的 Preview
GCVF 导出
清晰的过期、失败和阻断状态
```

### 17.2 P1：专业工作流

```text
材料/颜色可实现范围提示
薄壁和透明度风险
预览 HDRI、roughness、去噪和结果保存
诊断导出
高级 Profile 和自定义标定
企业 Python API/CLI
```

### 17.3 P2：增强体验

```text
阵列和自动排布
目标/预测交互对比
薄壁风险热图
逐体素来源探针
批处理与打印服务集成
```

---

## 18. 验收标准

### 18.1 标准模型流程

给定带 PNG 纹理的 OBJ：

1. 用户可拖放导入。
2. 纹理正确显示且没有静默丢失。
3. 可移动、旋转、缩放并输入精确尺寸。
4. 可选择兼容打印机/材料 Profile。
5. 可完成切片并逐层查看材料分布。
6. 可从 Preview 观察颜色、透明度、纹理模糊和表面 finish。
7. 可导出与当前切片一致的 GCVF。

### 18.2 多模型装配流程

给定三个不同材料来源的嵌套模型：

1. 用户可建立装配并设置覆盖顺序。
2. Design 视图能识别每个组件。
3. Slice Viewer 能逐层确认重叠区域的最终材料。
4. 改变覆盖顺序后旧切片立即过期。
5. 重新切片后结果按新顺序更新。

### 18.3 3D 梯度流程

1. 用户可创建至少两个停止平面。
2. 每个停止面可分配不同材料/数字材料。
3. 停止面可用 Gizmo 和数值控制。
4. 停止面之间形成体积线性过渡。
5. 梯度外使用最近停止面材料。
6. 用户可在 Slice Viewer 检查逐层过渡。

### 18.4 Preview 流程

1. 没有有效切片时不能生成误导性 Preview。
2. 用户可选择环境、roughness、分辨率、FoV 和样本数。
3. 渲染以渐进方式更新并提供去噪结果。
4. 用户可提前停止并保存当前结果。
5. 更换材料 Profile 或修改设计后旧 Preview 标记过期。

### 18.5 故障流程

1. 缺少纹理时明确列出文件并允许重定位。
2. 模型超出打印床时在视口高亮并阻止导出。
3. 当前材料不被目标设备支持时阻止切片或要求替换。
4. 切片取消/失败后不得把不完整数据标记为有效。
5. 导出失败不得破坏已有目标文件。

---

## 19. 与 SliceSoft 的适用边界

本 PRD 可以作为 SliceSoft 产品规划参考，但不能直接视为 SliceSoft 当前目标需求。映射时应保持以下边界：

```text
PrismSlicer 的 MaterialVoxel 不是 SliceSoft RGBWSV 通道的直接等价物；
PrismSlicer 的 Matte/Glossy 不是 SliceSoft V 通道的直接等价物；
PrismSlicer 的 GCVF 导出不替代 SliceSoft p0.rgbwsv.2；
PrismSlicer 下游生成支撑的流程不适用于 SliceSoft 当前 S 通道责任；
照片级 Preview 需要真实材料标定，不能只做 UI 仿制；
3D 梯度可借鉴为 MaterialIntent，但不应修改 RGBWSV 协议；
OpenVDB 只能保持 optional utility/candidate，不能因竞品研究转为默认依赖。
```

对 SliceSoft 最直接可吸收的 P0 产品思想是：

1. Design、Slice、Preview 三种信息必须明确区分。
2. Preview 和 Slice Viewer 应以真实切片输出为数据真源。
3. Profile 必须同时解释设备、材料和处理能力。
4. 多模型、装配、覆盖和材料来源要在 UI 中可解释。
5. 修改设计后必须显式失效旧切片和旧预览。
6. 支撑、光油、白墨和下游 RIP 的责任归属必须由 Profile/契约声明。

---

## 20. 未确认事项

1. PrismSlicer 当前是否内置自动摆放、自动朝向或模型修复。
2. 标准版是否允许用户自行创建打印机 Profile，还是必须购买标定服务。
3. 数字材料的最大基础材料数和精度。
4. 3D Gradient 是否只支持线性梯度，或还支持径向/曲线梯度。
5. Slice Viewer 是否提供体素探针、组件来源和差异视图。
6. 是否支持目标颜色 out-of-gamut UI 和定量色差。
7. 标准版是否提供 CLI 切片/导出，还是企业版只支持导入和渲染。
8. 几何错误和自动修复的具体准入规则。
9. 自定义设备中支撑生成由 PrismSlicer 还是下游系统负责。
10. 完整项目 3MF 中如何保存 PrismSlicer 专有的梯度和装配信息。

---

## 21. 参考资料

### 21.1 主要产品依据

1. [PrismSlicer 产品页](https://appearan.cz/3d-slicer-software/prismslicer-multi-material/)
2. [PrismSlicer 商店功能与格式矩阵](https://shop.appearan.cz/products/prismslicer)
3. [全彩模型导入、切片、Preview 和 GCVF 教程](https://appearan.cz/blog/how-to-preview-full-color-3d-printouts-with-photorealistic-accuracy/)
4. [Slice Viewer](https://appearan.cz/features/slice-viewer/)
5. [3D Gradient Editor](https://appearan.cz/features/3d-gradient-editor/)
6. [Photorealistic Preview](https://appearan.cz/features/photorealistic-preview/)
7. [Color Accuracy](https://appearan.cz/features/color-accuracy/)
8. [Texture Detail](https://appearan.cz/features/texture/)
9. [Printer Compatibility](https://appearan.cz/features/printer-compatibility/)

### 21.2 处理策略依据

10. [Scattering-aware Texture Reproduction for 3D Printing](https://cgg.mff.cuni.cz/?p=1714)
11. [Geometry-Aware Scattering Compensation for 3D Printing](https://cgg.mff.cuni.cz/~jaroslav/papers/2019-texfab3d/2019-sumin-rittig-texfab3d-paper.pdf)
12. [Neural Acceleration of Scattering-Aware Color 3D Printing](https://cgg.mff.cuni.cz/publications/neural-acceleration-of-scattering-aware-color-3d-printing/)
13. [A Gradient-Based Framework for 3D Print Appearance Optimization](https://cgg.mff.cuni.cz/publications/a-gradient-based-framework-for-3d-print-appearance-optimization/)
14. [Scattering-Aware Color Calibration](https://doi.org/10.1145/3763293)

