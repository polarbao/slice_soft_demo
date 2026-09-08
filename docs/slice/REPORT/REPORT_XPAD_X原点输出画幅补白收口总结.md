# XPAD X 原点输出画幅补白收口总结

日期：2026-09-08。状态真源：[任务清单](../../codex_task/current/TASKS_XPAD_X原点输出画幅补白.md)。设计合同：[画幅补白设计](../DOC/DOC_DESIGN_XPAD_X原点输出画幅补白.md)。

## 第一阶段结论与边界（六通道独立测试版）

以下为 XPAD-00..03 历史交付记录；本轮用户已授权追加 T 支持及合入 product，最新结果见末尾追加记录，不沿用第一阶段的 T 拒绝/未合入结论。

XPAD-00..03 完成，交付范围为 RGBWSV Scene 软件实现、自动化验证和独立测试版。原先联合 Raster 取模型有效范围并集，未保留 X=0 到最左侧 Raster 的空白。现在在联合合成写包前扩展画幅，不移动模型、不重写旧 TIFF；TIFF、manifest、统计和原生结果预览共用扩展后的网格。

- UI：切片设置 → 输出画幅 → “补齐至 X=0”，默认关闭，可持久化。
- 开启时 Profile 增加 `output.scenePadToOriginX=true` 并更新 hash；关闭不输出该字段，旧配置缺省行为不变。
- 只补 X 左侧。Y/Z、自动定向/排版、模型变换、层数、DPI、主体数据不变；Xmin<=0 不裁剪、不移动，原有越界准入不变。
- 所有补列 RGBWSV 均为 255，即不打印，而非增加白墨。无新依赖、无 ABI/协议版本/通道语义修改。
- 本期仅 Scene 六通道（包含单实例 Scene）；RGBWSVT 的独立路由不支持，UI/核心显式拒绝该组合。独立单模型 CLI 不应用 scene 专用选项。
- 保留原像素相位，补列 N=ceil(oldOriginX/pitchX)。新 originX 可略小于 0，但不足一个像素；不能直接标成精确 0 而改变物理位置。实例保持旧整数 offset 后加 N，避免半像素舍入导致一列漂移。

## 真实资产验证

输入为 `model/obj/alg_suoguo/20260908-HuangChenC/` 全部十个 OBJ，源顶点 XY 并集 `(20.07984,1.506456)..(194.0495,59.67867)` mm。测试禁用定向、不做 XY 平移，并启用 Z 触底；使用 `golden_material_process_top2_fixture` 工艺配置，包含 RGB/W/V/支撑，不代表用户未指定的全部生产预设均已验收。

600 DPI、0.2 mm、23 层实际对照：

| 项目 | 关闭 | 开启 |
| --- | --- | --- |
| 宽 × 高（像素） | 4110 × 1375 | 4585 × 1375 |
| originX（mm） | 20.07984 | -0.0284933333333335 |
| originY / Z（mm） | 1.506456 / 0 | 1.506456 / 0 |
| TIFF 总字节 | 779882781 | 870014031 |

增加 475 列，所有 23 层新增字节均为 255，原主体逐行逐字节一致。严格 Reader、manifest/TIFF 尺寸、通道统计通过；统计仅增加预期空白量。额外未压缩数据为 90,131,250 字节，等于 `475*1375*23*6`，因此补白仍增加缓存、写盘及校验开销，但不扩大各模型的几何采样域。

证据目录：`output/xpad/248650171388800`。命令：

```powershell
build-slicesoft/main/Release/x_origin_padding_tests.exe --real 600 0.2
```

另一次 127 DPI/0.5 mm 全 10 层对照通过：宽 870→971，补 101 列，originX=-0.12016 mm；证据 `output/xpad/248188927916400`。

## 自动化与交付

Release 核心、宿主、CLI、Reader 和相关测试构建退出码 0。最终定向 CTest 9/10 PASS：XPAD、TIFF source、FRAME、multi-model composer/production service、宿主 hb05/hb08 单元及两项 UI smoke 均通过。日志 `output/xpad/regression.log`。

唯一失败 `scene_layer_adapters_unit_tests` 的 `legacy_adapter_applies_admitted_instance_transform`，信息为 `translation preserves local layer bytes and dimensions`；属于此前已记录失败，本轮未修复，不计为全绿。该程序其余子用例通过。

XPAD 用例覆盖默认关闭、负/零/分数 X、多种 DPI、单/多实例、半像素、空列、主体字节、统计、保留式/借用式/流式路径、溢出、非法 padding 证据、配置类型、T 组合拒绝。宿主测试覆盖勾选状态、有效 Profile/hash 和旧工作区缺字段恢复 false。

源码规模门禁通过，存在 60 项既有警告。`git diff --check` 通过。没有将无关模型、analysis、cache、团队文档修改混入本任务。

独立部署命令：

```powershell
& scripts/PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly -RuntimeDir runtime/slicesoft-xpad
```

部署退出码 0，RIP module package/test PASS；新宿主 `--self-test` 返回 `STAGE14E02_SELF_TEST_PASS spi=1 calls=6`。宿主、module、Worker、CLI 四个部署文件与构建文件 SHA256 均相同。测试版版本 `0.2.432-dev+97afc31d3745.dirty.release.msvc-x64-md.x64-windows.tiff-libtiff.openvdb-off`。

启动：`runtime/slicesoft-xpad/Release/slicer_ui_host_sim.exe`。原 `runtime/slicesoft/Release` 未覆盖。当前分支 `codex/x-origin-canvas-padding`，本次修改未提交、未推送、未合入生产分支。GUI 人工交互、外部打印软件和物理打印未验证。

## 追加 T 支持与耗时测量（2026-09-08）

T Scene 走既有 admitted Facade 路径，在最终七通道层合成之后补空列，几何 grid 和 T owner plan 均不扩大。逐层 TIFF/manifest、slice/material process 报告、覆盖率分母和诊断预览同步使用输出画幅；T 独占和 V/T 冲突规则保持不变。默认关闭，原有 T 单实例限制不扩大。

合成闭合盒已实测 600 DPI/0.2 mm，3 层，宽 48→546、补 498 列，T 打印像素总量 3384 不变。严格 Reader、七通道原主体字节、空列、统计和 PPM 预览主体/尺寸对照 PASS。证据 `output/xpad/t250456912414400`（该次为基础预览尺寸 Gate，后续最终 CTest 同时覆盖预览主体字节）。

### 六通道真实十模型耗时

命令 `x_origin_padding_tests.exe --bench 600 0.2`，同一 Release 二进制、同一工艺；第一对热身剔除，后三对交替顺序。测量为 Scene 生产服务 wall time，包含服务内加载、采样、合成、写盘及验证，不含 GUI 导入、对话框操作和测试器额外的全层比对。每对最终 TIFF 主体仍逐字节一致。

| 配对 | 关闭（秒） | 开启（秒） | 增量（秒） | 增幅 |
| --- | --- | --- | --- | --- |
| 1（先开后关） | 4.225 | 5.555 | 1.330 | 31.48% |
| 2（先关后开） | 4.060 | 4.709 | 0.648 | 15.97% |
| 3（先开后关） | 6.286 | 6.458 | 0.173 | 2.75% |

配对增量中位数约 **0.65 秒 / 16%**，实测范围 **0.17~1.33 秒**。这是本机本次 23 层样本，不是任意模型/层厚/机器的固定增幅。关闭样本本身也有 4.06~6.29 秒波动，不应把全部时间差归因为补白算法。额外负担主要来自画幅变大后的层数据搬移、写出、回读/校验；不重新采样补白区域。发布/验证相关计时每对增加约 134~198 ms，TIFF 写出差值受文件缓存影响，不能用单次值推断物理磁盘吞吐。

证据：`output/xpad/250463313987500/timings.json` 与 `r0off/r0on..r3off/r3on` 全部包。原宽 4110、补后 4585，文件数据增加约 11.56%，约 90.13 MB；文件增幅不等于整体耗时增幅。

### T 通道真实模型对照与耗时

命令 `x_origin_padding_tests.exe --bench-t`。使用 `model/obj/reality/finger_suoguo/03.obj`，600 DPI/0.2 mm、32 层，T 匹配浅桃色 `(255,220,198)`。单实例平移至左边界 21.07984 mm、Y=5 mm 并 Z 触底，是显式测试摆位，未改源 OBJ；不代表六通道十模型场景的原始坐标。原宽 555→1053，补 498 列；每对所有七通道主体、补列、strict Reader、slice/material 报告与预览均 PASS，T 打印像素总数恒 439353。

| 配对 | 关闭（秒） | 开启（秒） | 增量（秒） | 增幅 |
| --- | --- | --- | --- | --- |
| 1（先开后关） | 4.794 | 5.399 | 0.605 | 12.62% |
| 2（先关后开） | 4.820 | 5.304 | 0.484 | 10.03% |
| 3（先开后关） | 5.259 | 5.731 | 0.471 | 8.96% |

热身剔除后，配对增量中位数约 **0.48 秒 / 10%**，范围 **0.47~0.61 秒**。该 T 样本为覆盖预览同步而开启每层 RGB/T PPM 诊断图生成，计时包含此项；不能将其百分比直接套用到宿主默认只读原生 TIFF 预览，更不能拿这两个不同模型/工艺矩阵比较六/七通道引擎速度。

证据：`output/xpad/t250926422532200/timings.json` 和 `r0..r3` 四对完整包。若现有画幅已覆盖 X=0，则无需扩框，只执行常数级开关/边界检查，没有本报告的新增画幅数据搬移和写盘成本。

### 最终回归与运行时

全部相关 Release 目标构建退出码 0。最终 CTest **21/22 PASS**、14.18 秒；日志 `output/xpad/regression_t.log`。覆盖 XPAD、八项 T 核心/实包用例、T 双协议 PackageQuery、T 生产矩阵、T 宿主 Profile、FRAME、六通道场景生产/合成、TIFF source、宿主设置/持久化/重绑与两项 UI smoke。唯一失败仍为 `scene_layer_adapters_unit_tests::legacy_adapter_applies_admitted_instance_transform` 的既有平移用例，无新增失败。源码规模门禁 PASS（60 既有警告）；冻结大文件 slicer.cpp、SceneLayerComposer.cpp、MultiModelProductionService.cpp 均无行数净增长。

原软件在部署前重新打开，PID 5372；标准脚本拒绝覆盖，因此未终止进程或更新现用运行时。原 `runtime/slicesoft/Release/output` 部署前后均为 3689 文件、6751741933 字节。另用 `PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly -RuntimeDir runtime/slicesoft-xpad-t` 交付新目录，自检 `STAGE14E02_SELF_TEST_PASS spi=1 calls=6`，RIP module package/test PASS。这是提交前已验证源码构建，版本仍带 `97afc31d3745.dirty`，不是提交后的版本快照刷新；未冒充干净发布构建。

| 部署文件 | 与构建文件一致的 SHA256 |
| --- | --- |
| slicer_ui_host_sim.exe | 5C286FEFFD8BBC27A0F029BB2EFE32F3A3663D36556646C104A957E9799A04F1 |
| slicer_module.dll | 60EB024FB9B28EDA70BA53E35307724CB365888F8A9B29BA323EFC9D13B15D3B |
| slicer_worker.exe | AE6A6CA37AAB98A48A08E8CF676392A20B0540AA2D406D1971441AC9424A621B |
| slicer_cli.exe | B88A2AECBE894223F9D11364840471189F589EE6A29E38734D3225D5C1DE7009 |

启动 T 兼容版：`runtime/slicesoft-xpad-t/Release/slicer_ui_host_sim.exe`。GUI 人工交互和物理打印未验证。提交和合入按用户授权执行，最终 Git 记录在任务清单收口。
