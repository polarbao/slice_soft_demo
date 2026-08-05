# DOC_DECISION_14A-04-R1 双视图纹理 ViewData 合同修订

> 文档状态：ACCEPTED / USER AUTHORIZED
> 日期：2026-08-05
> 适用范围：Stage 14A 合同层、14B Scene ViewData Provider、14E UI 宿主模拟
> 上游：`DOC_DECISION_14_UI_宿主模拟改造专项.md`、`DOC_SCHEMA_14_SceneViewData网格DTO规格.md`

## 1. 决策原因

14A-04 已冻结 15 项能力 DTO，并为 `scene.get_viewdata` 定义 position、normal、index、
LOD、`meshIdentity` 与 blob 分块。但是冻结版本没有定义 UV、材质、纹理图像和带纹理的俯视预览，
因此不能满足以下已确认的产品硬标准：

```text
宿主必须支持【俯视】与【3D】两种模型显示规格；
默认显示规格可在设置页选择；
两个显示规格对带纹理模型都必须显示模型纹理；
不得把“纹理存在但传输/解析失败”静默降级为无纹理灰模。
```

本次不是增加第 12 个 ABI 导出，也不是增加第 16 项能力，而是在打印侧书面确认完成前，
对 `scene.get_viewdata` 做一次受控 minor 修订。

## 2. 冻结文件处置

```text
原 14A-04                 保留 COMPLETE 历史事实
新增 14A-04-R1            记录本次受控修订
PM_SPI_VERSION             保持 1
公开 pm_* 导出             保持 11 个
能力数量                   保持 15 项
slicer_capability_dtos     1.1 -> 1.2
slicer_three_lane_contract 1.0 -> 1.1
打印侧确认                 必须基于 1.2 重新确认，不得沿用 1.1 回签
```

允许新增 optional/conditional 字段、扩大既有请求枚举并增加更严格的验收约束；
禁止删除或改变 1.1 字段的既有含义。

## 3. 双视图显示合同

### 3.1 视图模式

```text
top       +Z 正交俯视；保持当前 UI 的排版视角和操作习惯
three_d   可 orbit/pan/zoom；默认正交，可切换透视
```

设置页保存 `workspace.defaultViewMode=top|three_d`。中央画布同时提供即时视图切换控件；
设置决定下次进入工作区的默认值，即时切换不修改默认值。

切换视图时必须保持：

```text
sceneId / sceneRevision / sceneHash
实例变换、选中集和当前操作上下文
已缓存 meshIdentity / appearanceIdentity / textureIdentity
未完成作业和 Package 身份
```

### 3.2 纹理硬标准

对 `model.import.hasTexture=true` 的模型：

| 显示模式 | 必需数据 | 允许降级 |
|---|---|---|
| `top` | 带纹理 `surfacePreview`（合同响应必需） | 可降低预览分辨率；不得丢纹理 |
| `three_d` | position + normal + texcoord0 + index + submesh/material/texture | 可降低 mesh LOD 或纹理分辨率；不得退为无纹理灰模 |

模型本身没有纹理时允许使用 `baseColorFactor`，但必须返回 `textureStatus=not_provided`。
模型声明纹理但文件缺失、解码失败或 UV 无效时返回显式资源/输入错误；不能以纯色成功结果掩盖问题。
预算不足时先降低 mesh LOD 和纹理/俯视预览分辨率，仍不足则返回
`PM-SLICER-VIEWDATA-BUDGET`。

## 4. Scene ViewData 扩展

`scene.get_viewdata` 的 `content` 扩展为：

```text
bbox | outline | surface_preview | mesh | appearance
```

请求新增：

```text
viewMode      top | three_d
texturePolicy require_if_present
```

响应新增三类数据：

```text
mesh.buffers.texcoord0          float32x2
mesh.submeshes[]                index range -> materialId
appearances[].materials[]       baseColorFactor / textureId / alpha / UV 约定
appearances[].textures[]        RGBA8/sRGB 图像 blob 与 textureIdentity
surfacePreview                  top 模式的带纹理 RGBA8/sRGB 投影 blob
```

`appearanceIdentity` 只随材质/纹理内容变化；实例平移、旋转、缩放不得使它失效。
`meshIdentity` 与 `appearanceIdentity` 分离，允许同一模型的多个实例共享网格和纹理 GPU 资源。
多模型响应必须使用 `appearances[]`，实例预览与 submesh 引用都必须能解析到唯一的外观集合；
单数 `appearance` 无法表达同场景多个模型，禁止使用。

`outline_only` 只允许用于 top，并且仍须返回 `surfacePreview`；three_d 不得降为 outline_only。
预算不足时返回 `PM-SLICER-VIEWDATA-BUDGET`，不得删除纹理后继续成功。

## 5. 交互合同修正

Transient 车道只能使用宿主缓存的 mesh、texture、矩阵和近似 bbox：

```text
mouse-move / orbit / pan / zoom -> 0 次跨模块调用
拖拽期碰撞提示                -> 宿主本地 bbox 预测，标记为非权威
Commit                         -> scene.apply_operation 一次权威提交
正常 Commit 成功               -> 直接采用响应，不强制 get_snapshot
Stale / 恢复                    -> 才调用 scene.get_snapshot
```

`geometry.collision` 可用于静止场景显式检查或 Commit 前后的批量检查，禁止在每个 mouse-move 中调用。

## 6. M-MVP Gate 修正

为消除“M-MVP 达成后才能实现宿主，但 M-MVP 又要求宿主完成闭环”的循环定义：

```text
M-MVP-CANDIDATE = 14C-06 全绿 + 14D-05 完成
14E-01          = 纯 C 控制台宿主验证公开 ABI 端到端闭环
M-MVP           = M-MVP-CANDIDATE + 14E-01 PASS
14E-02 及后续 Qt UI 任务只依赖正式 M-MVP
```

## 7. UI 信息架构

参考行业软件只吸收稳定工作流，不复制其视觉样式或受许可证约束的代码：

| 参考 | 可吸收模式 | 来源 |
|---|---|---|
| PrusaSlicer | 中央 3D 工作区、对象列表、右侧 Profile、3D Editor/Layer Preview 切换 | [官方 UI Overview](https://help.prusa3d.com/article/ui-overview_1766) |
| UltiMaker Cura | Prepare -> Preview -> Monitor 阶段、中央模型区、折叠设置与动作区 | [官方界面说明](https://ultimaker.com/learn/simplify-3d-printing-with-ultimaker-cura-4-0/) |
| Bambu Studio | 项目/多平台、Prepare/Preview/Device、对象级参数与远程设备能力 | [官方开源仓库](https://github.com/bambulab/BambuStudio) |
| CHITUBOX | 模型视图与逐层预览并置、切片信息和输出动作集中 | [官方 Preview 文档](https://docs.chitubox.com/en-US/chitubox-basic/latest/ui-and-features/preview) |

宿主模拟采用三工作区：

```text
准备        模型导入、场景排版、top/three_d 带纹理视图、变换与预检
切片预览    Package 验证、逐层 TIFF 预览、材料通道与报告
模块诊断    ABI/Worker 版本、能力清单、作业状态、错误码与日志
```

固定布局：顶部作业栏；左侧模型/实例列表；中央主画布；右侧唯一 Context Inspector；
底部任务详情/层预览/模块日志标签页。不得用多个互相竞争的右侧面板。

## 8. 新增任务与 Gate

新增 `14B-03A TexturedSceneViewDataProvider`：

```text
输入    14A-04-R1、14B-01、14B-03
输出    textured mesh、surfacePreview、materials/textures、blob/LRU
边界    Qt-free，进入 slicer_base；不得依赖 slicer_engine
出口    OBJ+MTL+PNG、3MF Texture2D、纹理缺失/无 UV/预算不足矩阵通过
```

新增 UI Gate：

```text
UI-M9   top 模式带纹理显示，白/近白区域与背景可辨
UI-M10  three_d 模式 UV/材质/纹理显示正确
UI-M11  top <-> three_d 切换保持 scene/selection 且复用纹理缓存
UI-M12  声明纹理但加载失败时显式报错，不得静默灰模
UI-M13  设置页默认视图保存并在重启后恢复
```

验收资产至少包含：

```text
samples/models/3mf/texture2d_checker_cube.3mf
model/obj/shengdanjie_fudiao/star/MF_shengdanjie_zhongzhi_R_fy02.obj
一个白/近白纹理 fixture
一个 missing-texture 和一个 no-UV 负例
```

## 9. 渲染后端

首选 `QOpenGLWidget + QOpenGLFunctions`，不引入新的 vcpkg 包：

| 候选 | 优点 | 风险 | 结论 |
|---|---|---|---|
| QOpenGLWidget | 沿用 Qt 5.15；依赖少；可控制纹理缓存和性能 | 需要自行维护 shader/拾取/资源生命周期 | 推荐 |
| Qt3D | 场景图与相机较完整 | 新增 Qt3D 部署面；参考实现可移植性较差；维护风险更高 | 不作为首版 |

相机、视图切换和交互控制器必须与 OpenGL 渲染器分离。Qt 的许可证与部署义务沿用项目既有
Qt 5.15 策略；本次不增加新第三方库，也不修改 vcpkg。

## 10. 不变边界

本修订不改变 `p0.rgbwsv.2`、RGBWSV 顺序、uint8、`black_is_print`、Legacy 默认值、
Global opt-in、TIFF Writer 默认值、12G 冻结状态或生产切片算法。
