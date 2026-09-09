# XYPAD XY 原点补白验证与交付

日期：2026-09-09。任务真源：[XYPAD 清单](../../codex_task/current/TASKS_XYPAD_XY原点输出画幅补白.md)。
状态：本地功能/定向 Gate 完成，独立验证版已部署，后经用户确认覆盖原 Release 运行目录；按后续授权拆分提交至功能分支，未合入 product、未推送。

## 结果与边界

原 XPAD 仅扩展 X，Y 仍使用实际 Raster 并集最小 Y，故缺少平台原点到模型下侧的空白。本次新增独立默认关闭的“补齐至 Y=0”，与 X 开关可组合，支持六通道 Scene 与七通道 T Scene。
不采用切片后重写旧 TIFF；在最终层合成/首次写包前补空白，同步 manifest、报告、预览及返回网格。模型坐标/采样域/层厚/层数/通道顺序/极性均不变，`output` 目录不改为 `out`。

内部 minY-first 缓冲在前部补空行，已有 Writer 写成 maxY-first 后，空行位于 TIFF 下侧。新增通道值均为 255，preview mask 为 0。旧 X-only 配置和工作区不自动启用 Y；旧包不改写，T 单可见实例限制不变。

## 验证

Release 构建退出 0 后执行测试，不使用根目录旧 build。日志位于本机 output，不纳入 Git：

| Gate | 结果 / 证据 |
|---|---|
| 核心/宿主构建 | `output/xypad-build.log`；x_origin_padding_tests、slicer_ui_host_sim 及依赖模块/Worker 成功 |
| 定向目标构建 | `output/xypad-regression-build.log`、`output/xypad-final-build.log`；依赖库已先构建，后续测试目标使用 BuildProjectReferences=false 避免重复构建 |
| CTest | 9/9 PASS，7.15 s，`output/xypad-regression.log` |
| 六通道真实十模型 | 全部 23 层严格读取、原像素逐字节相等、空区/统计/原始 TIFF 行序 PASS，`output/xypad-real-xy.log` |
| 七通道真实 03.obj | 全部 32 层 TIFF、报告覆盖率、预览 PPM、空区与原始行序 PASS，`output/xypad-real-t.log` |
| 独立原始 TIFF 验证 | RawRows.h 直接用 LibTIFF 扫描物理行，不经过应用行序转换；确认原图从顶部保留、下部新增行全空 |
| UI/Profile/恢复 | 两轴独立、默认 false、有效 Profile hash、持久化往返、旧 X-only 不启用 Y；hb05/hb08 含 UI smoke PASS |
| 源码规模 | PASS，61 项既有警告，`output/xypad-size-final.log`；未扩大冻结核心文件行数 |
| 文档/截图 | 图 21 实际查看，两开关完整可见；手册补充 XY 说明和版本边界；diff 检查通过 |
| 部署 | 新目录 `runtime/slicesoft-xypad/Release`；宿主自检 `STAGE14E02_SELF_TEST_PASS spi=1 calls=6`，部署流程 RIP 模块自检 PASS，四个程序文件与构建产物 SHA256 一致 |

9 个 CTest 为 x_origin_padding_tests、multi_model_layer_composer_unit_tests、multi_model_production_service_unit_tests、tiff_row_layout_unit_tests、matvol_rgbwsvt_legacy_package_tests、hostflow_hb05_slice_settings、hostflow_hb08_workspace_state 及 hb05/hb08 UI smoke。不是全仓回归声明。

核心矩阵覆盖全关/仅 X/仅 Y/XY、正负零/分数原点、各向异性 DPI、半像素量化、多实例、借用/消费/流式、Y 溢出/非有限值、七通道 preview mask 与非 Scene 不补白。

首次 CTest 在六通道 Y-only 的整像素边界断言失败：原 pitch 计算的二进制浮点与 manifest 十进制舍入后重新 ceil 差一行。核心补白仍满足覆盖零点及不足一个像素余量，原像素无漂移；修正测试为校验物理原点/余量，不对已舍入 pitch 再求 ceil。保持原像素、所有新增字节、物理扫描行和配置接线检查不变；复跑 9/9 PASS。未借此改变旧 X 算法。

## 真实资产

### 十模型，保留源姿态和 XY

目录 `model/obj/alg_suoguo/20260908-HuangChenC`，10 个 OBJ，自动定向/排版关闭，逐实例 Z 触底；600×600 DPI，0.2 mm，23 层。

| 状态 | 网格 | origin XY (mm) |
|---|---|---|
| 不补白 | 4110×1375 | (20.07984, 1.506456) |
| 同时补 X/Y | 4585×1411 | (-0.0284933333333335, -0.017544) |

新增左 475 列、下 36 行。全层原模型字节、各通道打印计数不变；额外像素仅计入空白。
证据目录：`output/xpad/329664639250400/{off,on}/package`。
单次运行总耗时约 4.127 s / 4.591 s，仅作为此次执行记录，不是重复性能基线，也不能据此推导仅新增 Y 的耗时比例。

### 七通道独立路径

资产 `model/obj/reality/finger_suoguo/03.obj`，显式摆放 Xmin=21.07984、Ymin=5 mm，Z 触底；600×635 DPI，0.2 mm，32 层。
网格从 555×298 变为 1053×423，左 498 列、下 125 行；原点从 (21.07984,5) 变为 (-0.00216000000000349,0)。T 打印像素总数保持 464975。
证据目录：`output/xpad/t329705489729500/{off,on}/package`。各向异性参数与旧 XPAD 测时不同，不跨记录比较 T 总量/性能。

## 交付

从 `product/packaged-slicer` 新建 `codex/xy-origin-canvas-padding`，没有改写 product 分支历史。开发基线 88f087ed303f，程序版本 0.2.446-dev（dirty 为此次未提交工作树构建标识）。首次独立部署后，用户另行确认覆盖原运行目录。

打开 `runtime/slicesoft/Release/slicer_ui_host_sim.exe`，在“切片设置 → 输出画幅”按需勾选 X/Y 后重新切片。VSCode Quick Run 原启动入口现在使用此更新版本；独立验证副本仍保留。

2026-09-09 覆盖验证：更新 16 个差异文件，86 个程序/依赖/文档部署文件与验证版 SHA256 一致；目标目录宿主自检 `STAGE14E02_SELF_TEST_PASS spi=1 calls=6`，CLI 版本探测成功。保留 output、configs、model、samples 及用户设置，不删除文件。被覆盖的旧文件备份至 `runtime/slicesoft-backups/Release-before-xypad-20260909-174525`；随后同步本次交付状态文档。未重新编译，不改变此前定向测试结论。

未执行物理打印、目标设备验收或全仓 CTest；不把控件截图视为完整人工生产验收。先前的用户手册/RIP配图、团队文档、analysis/cache 和新增素材均保留，未与本任务混合提交。

## Git 同步

2026-09-09 用户授权同步 Git 分支后，按任务拆分：核心与测试 `12bca48d`、宿主开关及恢复 `e4cb2d27`，本文与设计/任务卡/图 21 及手册中 XYPAD 段落另作文档提交。手册及资产说明存在其他任务修改，按内容选择性暂存，不整体收录 RIP/导入章节及素材。提交前再次运行相同 9 项定向 CTest，9/9 PASS（9.21 s）。未重新构建二进制以改写版本号，已部署程序仍保留验证时的 88f087ed303f.dirty 来源标识。
