# HOSTUX 选择、导航、高 DPI 与 RIP 进度

日期：2026-09-14。用户已授权覆盖正式 Release 目录、同步手册、按任务提交并合入推送 product；不关闭正在使用的软件。

## Implementation Plan
### Problem Type
宿主交互与可观测性修复，不改变切片算法或生产包。
### Layer(s) Involved
宿主选择联动、CPU 显示、相机、Qt 布局、RIP 状态展示、CMake/VSCode 启动与部署。
### Official Documents
HOSTFLOW/RENDER 当前任务与宿主渲染边界，保持 ViewData/SPI/Worker 和六七通道合同。
### Historical Documents
旧选中染色和包围盒拾取是历史实现，不作为修复完成依据。
### AI Workspace Evidence
从 product/packaged-slicer 建立 codex/host-visibility-navigation；既有手册、RIP文档、配图、缓存和模型修改保留。
### Current Code Reality
选择只更新文字；3D 左键仅旋转；CPU Pick 只测 Z=0 矩形且不返回表面点；原点轴为单像素无标签；RIP stdout 丢弃细节；右栏无整体滚动保护。
### Current State
UX-00..14 COMPLETE（宿主定向 Gate）；UX-14补齐多图层参数持久化及阻断原因显示，两条真实模型流程切片成功。用户当次置灰现场未复现，不能把持久化缺陷认定为唯一原因。正式Release交付与分支记录见收口报告；不代表全仓/物理打印验收。下方较早轮次的“未覆盖/未提交”仅为当时历史记录。
### Target State
本地选择轮廓和列表联动；最近可见表面拾取、鼠标按下位置固定旋转锚点；三轴粗线与标签；自适应表单和滚动；短启动名兼容旧入口；真实RIP阶段、计时和输出文件观察计数。
### Historical State
已有包、工艺、模型变换和用户设置不因显示操作改变。
### Pending Confirmation
无。鼠标空白处使用相机焦平面锚点。供应方无内部逐阶段回调，不伪造内部百分比；输出文件出现不等于校验通过。
### Risk Points
透明材质拾取、遮挡轮廓、坐标映射比例、相机锚点跳动、缩放布局、旧exe入口和测试依赖、取消后RIP状态回退。
### Files To Change
HostMainWindowView、TopViewRenderPolicy、CPU raster/picking、CameraController、ThreeDCanvasWidget、宿主布局、RIP controller/panel、启动与部署脚本、定向测试。
### Verification Plan
Release目标构建退出0后，执行宿主相机/渲染/选择/UI/RIP定向测试；检查缩放布局与截图；独立部署验证，不修改既有切片输出。

| 任务 | 状态 | 完成日期 | 验证 |
|---|---|---|---|
| UX-14 默认Profile导入后切换多图层预设 | COMPLETE | 2026-09-14 | 修复4字段保存遗漏及自动材质名恢复误拒绝；恢复前失败用例转绿。严格原顺序与恢复设置两条真实按钮/Worker流程成功，19/19定向回归PASS；原现场未复现，新增具体阻断提示。正式负载670文件哈希一致，覆盖14文件、保留6910输出，三项部署自检PASS |
| UX-13 正式部署、手册与拆分提交 | COMPLETE | 2026-09-14 | 独立完整发布负载669文件哈希一致，正式目录替换31文件、保留6910个输出；宿主/RIP模块/RIP界面自检PASS；三份指南与六张新增图已同步，按任务本地提交，无合并/推送 |
| UX-00 准备和边界 | COMPLETE | 2026-09-14 | 源码与现有接口审计 |
| UX-01 选择与二维三维联动 | COMPLETE | 2026-09-14 | 2D/3D 轮廓像素、遮挡、材质内部不变、最近表面拾取、列表选择与旋转180度非对称预览匹配测试 PASS |
| UX-02 鼠标旋转与轴标识 | COMPLETE | 2026-09-14 | 正交/透视屏幕锚点不漂移、鼠标方向回归 PASS；三轴粗线与 X/Y/Z 截图检查 |
| UX-03 高DPI与短名称 | COMPLETE | 2026-09-14 | 100%/150%/200% 自动布局检测与实际宿主截图 PASS，含 3840x2160；部署入口 slice_soft_test.exe，保留旧名兼容副本 |
| UX-04 RIP阶段与进度 | COMPLETE | 2026-09-14 | 23层真实包副本经实际 RIP CLI 产生阶段/文件计数，诊断流程 PASS 24.682s；取消、超时、exit1/2 和清理 Gate PASS |
| UX-05 回归与交付 | COMPLETE | 2026-09-14 | Release 构建 PASS；10/10 定向 CTest PASS（37.21s）；独立部署自检/版本一致性 PASS；源码行数门禁和 diff 检查 PASS |
| UX-06 导入后切换工艺 | COMPLETE | 2026-09-14 | 导入后编辑/重绑快照字段不变、失败保留原工艺 PASS；改选后的 Worker 切片、重复作业和取消回归 PASS |
| UX-07 手动 RIP 默认路径 | COMPLETE | 2026-09-14 | 默认最近包 layers；输出父目录存在而最终目标不存在；人工输入保留、自动输出成功轮换、RIP UI 自检 PASS |
| UX-08 蓝色外轮廓与内部透明孔洞 | COMPLETE | 2026-09-14 | 内部透明孔洞像素不变、2D/3D/遮挡测试 PASS；真实 gubao05-dingwei 蓝色外轮廓及放大截图检查 |
| UX-09 高 DPI 复合字段真实裁剪 | COMPLETE | 2026-09-14 | 100%/150%/200% 完整宿主展开所有设置段，父容器包含输入框、内部编辑器不设最小高度、标签尺寸 PASS；新增 3 项 CTest |
| UX-10 导入后彩色纹理切换缩裹 T | COMPLETE | 2026-09-14 | 已支持预设六/七通道往返、用户参数保留、无 T 变体拒绝 PASS；08-04.obj 实际 UI/Worker 成功，42 层七通道包 |
| UX-11 滚轮防误改与导航边界 | COMPLETE | 2026-09-14 | 主窗口限定滚轮防护、焦点/动态控件/弹出列表/滚动转交/画布拖动回归 PASS；三档 DPI 和实际主窗口控件测试 PASS；16 项定向回归、独立部署自检 PASS |
| UX-12 右栏逐页滚动与标签滚轮恢复 | COMPLETE | 2026-09-14 | 三档 DPI 六页面/全部子页：RIP、变换、模型、工艺配置常规尺寸零纵向溢出；小窗口保护、标签双向滚轮及参数防误改 PASS；16 项回归及真实 Worker 切片 PASS |

## UX-12 准备与范围
- 当前证据：整组 QTabWidget 放在同一个 QScrollArea 中，最长的切片设置页面会抬高其他所有页面，导致本来能容纳的页面也显示滚动条。RIP 和变换表单全部上下排列，直接关闭滚动条会重现控件不可达。
- 方案：标签栏固定在滚动视口外，各页独立按需滚动，切换页面不继承其他页的长度/位置。模型、工艺配置使用现有列表/文本内部滚动；切片设置保留长表单滚动，作业页仅溢出时滚动。
- RIP 分为参数/路径/手动 RIP，变换与排版分为变换/导入与排版；复用现有控件和提交回调，不改变选项值。紧凑标签与输入同排，窄栏自动换行；常规尺寸无需页面滚动，小窗口或长诊断信息溢出时仍保留保护，不强制隐藏而裁剪。
- 标签栏恢复 Qt 原生滚轮切换，包括新增分组标签；参数滚轮防误改不变。仅鼠标在标签栏时切换，滚动参数区不能切换页。
- 影响文件：宿主右栏布局 helper、HostMainWindow、滚轮策略、布局/滚轮测试及规则文档。不改 SPI、切片/RIP 算法，不覆盖其他任务的手册和模型修改。
- Gate：三档 DPI 遍历六个页面及全部子页，确认常规尺寸 RIP/变换无滚动溢出；父容器/文字无裁剪、标签双向滚轮、参数不改值、既有切片/RIP 工作流回归；独立验证版交付。

## UX-11 Implementation Plan
本节为上一轮实施记录；其中标签栏禁用滚轮的决定已由 UX-12 按用户要求覆盖，当前允许标签栏滚轮切换。
- Problem Type / Layers：Qt 宿主输入防误触，不改 Profile 内容、切片算法或协议。
- Official Documents：HOSTUX 本卡、HOSTFLOW 宿主交互边界；Historical Documents：前轮布局/选择报告仅为背景。
- AI Workspace Evidence：沿用 codex/host-visibility-navigation；既有手册、模型、配图修改仍保留，不覆盖。
- Current Code Reality / Current State：Qt 默认滚轮可修改数值、下拉选项、滑条和标签页；画布平移/旋转期间滚轮仍触发缩放。缺少统一防护。
- Target State：滚轮用于导航而非更改业务参数。2D/3D 有图且无鼠标键按下时允许缩放；表单、列表、报告、结果图片滚动区域允许滚动；已展开下拉列表允许滚动浏览。数值框、关闭的下拉框、滑条、标签页禁用滚轮改值/切换，获得焦点也不例外；有上级滚动容器时转交滚动，不吞掉正常浏览。数值点击箭头/输入、滑条拖动、键盘和中键拖动画布均保留。
- Pending Confirmation：无；默认不新增开关或 Ctrl 滚轮改值后门，避免高风险参数误改。
- Risk Points：下拉弹出视图被误拦、滚动转发递归/重复、像素滚动丢失、事件坐标、焦点控件被修改、动态创建控件漏装过滤器。
- Files To Change：新增主窗口限定事件过滤器及独立测试；接入 HostMainWindow；两个画布 wheelEvent；本任务卡。
- Verification Plan：无焦点/有焦点数值与下拉/滑条/标签页不变、滚动容器继续滚动、弹出列表浏览、键盘/箭头仍有效、画布缩放及按键拖动期间不缩放；Release /W4 /WX、三档 DPI 与既有宿主回归；不覆盖原 product 目录。

## 用户复测后的修正准备
- UX-03 旧 Gate 仅看控件自身尺寸，漏掉内部编辑器和父容器裁剪；Qt 复合字段的最小尺寸未传递，展开后的 RGB 行同样受影响。旧截图 PASS 不代表该问题已经完全解决。
- UX-01 的黄色来源是选中描边而非模型数据；原算法把内部透明孔洞/栅格间隙也当边缘。改为蓝色、仅描与外部背景相邻的轮廓，不改网格/材质/TIFF。
- UX-06 仅重绑 Profile，遗漏从六通道到七通道的 packageprotocol 和 transferchannel 策略；因此默认彩色纹理导入后改选缩裹材料仍不满足已有一致性校验。显式切换 T 时复用目录中对应 `_rgbwsvt` 变体，非 T 时关闭 T；没有已验证 T 变体的自定义/候选工艺明确提示而不静默改变工艺。
- 用户指定真实资产 `model/obj/reality/finger_suoguo/08-04.obj`；计划以 100 DPI/0.15 mm 验证实际主窗口导入、切换、提交、Worker 成功和七通道包。此为快速工作流回归，不冒充默认分辨率/物理打印验收。

## 补充准备（2026-09-14）
- 当前根因：SetPendingSceneContext 拒绝导入后的 Profile 变化，设置面板另有一致性检查；旧测试只证明清空后重导成功，并未覆盖原模型直接改选。
- 方案：读取权威快照，仅替换宿主拥有的场景及实例 resolvedProfileId，使用既有 inline scene.apply_operation 的零平移提交重新建立权威状态，不改冻结协议、不改几何算法、不重导入。界面成功后刷新场景缓存与准入，失败保留原工艺/模型；运行中的作业仍禁止切换。设备画幅变更维持原限制。
- 边界：既有模块不提供 scene.release；旧场景元数据由模块会话持有至退出，底层模型资源共享。高频切换的长期内存预算不属于本次门禁，不能宣称新场景完全无内存成本。
- RIP 只创建默认输出父目录，最终目标目录仍保持不存在，不取消防覆盖或输入输出隔离校验。用户人工设置的输入/输出不被后续自动值覆盖。
- 验证计划：导入/变换后切换，核对实例和资源快照，实际 Worker 切片及重复作业回归；RIP 默认路径和三档缩放面板回归；独立验证版部署，不覆盖原运行目录。

## 补充验证与交付
- Release 构建：`output/hostux-followup-build.log` 与 `output/hostux-followup-ui-build.log`，退出0。
- `output/hostux-followup-ctest.log`：13/13 PASS，39.16s。包含工艺重绑、真实 Worker 作业、设置页、RIP页与原交互回归。
- RIP旧UI自测要求输出目录为空，已按新需求改为检查非空绝对路径且最终目录尚不存在；三档交互测试补充自动路径/保留人工输入/下一次作业目录测试。
- 4K宿主截图与布局检查：`output/hostux-followup-ui`，DPR=2 PASS。源码行数门禁 PASS（74项既有warning），`git diff --check` PASS。
- 更新 `runtime/slicesoft-hostux/Release/slice_soft_test.exe` 及兼容旧名，复制前确认未运行，并验证现有 module/worker 与构建版本 SHA256 一致。新旧入口与构建主程序 SHA256 相同；部署后模块自检、RIP UI自检 PASS。
- 覆盖前主程序备份：`output/hostux-binary-backup/20260914063024564`。没有替换该测试目录的其他资源/用户输出，更未覆盖 `runtime/slicesoft/Release`。
- 仍未提交、合并或推送；未进行全仓回归或物理打印验收。

## 实现与交付
- 选择最终使用蓝内边、深色外边，沿可见外轮廓绘制，排除内部透明孔洞；从右侧列表或画布选择均更新本地缓存显示，不修改模型坐标、场景 revision 或切片数据。
- 三维拾取使用缓存三角形、深度、透视校正和透明判定。旋转以按下时的表面点为锚，空白处使用相机焦平面，拖动方向已翻转；不会将模型本身旋转。
- 额外修复俯视180度旋转时旧 axisAligned 快捷绘制把负向变换归一为正向矩形的问题，避免画面、轮廓与拾取不一致。只影响渲染，不改 TIFF。
- 表单标签独占一行，输入框有字体相关最小尺寸，右侧区域支持滚动，避免窄栏挤压。未修改其他任务正在维护的用户手册。
- RIP 每500ms刷新真实宿主阶段、累计时间和已出现 TIFF 文件数。文件出现不等于完成或通过校验；输出校验期间不伪造100%。供应方DLL内部细分计算步骤仍没有接口，不能声称已获得内部精确百分比。
- 验证版：`runtime/slicesoft-hostux/Release/slice_soft_test.exe`。原 `runtime/slicesoft/Release` 未覆盖。现有 VSCode Release 构建启动配置会在构建部署后使用新名称；原目录尚未部署新名称时不能直接用 No Build 启动项。
- 分支：`codex/host-visibility-navigation`；本轮未提交、未合并、未推送。既有手册、素材、模型及缓存修改未纳入本任务。

## 验证证据
- 构建：`output/hostux-close-build.log`；定向 CTest：`output/hostux-ctest.log`。
- Qt真实宿主各页截图：`output/hostux-ui/{1,1.5,2}/tab_*.png`。2D/3D合成fixture截图：`build-slicesoft/main/hostux-evidence/{1,1.5,2}/`，不是用户真实模型截图。
- RIP生命周期：`output/hostux-rip-lifecycle.log`。实际RIP正向观察：`output/hostux-rip-positive.log`，文件观察从0/23递增，输出校验与源身份复核期间计时连续，最终发布至独立副本 `rip_diagnostic`。
- 该RIP正向实验使用 `diagnostic_unvalidated`，不是 strict S2 或物理打印验收；没有修改原包、生产默认校验模式或协议。
- 部署：`output/hostux-deploy.log`；源码门禁：`output/hostux-size.log`（PASS，74项既有warning）；VSCode JSON和部署PowerShell语法解析 PASS。
- 未运行全仓回归，未进行物理打印、多显示器跨屏拖动或用户真实多模型人工交互验收。

## 修订
- r12：UX-14相关代码/文档以51281a1e、618c75ce、2704ee3e拆分提交，product已快进合入并推送2704ee3e，远端核对一致；原功能分支更名为codex/feature-host-ui。本条收口文档随后同步推送，不修改既有验证口径。
- r11：UX-14 COMPLETE。复现并修复持久化遗漏，保留原始置灰现场未复现的限制；补充两条真实GUI/Worker回归、具体按钮阻断原因及2026-09-14更新公告。用户已授权正式覆盖、合并推送与功能分支更名。
- r10准备：用户复测仍出现多图层预设置灰；重新打开独立UX-14，验证完成后合入并推送product，整理今日更新公告。

## UX-14 Implementation Plan
### Problem Type
宿主常用预设切换后的就绪状态缺陷，优先保留失败复现。
### Layer(s) Involved
HostSliceSettingsPanel、HostMainWindow就绪接线和真实GUI自测。
### Official Documents
HOSTUX任务及正式交付报告、当前多图层命名规范；保持材料准入和生产协议。
### Historical Documents
UX-10及先前gubao05测试属于其他路径证据，不证明本次用户顺序通过。
### AI Workspace Evidence
当前分支已提交八项修复，未跟踪模型/缓存等保持原样；用户授权后续合入、推送与分支处置。
### Current Code Reality
常用预设应用会同步多个子面板并发出设置通知；IsReady依赖有效Profile缓存；作业页仅显示泛化阻断文案。
### Current State
待复现：默认彩色Profile导入gubao05后直接选多图层透明，期间不再改Profile。
### Target State
有效预设切换后按钮立即就绪且实际Worker成功；真正非法配置继续阻断并说明具体原因。
### Historical State
生产核心和现有输出保持，不能通过绕过准入或仅手动刷新掩盖缺陷。
### Pending Confirmation
无。沿用gubao05-dingwei.obj真实资产；用户已授权合并推送。
### Risk Points
信号重入、部分状态更新、缓存就绪失真、误放行材料/拓扑失败、旧测试修改流程后掩盖问题。
### Files To Change
按实测最小修改宿主设置或就绪实现及HostUxSceneSmoke，补充定向测试和公告。
### Verification Plan
先失败复现；修复后严格原顺序点真实开始按钮并实际切片；运行相关工艺/宿主回归、源码门禁和diff检查，再备份部署、提交合入推送。
- r9：UX-13 COMPLETE；用户授权正式覆盖、主手册同步及拆分提交。右栏最大宽度由420扩至720逻辑像素，新增宽栏200%真实截图；669文件部署哈希、6910个输出保留与正式自检通过。
- r1：冻结七项问题边界及低风险实现策略，显示与可观测性不改变生产数据。
- r2：完成 UX-01..05，记录真实 Gate、RIP接口限制、独立交付与未验证边界；不覆盖并行手册任务。
- r3：根据用户补充新增 UX-06/07，记录根因、现有接口复用、失败回退和路径防覆盖合同。
- r4：UX-06/07收口，补充13项回归、部署备份与未验证边界。
- r5：用户复测推翻首轮完整性结论，新增 UX-08..10；补上内部编辑器/复合字段 Gate、真实透明材质外轮廓及 T 协议同步。
- r6：UX-08..10 收口，记录 16 项回归、真实资产工作流、独立部署和剩余验证边界。
- r7：UX-11 准备、实现与收口；滚轮只做导航，参数/标签/滑条不因悬停或焦点被误改，保留弹出列表滚动与中键拖动。
- r8：UX-12 完成：右栏改为各页独立按需滚动，RIP/变换分组分页并用紧凑自适应行；恢复标签滚轮，记录小窗口边界与回归交付。

## UX-12 验证与交付
- 保留原面板对象作为 QTabWidget 直接页面，仅将其原布局移入页内视口，既有 `setCurrentWidget(m_sliceJobPanel/m_ripSettingsPanel)` 自动导航不变。
- `output/hostux-pages-final-build.log` Release 构建退出0；`output/hostux-pages-final-ctest.log` 16/16 PASS，61.92s；`output/hostux-pages-size.log` 门禁 PASS（74 项既有 warning）；`git diff --check` PASS。
- 完整宿主在 100%/150%/200% 缩放、1920x1080 逻辑尺寸遍历全部六页面及 RIP 三子页、变换两子页。模型、工艺配置、RIP、变换页面纵向滚动范围均为0。新增顶级/子级标签双向滚轮断言、参数滚轮不改页断言。
- 小窗口请求 1280x720 和 1024x600，受原主窗口最小尺寸限制，后一请求实际截图为 1024x642。验证输入父容器不裁剪、滚动到页尾仍保留顶级标签栏；不宣称任意物理屏幕尺寸均能无滚动显示全部内容。
- Qt offscreen 截图位于 `build-slicesoft/main/hostux-full-ui-evidence/{1,1.5,2}/tab_*_section_*.png` 及 `small_*_tab_*.png`；已检查 RIP、变换常规截图和最小窗口的溢出保护截图。
- `output/hostux-pages-finger.log`：08-04.obj 真实主窗口导入、工艺切换及 100 DPI/0.15 mm 七通道 Worker 切片返回 `PM-SLICER-OK-0000`，验证布局调整不阻断作业跳转/提交；不是默认分辨率或物理打印验收。
- 已更新独立 `runtime/slicesoft-hostux/Release` 的新旧 exe 入口和滚轮规则说明；module/worker 与构建哈希一致，部署模块自检及原生 Windows RIP UI 自检 PASS。
- 覆盖前备份 `output/hostux-binary-backup/20260914155332111`；exe SHA256 `E1D4CC0287AD024F9002662D937D2324C58554F7ED9491084B016B18F336D866`。未覆盖 product 运行目录，未提交/合并/推送，其他任务的手册/配图/模型修改保留。

## UX-11 验证与交付
- 新增 `HostWheelPolicy.h`，应用级事件过滤仅作用于所属主窗口；动态控件自动覆盖，不拦其他窗口；优先允许滚动视图及下拉弹出列表。被拦控件的滚轮携带原 angleDelta/pixelDelta/phase/modifier/source 转交最近上级滚动视口，不改变参数。
- 两个画布仅在有图、无鼠标按键且不处于拖动状态时接收滚轮缩放；中键拖动平移和原有鼠标拖动方向不变。
- 新增 `HostWheelPolicyTests.cpp`：有/无焦点的数值框、内部编辑器、下拉框、滑条、标签页值和信号不变；滚动转交、像素滚轮防误改、动态控件、独立窗口不受影响、弹出列表滚动不提交选项、键盘/显式步进、空画布和拖动期间不缩放均 PASS。
- 完整主窗口三档缩放测试新增真实设置控件聚焦后滚轮不改值检查；`output/hostux-wheel-final-build.log` 构建退出0；`output/hostux-wheel-final-ctest.log` 16/16 PASS，44.11s。
- 源码行数门禁 `output/hostux-wheel-size.log` PASS（74 项既有 warning）；`git diff --check` PASS。未运行全仓回归、多显示器动态切换或全部触控板设备实测。
- 新增独立规则说明 `docs/user_guides/SLICE_HOST_鼠标滚轮交互规则.md`，不改其他任务正在维护的主手册，并复制至验证版同名目录。
- 已更新 `runtime/slicesoft-hostux/Release` 下新旧两个 exe 入口，未覆盖原 `runtime/slicesoft/Release`。备份：`output/hostux-binary-backup/20260914154219058`；新 exe SHA256 `8577ED17F173314572B5FEF32A6E9D940DC54C3BFAFFF13E63AAB8023800AABB`；模块/Worker 哈希一致、部署模块自检和原生 Windows RIP UI 自检 PASS。
- 本轮无提交、合并或推送；既有模型、手册和配图修改原样保留。

## 第二轮修复验证与交付
- Release 构建退出0：`output/hostux-r2-final-build.log`；实际布局定位中先后发现 DPI 复合行与展开后的回退 RGB 行父容器高度 27、子控件高度 30。现已让复合字段布局动态传递最小尺寸，不仅设置子控件高度。
- CTest：`output/hostux-r2-final-ctest.log`，16/16 PASS，43.78s。新增 `hostux_full_ui_scale_{1,1.5,2}` 覆盖完整主窗口、展开状态及 Profile 协议往返，旧单控件测试继续保留。
- 展开状态截图：`output/hostux-r2-ui/{1,1.5,2}/tab_*.png`，200% 为 3840x2160。这里是 Qt offscreen 自动化验证；未做多显示器动态跨屏/所有系统字体组合验证。
- `output/hostux-r2-finger-final.log`：08-04.obj 导入后 Profile 连续切换均 ready=1，最终 T 工艺 Worker 返回 `PM-SLICER-OK-0000`；`package/manifest.json` 为 `p0.rgbwsvt.1`、RGBWSVT 七通道、42 层、100x100 DPI/0.15 mm。
- `output/hostux-r2-gubao-detail.log`：gubao05-dingwei.obj 导入后切换多图层透明实际提交成功；`selected.png` 与 `selected_detail.png` 确认蓝色外轮廓，不再遍布黄色内部描边。此项为该单模型快速回归，不代表目录全部 22 个模型或默认生产分辨率矩阵。
- 源码行数门禁 `output/hostux-r2-size.log` PASS（74 项既有 warning），`git diff --check` PASS。
- 已更新独立验证版 `runtime/slicesoft-hostux/Release/slice_soft_test.exe` 和兼容旧名；覆盖前确认未运行，module/worker 与构建 SHA256 完全一致；旧 exe 备份 `output/hostux-binary-backup/20260914152212354`。
- 新 exe SHA256：`29A2A5109F6CA9C69CCB462CC6B25E111D3B330E7C2444F4206E40A4433F35F9`。部署模块自检 PASS、原生 Windows 平台 RIP UI 自检 PASS。部署包只有 windows 平台插件，首次误用 offscreen 自检因缺插件失败，已仅关闭该自检进程并用 windows 重跑通过；未改变部署插件集合。
- 未覆盖 `runtime/slicesoft/Release`，未提交/合并/推送，未改其他任务的手册、模型和配图；未进行全仓回归或物理打印。
