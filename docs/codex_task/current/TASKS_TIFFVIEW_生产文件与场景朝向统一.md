# TIFFVIEW 生产文件与场景朝向统一

日期：2026-09-08。授权：用户要求生成文件、模型俯视及结果预览一致，并允许同步修复上下游软件；物理打印不作为本次前置。

收口状态：TV-00..04 COMPLETE。阅读入口：[专项收口总结](../../slice/REPORT/REPORT_TIFFVIEW_生产文件与场景朝向统一收口总结.md)。任务状态以本清单为准，技术约定以决策文档为准。

## 准备裁决

- 已确认 ND002 的 Z=90 度变换正确；原 TIFF 的首行是世界 minY，而显示首行是 maxY。六通道和 RIP 样本均证实是 Y 反射，不是 X 反射。
- 内部几何/材料 buffer 继续 minY-first；新生产 TIFF 实际写出 maxY-first，Orientation=1，不依赖查看软件执行翻转标签。
- 受控存储约定修订：ImageDescription 扩展为 `RGBWSV;rowOrder=max_y_first` 或 `RGBWSVT;rowOrder=max_y_first`；manifest.tiff.rowOrder 同步声明。旧描述/缺失声明按旧 minY-first 读取。未知或冲突必须拒绝。
- Native Reader 返回的 pixels 仍为 minY-first，spec.row_order 描述磁盘布局；由此保持材料统计、预览、算法及调用方合同。新旧 TIFF 不得混读猜测。
- RGBWSV/RGBWSVT 通道顺序、uint8、black_is_print、场景坐标、模型变换不变；不改 SPI/Worker ABI，不改既有包，不覆盖源模型。
- Writer 只复用条带/瓦片 scratch，不为翻转另建无条件整层副本或 layer×pixel 栈。额外内存上界为一个配置条带；若用户将一个条带设为整幅面，该条带本身也会达到整层大小。默认 64 行。Legacy/Global/Scene、六/七通道与显式 handwritten 路径一并覆盖。
- 独立以 LibTIFF 原始行读取为 oracle，不能仅靠 Writer/Reader 对称往返自证。软件 RIP 验证不等于物理打印验证。
- DPI 不等导致的像素宽高比与物理宽高比差异不是镜像；本次不改变切片采样密度。

准备结论：PREPARED / IMPLEMENTATION GO。原审计报告中的待用户确认已由本轮授权解除。

## 清单

| 任务 | 状态 | 完成日期 | 验证 |
| --- | --- | --- | --- |
| TV-00 根因与受控约定 | COMPLETE | 2026-09-08 | 旧包 147 层标签审计，7 个非空通道样本 flipY 零差异；用户授权 |
| TV-01 存储与读取兼容 | COMPLETE | 2026-09-08 | Release 构建退出 0；23/23 CTest；24 存储组合、16 生产方向、六/七通道、错误元数据与包/层冲突拒绝 |
| TV-02 预览及软件 RIP 同步 | COMPLETE | 2026-09-08 | 新包 7 组真实 SPI 预览与原始 TIFF 占用原样零差异；旧包兼容 PASS；真实软件 RIP 147 层诊断输出成功，0/30/90 层 S 占用原样零差异 |
| TV-03 实模验证与部署 | COMPLETE | 2026-09-08 | ND002 原包经共享生产 Writer 派生 147 层，独立原始字节对照全部零材料差异，非重新计算切片；Release 部署退出 0、自检通过，5 个程序文件与构建哈希一致；部署后 SPI 7/7 零差异 |
| TV-04 提交拆分与文档收口 | COMPLETE | 2026-09-08 | 修复 `47ed7f3`、测试 `6a5c48c` 与独立文档提交；本轮重跑 Release 定向 CTest 23/23 PASS，12.01 秒；源文件规模门禁 PASS（61 项既有警告）；索引、总结及本清单同步 |

## 交付范围

- 本次拆分为修复、回归测试、文档三笔提交，不推送远端。Writer、Reader、manifest 与预览绑定保留在同一修复提交中，避免只提交单端造成新旧包误向。
- 无关 `model/obj/multi-material/gubao05/gubao05-dingwei.mtl`、`gubao05-dingwei.obj`、`model/obj/multi-material/xx04/` 与 `analysis/` 不纳入本专项提交。
- 本轮只整理提交和文档、复跑定向测试，没有再次编译或部署。已验证的 Release 程序仍使用上轮构建版本字符串，提交编号变化不表示程序已重新构建。

## 修订记录

- 2026-09-08：冻结本轮实施合同；保留工作树无关 model/analysis 修改，不提交、不覆盖原生产包。
- 2026-09-08：TV-01 完成；新增真实包重编码证据工具和独立 LibTIFF 原始行断言。S2 软件描述符 2 正向 + 7 负向 PASS；源文件规模门禁相对 HEAD PASS（61 项既有警告）。
- 2026-09-08：TV-02/03 收口。部署目录被占用时走既有不可变程序文件原位同步回退，保留 output，原包 manifest 哈希确认未变；未关闭用户进程。真实 RIP 为 `diagnostic_unvalidated`，物理打印/外部验收不计 PASS。该阶段尚未提交，随后由 TV-04 完成提交收口。
- 2026-09-08：用户授权按任务拆分提交；TV-04 完成，新增 REPORT 目录收口总结与双索引，保留历史排查报告。复跑日志为 `output/tiffview/ctest-precommit.log`，未将本地大体积验证产物纳入 Git，未推送。
