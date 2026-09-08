# TIFFVIEW 生产文件与场景朝向统一收口总结

日期：2026-09-08。状态：TV-00..04 COMPLETE / LOCAL_GATES_PASS / RELEASE_DEPLOYED。

本总结按正式文档目录约定存放于 `docs/slice/REPORT/`，汇总已完成工作，不替代任务清单或技术裁决。仅收口 TIFFVIEW，不改变 MEMFLOW、FRAME 等其他专项状态。

## 1. 目标与根因

用户要求生成的切片 TIFF、结果预览和模型生产俯视位置一致，并授权同步修改上下游软件；暂不以打印设备方向作为前置条件。

ND002 模型绕 Z 轴旋转 90 度后，工作区上排 6 个、下排 4 个，而原 TIFF 上排 4 个、下排 6 个。根因是内部栅格第 0 行对应场景最小 Y，原 Writer 原样写出；图片第 0 行显示在顶部，而工作区与结果预览为 +Y 向上。该差异是整幅面 Y 反射，不是 Z 旋转额外增加了模型镜像，也不是定位素材排版改变。

## 2. 最终规则与修复范围

| 对象 | 当前约定 |
| --- | --- |
| 新生产 TIFF | 实际首行为场景最大 Y，显式 Orientation=1，直接打开与生产俯视同向 |
| 新文件元数据 | ImageDescription 为 `RGBWSV;rowOrder=max_y_first` 或 `RGBWSVT;rowOrder=max_y_first`；manifest.tiff.rowOrder 同步为 `max_y_first` |
| 旧包 | 原描述或缺失行序声明按 minY-first 解读，不自动改写历史文件 |
| Native Reader / Writer 输入 | 内部 pixels 保持 minY-first；读取新文件时恢复内部规范，避免预览二次错误翻转 |
| 错误数据 | 未知行序、非 1 的 Orientation 或包与层声明冲突必须拒绝 |

完成共享六/七通道 Writer、Native Reader、严格包校验、预览层索引和诊断 PNG/PPM 同步。覆盖 Legacy、Global、Scene 共享生产路径，stripped/tiled、none/PackBits 和显式 handwritten 六通道验证轨道。条带翻转只复用一个配置条带的缓冲，不增加整栈；若条带配置为整层，缓冲也会达到该条带大小。

本次属于用户授权的存储约定受控扩展，不应描述成输出文件完全不变。场景坐标、用户显式旋转与镜像、材料通道顺序、uint8、black_is_print、DPI、层高和 SPI/Worker ABI 均未改变。旧 Reader 不能与新 Writer 输出混用；软件组件应成套更新。

## 3. 验证与证据

| 验证项 | 实际结果 | 证据来源 |
| --- | --- | --- |
| Release 构建 | 上轮两批构建退出 0；新增测试 `/W4 /WX` | `output/tiffview/build-final.log` 及详细报告第 7 节 |
| 定向回归 | 上轮 23/23 PASS；本轮提交前再次 23/23 PASS，12.01 秒 | `output/tiffview/ctest.log`、`ctest-precommit.log` |
| 独立存储断言 | 24 存储组合、16 方向/显式镜像输入组合及元数据负例通过 | `tests/unit/tiff_row_layout/Main.cpp` |
| ND002 全层材料 | 147 层严格校验通过，统一世界坐标后所有材料字节零变化 | `output/tiffview/new-preview/evidence.json` |
| TIFF 与实际模块预览 | 新包 7 组非空 W/S 原样占用差异为 0；旧包经新 Reader 仍保持原预览 | `new-preview/evidence.json`、`legacy-preview/evidence.json` |
| 软件 RIP | 147 层诊断输出成功；0/30/90 层 S 占用与新切片原样零差异 | `output/tiffview/rip.log`、`deployed-preview/evidence.json` |
| S2 本地描述符 Gate | 2 正向、7 负向 PASS，不等于物理生产放行 | `output/tiffview/s2-contract/stage14f04_s2_gate.json` |
| 部署 | DeployOnly 退出 0，宿主/RIP 自检通过，31 场景资源通过，5 程序文件哈希一致 | `output/tiffview/deploy.log`、详细报告第 7.4 节 |
| 提交前检查 | 源文件规模门禁 PASS，61 项既有警告；diff 检查通过 | 本轮执行记录，规模比较基线为 `0008f33` |

`output/` 中的证据是本机验证产物，不纳入本次 Git 提交；测试源码与本总结中的事实可供后续追溯。真实派生包位于 `output/tiffview/ND002/package`，原包为 `runtime/slicesoft/Release/output/h260907160338160/package`，没有覆盖原包。

**ND002 全层验证是对已切好数据经共享生产 Writer 重编码，不是重新导入并完整重切。** 该方法隔离了行序变化；真实软件 SPI/RIP 验证与生产路径 fixture 回归分别提供其他证据，不混称为完整实模重切验收。

## 4. 提交拆分

分支：`product/packaged-slicer`。本轮提交前 HEAD 为 `0008f33`。

| 提交 | 任务归属 | 内容 |
| --- | --- | --- |
| `47ed7f3` | TV-00/01/02 | `fix(tiffview): 【朝向统一】同步生产TIFF行序与新旧包读取预览`，包含受控决策与上下游代码 |
| `6a5c48c` | TV-01/03 | `test(tiffview): 【朝向回归】补齐独立原始行断言与真实包重编码工具`，包含 CMake 与测试 |
| 本文所在提交 | TV-04 | `docs(tiffview): 【专项收口】整理朝向统一总结与提交验证记录`，包含任务卡、详细报告、总结与双索引 |

Writer、Reader、manifest 与预览绑定作为同一修复提交，避免拆出只能写新文件却不能正确读取的中间状态。三笔提交按顺序依赖，不推送远端。

没有纳入本次提交：`model/obj/multi-material/gubao05/gubao05-dingwei.mtl`、`gubao05-dingwei.obj`、`model/obj/multi-material/xx04/` 与 `analysis/`。这些工作树变更保留，不回退、不覆盖。

本轮没有再次编译或部署，已部署程序仍是上一轮验证版本：

```text
0.2.423-dev+0008f335e2d1.dirty.release.msvc-x64-md.x64-windows.tiff-libtiff.openvdb-off
```

该程序包含此次已验证修复；版本字符串记录的是当时构建来源，不能因提交完成而宣称二进制已带上新提交号。

## 5. 复验入口与边界

关键行序测试可在正确的构建目录单独复验；必须先确认构建成功，再执行 CTest：

```powershell
cmake --build build-slicesoft/main --config Release --target tiff_row_layout_unit_tests
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R '^tiff_row_layout_unit_tests$'
git diff --check
```

上述为后续复验入口，不代表本轮重新构建。完整的 23 项定向回归名单与结果保留在 `output/tiffview/ctest-precommit.log`；不能将其描述为全仓回归。

软件范围内本专项无剩余开发项。以下没有完成，也不计为 PASS：物理打印、目标设备方向与外部生产验收、所有彩色 UV 资产矩阵、外部图像编辑器人工打开验证。RIP 使用 `diagnostic_unvalidated`；W 受工艺变换，不按一对一保真通道判断。非等向 DPI 的物理/像素宽高比差异不是镜像，本次没有修改采样密度来掩盖比例差异。

## 6. 文档导航

- [任务清单](../../codex_task/current/TASKS_TIFFVIEW_生产文件与场景朝向统一.md)：TV-00..04 状态、日期和实际验证真源。
- [技术裁决](../DOC/DOC_DECISION_TIFFVIEW_生产TIFF行序与旧包兼容.md)：新旧行序、错误处理和同步更新边界。
- [详细排查报告](REPORT_TIFFVIEW_ND002_生产TIFF与预览朝向差异排查.md)：历史排查与当前实现分开记录，含资产哈希及逐层样本。
- [正式文档索引](../README.md) 与 [任务文档索引](../../codex_task/README.md)：均已增加本专项入口。

修订记录：2026-09-08 首次收口，记录实现、测试、部署证据与本轮提交前复验；不迁移或删除原决策、任务及排查文件。
