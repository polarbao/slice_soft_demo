# HOSTUX 交互修复与正式目录交付收口

日期：2026-09-14。实现分支：`codex/host-visibility-navigation`。
任务真源：`docs/codex_task/current/TASKS_HOSTUX_选择导航高DPI与RIP进度.md`，UX-00..13 COMPLETE。

## 最终行为

- 列表与二维/三维选择同步蓝色可见外轮廓，透明图案内部不描边；相机按鼠标落点旋转并加粗 XYZ 轴。
- 导入后可直接切换工艺，保留资源与模型摆放；支持的缩裹 T 工艺同步七通道策略，失败恢复原设置。
- 右侧页面独立滚动，RIP/变换使用子标签；常规尺寸无需整体滚动这些分组，小窗口保留溢出保护。右栏可拖宽至720逻辑像素。
- 参数输入框、未展开下拉框与滑条禁止滚轮改值；标签栏允许滚轮切换，画布拖动期间禁止滚轮缩放。
- RIP显示真实宿主阶段、累计耗时和输出文件观察数；手动输入跟随最近成功包，输出使用唯一目录且保留人工编辑。
- 主程序为 `slice_soft_test.exe`，保留相同内容的 `slicer_ui_host_sim.exe` 兼容入口。

## 正式交付

正式目录：`runtime/slicesoft/Release`。此次授权仅覆盖正式运行文件和本地提交，没有合并或推送分支。
通过 `PrepareSliceSoftRuntime.ps1 -DeployOnly` 在独立目录生成配套负载后，逐文件备份并覆盖正式目录。
没有删除正式目录、切片输出或用户设置，也没有关闭用户运行中的程序。

- 负载：`output/hostux-formal-payload-20260914/Release`，31个配置场景。
- 备份：`output/hostux-formal-backup/20260914161550102/files`；同级 `inventory.json` 记录各路径部署前后SHA256。
- 覆盖证据：`output/hostux-formal-deploy.log`；669个负载文件部署后SHA256一致，其中31个新增或变化文件。
- 新旧主程序入口SHA256均为 `41014D35E15BF503E19574948072E2EE4C5479848919AFB816A4D0C4BE78264F`。
- 原输出保护：6910个原有文件的路径、长度、最后修改时间逐项保持不变；未读取重写切片内容。
- 手册：主手册、RIP指南、滚轮说明及全部27张配图已同步，新增图22..27均逐图查看；主手册直接引用的命名规范和XPAD/XYPAD报告随包复制。

## 验证及限制

- Release构建退出0：`output/hostux-publish-resume-build.log`。发布脚本退出0：`output/hostux-formal-payload.log`。
- 定向CTest 16/16 PASS（56.85s）：`output/hostux-publish-final-ctest.log`。
- 右栏宽度调整后的100%/150%/200%完整UI复验3/3 PASS（25.68s）：`output/hostux-publish-wide-ctest.log`。
- 正式目录 `--self-test`、`--rip-module-self-test`、Windows平台 `--rip-ui-self-test` 均退出0。
- 三份指南文件链接有效；主手册27个、RIP指南5个图片引用均可解码；VSCode JSON与部署/截图脚本语法检查通过。
- 源码行数门禁以本轮提交前基线 `101277f2^` 验证PASS（74项既有warning）：`output/hostux-publish-final-size.log`；`git diff --check` PASS。

此前真实08-04.obj工艺切换以100 DPI/0.15mm验证42层七通道包成功；gubao05-dingwei单模型检查了选中截图及低分辨率工艺提交。
这些证据不代表默认生产参数下全目录矩阵、全仓回归、多显示器动态切换或物理打印验收。
RIP文件出现不等于写完或校验通过；供应方DLL内部算法百分比仍无接口，未伪造内部进度。
本次没有修改切片核心、SPI/Worker/ViewData协议、RGBWSV/RGBWSVT数据合同或旧切片文件。

## 拆分提交

| 提交 | 任务 |
|---|---|
| 101277f2 | 导入后工艺重绑及缩裹T策略同步 |
| 1853e694 | 蓝色轮廓、表面拾取与鼠标导航 |
| cf7dfb6c | 主窗口选择刷新接线 |
| be8bc4dc | RIP进度及手动目录 |
| d3c13204 | 高DPI、逐页布局与滚轮防误改 |
| 89a793e3 | 使用手册、截图与取图脚本 |
| 279ec248 | 短程序名与配套文档部署 |

本报告及两份任务清单以独立文档收口提交记录。仅本地提交，分支尚未合入product。
无关 `analysis/`、`cache/`、`docs/team-collaboration/`、`model/obj/multi-material/gubao-xin-2D/`、
`model/obj/soft_test/` 保留在工作树，不纳入此次提交。
