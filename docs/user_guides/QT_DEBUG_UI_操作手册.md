# QT_DEBUG_UI_操作手册

> 日期：2026-07-28
> 适用程序：`slicer_debug_ui`
> 适用对象：切片 Demo 调试、样例验证、模型导入、层预览、报告查看。

## 1. 软件定位

`slicer_debug_ui` 是 SliceSoft 的本地调试 UI。它负责组织配置、调用命令行工具、加载输出包和展示报告，不直接实现喷头控制、RIP 半色调或设备通信。

固定输出协议仍为：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

## 2. 启动方式

PowerShell：

```powershell
.\scripts\Configure12CQtUi.ps1 -BuildDir build-12c-ui -Config Debug
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe
```

VSCode：

```text
SliceSoft: Debug Qt UI
```

## 3. 推荐工作流

### 3.1 使用已有场景

1. 在左侧“场景/Profile”选择一个场景。
2. UI 会自动填充“配置文件”和“输出包”。
3. 点击“运行切片”。
4. 切片完成后 UI 自动加载输出包。
5. 在“预览”页切换生产层检查、材料叠加和原始调试预览；通过底部诊断区域检查报告与曲线。
6. 点击“运行 RIP 摘要”确认输出包协议兼容。

### 3.2 一键导入模型并切片

点击：

```text
导入模型并切片
```

然后选择任意目录下的：

```text
*.obj
*.stl
*.3mf
```

UI 会自动生成：

```text
output/ui_sessions/<模型名_时间戳>/slice_config.generated.json
output/ui_sessions/<模型名_时间戳>/package
```

并自动运行：

```powershell
build/Debug/slicer_cli.exe --config <generated_config>
```

如果选择的是 OBJ，全彩纹理要求：

```text
model.obj
model.mtl
texture.png / texture.jpg / ...
```

OBJ 中需要引用 MTL：

```text
mtllib model.mtl
```

MTL 中需要引用贴图：

```text
map_Kd texture.png
```

贴图文件可以放在 OBJ / MTL 同级目录或 MTL 内相对路径指向的位置。当前 importer 会按 OBJ/MTL 路径解析贴图。

### 3.3 多模型俯视、排版与当前场景切片

点击左侧：

```text
导入模型预览
```

一次可选择一个或多个 OBJ、STL、3MF。UI 会按选择顺序串行导入，切换到“模型”页并在同一 +Z
俯视画布显示全部可见实例。批次完成后只执行一次规则排版；单批坏文件不会删除同批已成功模型。

场景总实例数上限为 22。若本次选择会超过剩余容量，整批在导入前阻断，不静默截断。

“模型”页当前支持：

| 控件 | 作用 |
|---|---|
| X / Y | 设置模型在软件场景平面的毫米位置。 |
| 绕 Z | 按度设置平面旋转。 |
| 统一缩放 | 等比例缩放，不开放非均匀缩放。 |
| 应用 | 原子提交四项数值，并异步刷新俯视几何和变换后预检。 |
| 原点居中 | 把当前模型 XY 包围盒中心移动到软件场景原点。 |
| 重置 | 恢复 identity transform。 |
| 镜像 X / 镜像 Y | 沿源模型包围盒中心镜像实例，不修改源文件。 |
| 保存场景配置 | 写入并回读当前 session 的 scene draft/effective config。 |
| 模型列表 | 切换当前实例、追加、复制、删除以及控制 visible/locked。 |
| 排版 | 设置 11 x 2 规则排版和 20/30 mm 默认净距。 |
| 切片当前场景 | 冻结当前全部可见实例并产生一个联合 RGBWSV Package。 |
| 停止切片 | 终止当前场景进程，不接受或回载取消后的 Package。 |

画布坐标固定为：

```text
+X 向右；
+Y 向上；
单位 mm；
Z 落台与 autoOrient 沿用现有生产逻辑，界面不开放 Z 平移。
```

状态区会分别显示源模型、变换后模型、Legacy 和 Global 准入。`PENDING`、`FAILED`、`BLOCKED`、
`stale` 或取消结果均不会放行场景切片。所有可见实例和场景准入通过后，点击：

```text
切片当前场景
```

UI 会自动冻结：

```text
sceneId / sceneRevision / sceneHash；
每个 modelId / instanceId / transformRevision；
当前 scene-wide Profile；
DPI、层厚、buildVolume 和显式 pipeline mode。
```

随后执行：

```powershell
slicer_cli --scene-config <scene_config.effective.json>
```

成功后只接受与冻结身份一致的一个 Package，并自动打开 TIFF 生产预览。场景在运行期间被修改、用户取消、
Package 身份不匹配或进程失败时均不会回载旧结果。

当前多模型场景生产只准入 Legacy。选择未准入 Global 时按钮明确阻断，禁止静默回退 Legacy。设备
buildVolume、原点/轴向和 22 实例预算尚未关闭，因此当前场景输出属于功能 fixture，
`productionReady=false`，不能作为设备 production GO。

复杂或开放拓扑模型被阻断后仍可查看，用于调整和诊断，但不能把 Legacy 的通过或警告自动视为 Global
通过。当前不提供自动修复、鼠标 gizmo、非均匀缩放或完整 3D 相机。

### 3.4 OpenVDB 实验诊断

点击：

```text
导入模型并 OpenVDB 诊断
```

UI 会选择模型、生成临时配置，然后运行：

```powershell
slicer_cli --config <generated_config> --experimental-openvdb-shell --admission-mode diagnostic_only
```

当前该按钮只生成实验诊断报告：

```text
output/ui_sessions/<模型名_时间戳>_openvdb/reports/experimental_openvdb_shell_report.json
```

它不生成生产 RGBWSV 切片包。

## 4. 左侧区域说明

| 控件 | 作用 |
|---|---|
| 场景/Profile | 从 `samples/scenarios/slicer_scenarios.json` 选择样例。 |
| 配置文件 | 当前传给 `slicer_cli --config` 的 JSON。 |
| 输出包 | 当前加载的 RGBWSV package 目录。 |
| 对比包 A/B | 用于材料工艺 Profile 对比。 |
| 构建调试版 | 执行 `cmake --build build --config Debug`。 |
| 导入模型并切片 | 选择任意模型，生成临时配置，并执行 legacy production 切片。 |
| 导入模型预览 | 一次选择 1..N 个模型，导入当前 SceneDocument 并进行单次规则排版。 |
| 切片当前场景 | 消费当前可见实例，通过显式 `--scene-config` 写一个联合 Package。 |
| 导入模型并 OpenVDB 诊断 | 选择任意模型，生成临时配置，并执行 OpenVDB experimental diagnostic。 |
| 运行切片 | 运行当前单模型配置；SceneDocument 已加载时禁用，避免绕过当前场景。 |
| 运行 RIP 摘要 | 对当前输出包执行 RIP reader summary。 |
| 运行快速回归 | 执行 `scripts/run_regression.ps1 -Mode quick`。 |
| 对比工艺配置 | 对比两个输出包的材料工艺报告。 |
| 加载输出包 | 手动加载当前输出包路径。 |
| 打开输出目录 | 在文件管理器中打开当前输出包。 |
| 切片进度与耗时 | 实时显示当前阶段、图层进度，以及切片计算和输出保存的耗时拆分。 |

### 4.1 场景/Profile 显示规则

`场景/Profile` 默认只显示长期常用场景，会隐藏高级场景和测试夹具。左侧勾选：

```text
显示全部场景
```

后，UI 会显示完整场景列表，包括高级、测试和实验场景。下拉项过长时可直接展开下拉框或悬停查看完整说明，说明中包含：

```text
场景名称
配置路径
输出包路径
实验/测试/高级标记
```

### 4.2 切片进度与耗时

执行“导入模型并切片”“切片当前场景”“运行切片”或“导入模型并 OpenVDB 候选切片”后，左侧按钮区
下方会显示：

| 字段 | 含义 |
|---|---|
| 当前阶段 | 配置读取、模型加载、几何采样、支撑生成、逐层处理、报告生成等实时阶段。 |
| 图层进度 | 当前已处理图层数、总图层数和百分比。 |
| 模型加载 | 模型和材料文件读取耗时；OpenVDB 候选路径无法独立拆分时可能为 0。 |
| 切片处理 | 网格、几何采样、纹理准备、支撑生成和逐层材料计算耗时，不含文件保存。 |
| TIFF 保存 | 生产 RGBWSV TIFF 编码和写盘耗时。 |
| 预览保存 | preview 构建、编码和写盘耗时。 |
| 报告处理 | 报告对象生成与 JSON 写盘耗时。 |
| 切片保存合计 | TIFF、preview、报告写盘和候选包发布耗时之和。 |
| 总耗时 | 切片核心整次运行墙钟耗时；失败或旧版 CLI 无细分数据时回退为界面等待的进程耗时。 |

这些数据用于诊断性能，不改变 `RGBWSV` 通道、位深、极性或材料策略。“切片处理”与 `--benchmark-core-only` 的严格基准口径相关但不等同，正式引擎性能对比仍应使用 core-only benchmark。

## 5. 中央页面说明

| 页面 | 作用 |
|---|---|
| 模型 | 切片前 +Z 俯视、精确实例变换、镜像和变换后准入状态；不直接启动生产切片。 |
| 预览 | 统一预览工作区，可在生产层检查、材料叠加和原始调试预览之间切换，并保持同一真实 `layerIndex`。 |
| 配置 | 编辑当前配置 JSON 和常用参数。 |

### 5.1 统一预览的三种模式

三者已经整合为一个“预览”工作区，但仍保留不同模式，因为数据来源不同：

```text
层预览：面向生产 TIFF/RGBWSV 层检查，适合判断真实打印通道。
叠加预览：面向视觉关系检查，适合看 RGB 与 S/W/V 是否同层对齐。
原始预览：面向调试文件检查，适合确认 preview 目录实际生成了哪些图片。
```

工作区模式为：

```text
生产层检查 / 材料叠加 / 原始调试预览
```

模式切换共享真实 `layerIndex`。如果原始 preview 或某个叠加材料在该层没有图片，界面会保持当前层并显示同层缺失，不会跳到最近或后续有图层。

### 5.2 材料图例与像素探针

预览工作区顶部常驻显示：

```text
RGB：模型颜色或 RGB 模型填充；
W：白墨模型填充；
S：模型外支撑或内部镂空支撑；
V：表面/外侧光油，或选择光油时的模型填充；
真实空白：RGBWSV 六个生产值均为 255。
```

生产 TIFF 固定为 `RGBWSV uint8 + black_is_print`，`0=打印`、`255=不打印`。W/S/V 色块是可配置的 UI 伪彩，RGB 可以是真彩显示；这些显示颜色都不能替代生产值判断。

切换到“生产层检查”并点击图像，可以在工作区看到当前 `layerIndex`、坐标、`R/G/B/W/S/V`、打印通道和材料语义。切换层或通道后旧探针会清除，避免误读上一层数据。

### 5.3 底部诊断区域

报告、曲线和日志已经从中央页面移入底部诊断区域。软件启动时该区域默认隐藏，不会持续压缩模型预览。

打开方式：

```text
菜单栏 -> 视图 -> 诊断区域
```

诊断区域包含：

```text
报告：manifest 与 reports JSON 摘要；
曲线：RGB/W/S/V 随 layer 的统计变化；
日志：构建、切片、RIP 和诊断命令的输出、错误及退出码。
```

关闭诊断区域只会隐藏面板，不会清空当前日志或卸载输出包。

### 5.4 加载 OpenVDB Utility 诊断报告

打开：

```text
菜单栏 -> 视图 -> 诊断区域 -> 报告 -> 加载诊断报告...
```

可选择 package 外的 `slicesoft.openvdb_sdf_utility.12b_r2.1` JSON，例如 `output/benchmarks` 中由 utility probe 生成的报告。加载操作只把路径加入当前 UI 会话，不会复制、改写报告或修改当前输出包。

摘要会显示：

```text
OpenVDB 是否编译和运行可用；
外侧光油壳层、间隙距离、拓扑诊断、材料闭环辅助四项 utility；
每项的执行状态、推进建议和阻断项；
productionReplacementAllowed=false；
Legacy 默认生产路径和 guard 是否实际运行。
```

“Utility 验证通过（非生产）”只说明该辅助能力在本报告中通过，不表示生产切片验收通过。错误 schema、安全输出策略异常或 `productionReplacementAllowed=true` 会显示“报告无效，禁止作为生产证据”。

### 5.5 小窗口与滚动

工作台已验证以下窗口尺寸：

```text
1440x900
1280x720
1024x768
```

三种尺寸下左侧项目区、中央预览/配置区和右侧参数区均保留，不会相互遮挡。中央区域优先获得伸缩空间；左侧项目区和配置页内容超过可用高度时使用滚动条。长路径可把鼠标停留在输入框上查看完整值。

底部诊断区域默认隐藏。展开后它位于主工作区下方，不覆盖预览；再次关闭会恢复中央空间，并保持当前真实 `layerIndex`。

## 6. 配置页设置位置

常用设置位于：

```text
配置 -> 常用
```

该页包含：

| 分组 | 主要功能 |
|---|---|
| 基础 | 模型文件、输出目录、层高。 |
| 材料 | 纹理策略、非表面 RGB、模型内部白墨/光油填充、顶部/表面/外侧光油。 |
| 支撑 | 支撑总开关、支撑位置、内部镂空支撑和面积阈值。 |
| 预览 | 是否生成 preview、preview 间隔。 |
| 实验 | OpenVDB 实验管线开关。 |

材料分组中的两个白墨概念不能混用：

```text
模型内部填充材料 = 白墨：把颜色表层之间的模型实体区域写入 W 通道，稳定彩色甲片 Profile 默认使用此项。
叠加白墨底层：在模型区域额外叠加 W 通道白墨，默认关闭，仅在明确需要全模型白墨底层时开启。
```

当模型内部填充选择白墨时，纹理策略必须为“顶面纹理带”。如果历史模板仍使用“顶面纹理投影到实体”，纹理会占满全部模型层，使内部白墨区域变为 0；一键切片生成生效配置时会自动改为 1 层顶面纹理带并给出警告。

高级设置分布在：

| 页签 | 主要功能 |
|---|---|
| 工艺 Profile | 描述期望的 RGB/W/S/V 材料组合和报告校验要求。 |
| 材料策略 | 控制 RGB、白墨、光油如何写入生产通道。 |
| 材料角色 | 把 OBJ/3MF 材料名映射到 RGB、白墨、光油、支撑或忽略。 |
| 支撑 | 设置支撑模式、孤岛过滤、XY 膨胀和连通性。 |
| 配置差异 | 查看 UI 当前配置与磁盘 JSON 的差异。 |
| 生效配置 | 查看本次实际交给 `slicer_cli` 的会话配置摘要和模板差异。 |
| 设置说明 | 按中文设置名查看影响通道、默认值、生产安全和相关文档路径。 |

“常用”页关键控件的悬停说明和“设置说明”页来自同一份帮助元数据。当前覆盖模型填充、支撑、光油、preview、Legacy 生产引擎和 OpenVDB 候选/诊断引擎；OpenVDB 说明固定标记为默认关闭和非生产。更详细的材料和层策略说明见：

```text
docs/user_guides/SliceSoft切片策略与材料层说明.md
```

## 7. 常用配置建议

全彩 OBJ：

```text
场景/Profile: 纹理 OBJ / 彩色纹理浮雕 RGB
模板: samples/configs/textured/textured_relief_rgb.json
```

全彩 OBJ、全实体 RGB、无白墨：

```text
场景/Profile: 彩色纹理甲片 - 全实体 RGB + 下表面支撑（无白墨）
模板: samples/configs/material_process/obj_mtl_texture_rgb_only.json
说明: 贴图颜色沿模型实体列投影；W/V 保持不打印，S 支撑仍按下表面和内部镂空策略生成。
```

OBJ 全彩 + 白墨 + 光油：

```text
场景/Profile: 材料工艺 / OBJ 纹理 + RGB/W/V
模板: samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json
```

真实 3MF：

```text
场景/Profile: 3MF 真实模型 / 真实 3MF 01/02/03
```

UI smoke：

```text
场景/Profile: UI Smoke / UI 层预览 Smoke
```

## 8. 当前交互优化建议

当前 UI 已经比早期“配置文件 + 输出包”模式更清晰，但仍建议后续继续优化：

1. 把“导入模型并切片”升级为向导式流程：模型、材料策略、输出目录、预览设置四步。
2. 在导入 OBJ 后显示 MTL/贴图检测结果，包括贴图缺失、UV 缺失和 fallback 策略。
3. 将 OpenVDB 诊断按钮视觉上标为“实验”，避免误认为生产切片。
4. 在“运行切片”前显示当前执行引擎：Legacy / OpenVDB Diagnostic / Future OpenVDB Candidate。
5. 将常用配置页拆成“基础”“材料”“支撑”“预览”“实验”五组，减少参数堆叠。
6. 对模型高度、autoOrient 结果、输出层数给出运行前摘要。

## 9. 当前限制

当前 UI 不做：

```text
RIP 半色调
设备通信
喷头 bitstream
生产作业队列
完整 3D 模型编辑
OpenVDB production RGBWSV 输出
自动 mesh repair
```

OpenVDB 当前只能通过 UI 触发实验诊断，不应作为正式切片结果交付。

## 10. 阶段封口验证

需要复核完整 12C 工作台时，使用独立 fresh build 目录：

```powershell
.\scripts\Configure12CQtUi.ps1 -BuildDir build-12c-ui-r2-final -Config Debug
.\scripts\Run12CUiClosure.ps1 -BuildDir build-12c-ui-r2-final -Config Debug
ctest --test-dir build-12c-ui-r2-final -C Debug --output-on-failure
```

`Run12CUiClosure.ps1` 会重新生成 UI Smoke 输出包并执行 Profile、设置、生效配置、共享层、图例/探针、诊断区、OpenVDB 摘要和三种窗口尺寸的自动化检查。任一检查失败时脚本会返回非零退出码。
