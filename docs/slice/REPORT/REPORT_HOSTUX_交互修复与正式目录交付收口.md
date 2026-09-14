# HOSTUX 交互修复与正式目录交付收口

日期：2026-09-14。实现分支：`codex/host-visibility-navigation`。
任务真源：`docs/codex_task/current/TASKS_HOSTUX_选择导航高DPI与RIP进度.md`，UX-00..14 COMPLETE（宿主定向 Gate）。

## 最终行为

- 列表与二维/三维选择同步蓝色可见外轮廓，透明图案内部不描边；相机按鼠标落点旋转并加粗 XYZ 轴。
- 导入后可直接切换工艺，保留资源与模型摆放；支持的缩裹 T 工艺同步七通道策略，失败恢复原设置。
- 右侧页面独立滚动，RIP/变换使用子标签；常规尺寸无需整体滚动这些分组，小窗口保留溢出保护。右栏可拖宽至720逻辑像素。
- 参数输入框、未展开下拉框与滑条禁止滚轮改值；标签栏允许滚轮切换，画布拖动期间禁止滚轮缩放。
- RIP显示真实宿主阶段、累计耗时和输出文件观察数；手动输入跟随最近成功包，输出使用唯一目录且保留人工编辑。
- 主程序为 `slice_soft_test.exe`，保留相同内容的 `slicer_ui_host_sim.exe` 兼容入口。

## 首轮正式交付（历史记录）

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

本报告及两份任务清单以独立文档收口提交 `f0fca9b4` 记录。当时仅本地提交，分支尚未合入product。
无关 `analysis/`、`cache/`、`docs/team-collaboration/`、`model/obj/multi-material/gubao-xin-2D/`、
`model/obj/soft_test/` 保留在工作树，不纳入此次提交。

## UX-14 工艺预设复核与设置恢复修复

用户报告的顺序为：保留“彩色纹理生成”Profile，导入 `gubao05-dingwei.obj`，直接将常用预设改为“多图层透明”，点击开始切片。旧测试在这期间还切换过 Profile，不足以证明此路径。

新增真实主窗口测试严格保持上述顺序，先检查实际按钮，再点击按钮提交真实 Worker 作业。修复前的干净配置已可提交（`output/hostux-preset-before.log`），因此没有复现用户当时的置灰现场。用户未提供该时刻阻断文字，不能将下面发现直接认定为唯一根因。

确定性缺陷：`HostWorkspaceMatvolState` 只保存五个旧字段，遗漏自动材质名优先级、光油判定开关、光油阈值和退化面阈值；恢复逻辑仍要求两个手填材质名，拒绝多图层预设合法的自动命名设置。本机已保存设置也有同样遗漏。新增持久化测试修复前报 `MATVOL_PERSISTENCE_LOST_MULTILAYER_SETTINGS`（`output/hostux-persistence-before.log`），修复后通过。

- 完整保存和恢复上述四字段；仅对已知历史多图层预设的四字段全缺失状态恢复原默认值，不推测自定义工艺。部分缺失及非法数值继续拒绝。
- 作业页及开始按钮提示使用设置校验的具体原因；清空输出路径仍必须阻断，恢复有效路径后按钮重新可用。
- `hostux_preset_after_import` 和 `hostux_restored_preset_after_import` 均显示 `ready=1 button=1`，最终返回 `PM-SLICER-OK-0000`。恢复测试通过临时 INI 执行完整工作区保存/恢复，不改用户注册表。
- 两个实际包分别位于 `output/ux14/r6d749e689ce6/package` 和 `output/ux14/r699feab2f620/package`；均为100 DPI、0.15 mm、867x237、33层，`p0.rgbwsv.2`。按钮检查发生在低分辨率测试参数覆盖之前。
- Release定向回归19/19 PASS，81.41s：`output/hostux-preset-final-ctest.log`。新测试首次使用过长证据路径导致写包失败，已缩短测试目录重跑；没有修改生产写包算法。
- 源码行数门禁PASS（74项既有warning）：`output/hostux-preset-size.log`。正式发布构建记录 `output/hostux-ux14-publish-build.log`、`output/hostux-ux14-tools-build.log`。

这里完成的是实际发现的设置恢复缺陷、流程回归和阻断可观测性；不宣称已证明原置灰现场的所有成因。未做全仓回归、默认分辨率全模型矩阵或物理打印验收。

## UX-14 正式覆盖

2026-09-14再次备份并更新 `runtime/slicesoft/Release`。发布前版本一致性检查发现 CLI 仍是上次构建，已补建 CLI/Reader，使宿主、模块、Worker及CLI身份一致后才生成完整负载；未跳过该检查。

- `output/hostux-ux14-payload.log` 发布退出0，31个配置场景；负载 `output/hostux-ux14-payload/Release`。
- `output/hostux-ux14-formal-deploy.log`：670个负载文件SHA256一致，14个文件新增或变化，6910个既有输出的路径/长度/最后修改时间保持不变。
- 覆盖前备份：`output/hostux-formal-backup/20260914172639111`，含逐文件旧内容与部署前后哈希清单。
- 新旧主程序入口SHA256：`DF4DF45788BC6785F25D0822351A415C8C859DB00E4ED842DEAA92B139257A33`。
- module SHA256：`CB53401D79B63B029A4B2C01C21BE878BA3E79C71331357312C088B40008177F`；Worker SHA256：`0066270904F56B59030F7498C471DF95AF07916D4CD799B8AE56DAAB3E8E845A`。
- 正式目录原生Windows平台 `--self-test`、`--rip-module-self-test`、`--rip-ui-self-test` 均退出0；日志前缀 `output/hostux-ux14-formal--`。
- 最终源码门禁PASS（74项既有warning）：`output/hostux-ux14-final-size.log`。使用手册与 `SLICE_更新公告_2026-09-14.md` 随包同步；没有删除输出或改写用户设置。
