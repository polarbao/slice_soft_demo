# REPORT TIFFVIEW ND002 生产 TIFF 与预览朝向差异排查

> 初次排查：2026-09-07；修复：2026-09-08。
> 当前状态：COMPLETE / LOCAL_GATES_PASS / RELEASE_DEPLOYED。
> 第 1～6 节是旧包只读排查证据与当时边界，第 7 节是用户授权后的当前实现。
> 不覆盖源模型或原生产包，不宣称物理打印已验证。

收口阅读入口：[生产文件与场景朝向统一收口总结](REPORT_TIFFVIEW_生产文件与场景朝向统一收口总结.md)。本报告保留原始排查过程，第 8 节记录提交状态，避免将第 1～6 节的旧结论当作现行规则。

## 1. 问题与资产

- 输入：`C:/Users/admin/Downloads/黄晨晨ND002.obj`。
- 当前源文件 SHA256 为 `3202e6f88806afbaf8a32b5b477f3c802f347f1979ff756f76db240c56e5733e`，与场景报告的 sourceHash 一致。
- 包：`runtime/slicesoft/Release/output/h260907160338160/package`。
- 用户确认打开的是 `layers/` 切片 TIFF，而不是 RIP TIFF。
- 截图：工作区上排 6 个、下排 4 个；切片图片上排 4 个、下排 6 个，左右次序保留。
- `multimodel_scene_report.json` 记录实例 Z 旋转 90 度，X/Y 旋转均 0，mirrorX/mirrorY 均 false。
- 输出：147 层，4350×1375 px，635×600 DPI，层厚 0.038 mm，RGBWSV/uint8/PackBits。
- 此包 RGB/V 均无打印像素，主要是 W/S；软件中的伪彩色或外部查看器的附加通道显示，不等于物理 RGB 颜色。

## 2. 已证实根因

生产栅格使用：

```text
worldY(row) = originY + (row + 0.5) * pixelSizeY
```

即第 0 行是最小 Y，行号增大沿场景 +Y。典型图像显示器第 0 行在屏幕顶部，行号增大向下。
工作区俯视则是 +Y 向上。三者当前没有同一显示行原点：

| 表达 | 顶部对应的场景位置 | 实际处理 |
|---|---|---|
| 工作区俯视 | 最大 Y | 场景坐标投影 |
| 软件结果 PNG/BMP/PPM | 最大 Y | 读取 TIFF 后垂直翻转显示行 |
| 外部直接打开生产 TIFF | 最小 Y | 缺少 Orientation 标签，LibTIFF 默认值为 1/top-left |

实现证据：

- `src/slicer_core/slicer.cpp` 的 `grid.origin_y_mm` 及 `y_mm/yMm` 正向采样公式。
- `src/slicer_core/api/implementation/PackageQueryFacadePreview.cpp` 合成后统一调用 `OrientPreviewPositiveYUp`。
- `src/slicer_core/api/implementation/PackageQueryFacadePreviewEncoding.cpp` 交换上下行，不交换列。
- `src/slicer_core/output/tiff/LibTiffWriter.cpp` 不写 Orientation 标签；写入保持输入行序。
- `docs/slice/REPORT/REPORT_HOSTFLOW_H_F_07_连续切片与结果朝向统一修复.md` 是这项预览转换的历史依据：明确只改预览、不改生产 TIFF。

因此，这是**生产数据行序与人眼俯视表达不一致**，不是 Z90 额外施加 mirrorX/mirrorY，
也不是 FRAME 新增了镜像。本次旋转让原本不容易察觉的上下不对称更显眼。
仅凭显示差异不能断定实际打印已经镜像；打印端如何把行号映射到机械 Y 尚未获得实机证据。

## 3. 当前包实测

诊断脚本及派生查看图位于 `output/mirror_audit/`，不属于生产包。
脚本通过已部署 `tiff.dll` 只读解码，并通过实际 `slicer_module.dll` 的冻结 SPI 调用
`package.render_layer_preview`，以全尺寸单通道 PNG 做对照；不自行替代模块预览算法。

### 3.1 标签检查

| 范围 | 文件数 | Orientation | 其他事实 |
|---|---:|---|---|
| `layers/` | 147 | 全部未写，默认值 1 | RGBWSV，6 通道，8-bit，PackBits |
| `rip_diagnostic/` | 147 | 全部显式写 1 | CMYKWSV，7 通道，8-bit，LZW |

### 3.2 TIFF 占用区域与实际结果预览逐像素对照

以下层号均为 0-based；数值是二值打印区域的差异像素数。零差异不代表两种伪彩色编码字节一致。

| 层 | 通道 | 原样 | 仅上下翻转 | 仅左右翻转 | 上下左右翻转 |
|---:|---|---:|---:|---:|---:|
| 0 | W | 4710 | 0 | 4710 | 4710 |
| 0 | S | 1144910 | 0 | 1000818 | 1181078 |
| 30 | W | 109514 | 0 | 106618 | 109150 |
| 30 | S | 909546 | 0 | 825030 | 799534 |
| 90 | W | 24472 | 0 | 24540 | 24476 |
| 90 | S | 102022 | 0 | 101858 | 99838 |
| 146 | W | 40 | 0 | 40 | 40 |

层 146 的 S 为空，未用它做方向判别。7 组非空对照全部只有上下翻转后零差异，
无需额外左右翻转或转置。完整数值见 `output/mirror_audit/evidence.json`。

抽查 RIP 第 0/30/90 层 S 通道与原始切片 S 占用区域原样零差异，未发现 RIP 又额外翻转 S。
W 不可按同样方法当作保真通道：抽查输出占用与输入不同，受 RIP 材料处理影响；
本轮不据此判断 W 的空间镜像。该目录状态为 `diagnostic_unvalidated`，不可视为 S2/实机放行证据。

## 4. 其他镜像及外观风险

| 项目 | 当前结论 | 证据边界 |
|---|---|---|
| 仅 Z90 有问题 | 否 | 行原点差异不以旋转角度为条件，0/180/270 度同样有显示差异风险；本轮没有对真实资产重切四个角度 |
| 单模型/Scene | 共同风险 | 都进入共享 TIFF 行序与 PackageQuery 显示转换，不是当前场景独有 |
| RGBWSVT 的 T 通道 | 共同风险 | 七通道分支与六通道分支在合成后共用同一个上下翻转；当前真实包没有 T，不宣称实测 T 镜像 |
| 额外左右镜像/转置 | 本包抽查未发现 | mirrorX/mirrorY 为 false；仅上下翻转即可与模块预览零差异 |
| 用户显式 X/Y 镜像 | 合法变换，不可擅自取消 | 需区分源模型变换与图像显示行序 |
| 纹理 flipV | 独立的 UV 坐标转换 | 本包 RGB 全空，不能以本包证明所有彩色贴图方向均无问题 |
| 从模型底面观察 | 会改变人眼左右判读 | 应以工作区 +Z 俯视作比较，不用背面视角证明 TIFF 镜像 |
| 非等向 DPI | 另一个比例差异，非镜像 | 本包像素纵横比约 3.164，毫米纵横比约 2.989；按像素显示与按物理尺寸显示会有约 5.83% 的比例差异，不应靠旋转修正 |

## 5. 原排查阶段的决策边界

2026-09-07 排查结束时只完成根因确认，尚未切换生产行序。2026-09-08 用户已明确授权
统一生产文件与预览，并同步修改上下游软件，物理打印不作为本次前置。现行裁决见
`docs/slice/DOC/DOC_DECISION_TIFFVIEW_生产TIFF行序与旧包兼容.md`。

### 5.1 低风险查看方案

保留生产 TIFF 不变，独立导出 +Y 向上的查看图（PNG 或查看专用 TIFF），
按毫米纵横比显示，明确不可作为生产输入；可同时提供原始行序与工作区朝向对照。
本轮已利用现有模块接口生成若干正确朝向的 PNG 供判读，但未新增 GUI 导出功能。
该方案能解决外部目视比对，不改变原始生产 TIFF 本身的默认显示。

### 5.2 当时提出的生产文件统一方案

若要求直接打开 `layers/*.tiff` 就必须与工作区相同，需要：

1. 冻结新旧行原点、+X/+Y、Orientation 标签、像素中心及机械方向的对应关系；旧包不可被悄悄重写。
2. 明确是兼容的新输出模式还是新合同版本，不在 `p0.rgbwsv.2` 名下静默改变旧含义。
3. 同步 Writer、Reader、Scene 栅格位置解释、六/七通道、RIP 输入与输出、预览缓存和元数据。
4. 禁止单独把模型 mirrorY 当修复；反转 Writer 行后，必须同步 Reader 的规范化规则，不能让预览对未经规范化的新文件再翻转。
5. 不能仅设置 Orientation=4 就宣告解决：扫描行 Reader/RIP 可能不按图像查看器方式解释标签，需逐链路证明。
6. 用非对称角标/文字 fixture 覆盖 Z0/90/180/270、单/双轴显式镜像、等/非等向 DPI、
   stripped/tiled、none/PackBits、RGBWSV/RGBWSVT、BMP/PNG/PPM、RIP。明确断言存储索引与世界坐标，不只比较两个共用 helper。
7. 物理打印只能由外部回签和实机确认；按 2026-09-08 用户裁决，它不阻塞本次软件统一，不计为已验证。

## 6. 验证记录

- 真实包只读标签及模块预览比对：已完成，7 组非空 W/S 上下翻转后零差异。
- 用户两张截图与上述 Y 行原点差异一致；本轮未操纵用户 UI 或外部图像查看器。
- 相关 Release 构建退出 0；随后 5/5 CTest PASS（0.64 s）：
  `package_query_facade_14b02_unit_tests`、`tiff_layer_source_unit_tests`、
  `material_preview_composer_unit_tests`、`matvol_rgbwsvt_tiff_io_tests`、`model_transform_unit_tests`。
  这些测试覆盖既有合同，并不表示尚未实现的“生产 TIFF 与屏幕朝向统一”需求已经完成。
- 构建触发 CMake 再生成，并有既有 `.git/refs` 依赖路径 MSB8064 告警；没有依赖升级或以陈旧二进制冒充构建结果。
- 未修改生产代码、用户源模型或原生产 TIFF；未提交 Git、未推送。

## 7. 2026-09-08 当前修复

### 7.1 实现

- 共享生产 Writer 写出实际 maxY-first 行序，显式 Orientation=1；并非仅修改查看器标签。
- `ImageDescription` 与 `manifest.tiff.rowOrder` 自描述新约定。旧文件仍按旧约定读取，未知/不一致的标记拒绝。
- Native Reader 的输出保持 minY-first，现有结果页转换后与新 TIFF 同向；Legacy/Global/Scene、RGBWSV/RGBWSVT、显式 handwritten 均覆盖。
- 诊断 PNG/PPM 同步改为 +Y 向上；不改变源模型旋转、显式镜像、材料通道、像素采样或定位素材。
- 只使用一个条带/瓦片的可复用缓冲；不额外创建整栈。不等向 DPI 的像素/物理宽高比差异没有当作镜像修改。

### 7.2 已完成验证

- 两批 Release 构建退出 0；新测试采用 `/W4 /WX`。23/23 定向 CTest PASS，12.78 秒，详见 `output/tiffview/ctest.log`。
- 独立 LibTIFF 原始行 oracle：24 个六/七通道、条带/瓦片、none/PackBits、新旧行序、Writer 后端组合；16 个方向/显式镜像输入组合；六/七通道生产 Writer 接线；元数据错误及 manifest/层冲突拒绝；诊断 PPM 行序。
- ND002 证据包：`output/tiffview/ND002/package`。使用原包计算结果，经实际共享生产 Writer 重新编码；**不是重新导入模型或重新计算切片**。147 层均严格校验通过，内部材料字节零变化。
- 独立 raw 读取新旧包 147 层：新文件每一个样本都等于旧文件对应上下反转位置的样本，差异全部为 0。尺寸 4350×1375、DPI 635×600、层高 .038 mm、147 层及所有材料数值不变。
- 实际 `slicer_module.dll` SPI 全尺寸预览：0/30/90/146 层的 7 组非空 W/S，**新 TIFF 原样占用与预览差异全部为 0**，不再需要额外上下翻转。旧包通过新 Reader 的同 7 组预览仍与原预览一致，没有反向回归。
- 证据：`output/tiffview/new-preview/evidence.json`、`output/tiffview/legacy-preview/evidence.json`。已目视核查第 30 层 W 原始行图与实际模块 PNG，均为上排 6 个、下排 4 个；颜色表达不同不属于位置差异。
- S2 软件描述符 Gate：2 正向 + 7 负向 PASS，`output/tiffview/s2-contract/stage14f04_s2_gate.json`。首次调用因 bundled PowerShell 路径不含 powershell.exe 失败，改用系统 Windows PowerShell 后通过，未修改测试脚本。
- 源文件规模门禁相对 HEAD PASS，61 项既有警告；`git diff --check` PASS。CMake 仍有既有 `.git/refs` MSB8064 警告。

### 7.3 验证边界

原用户包未覆盖；派生包不复制旧 RIP，旧源数据保持可追溯。ND002 全层重编码验证与生产路径 fixture 切片回归分别成立，不把前者描述为完整实模重切。
未执行外部图像编辑器人工打开、物理打印或所有 UV 资产矩阵。

### 7.4 软件 RIP 与部署收口

- 真实已打包 RIP 模块处理派生包 147 层成功：`RIPFLOW_JOB_SELF_TEST_PASS outcome=diagnostic code=RIP_DIAGNOSTIC_SAVED`，114938 ms。保留诊断模式，与原任务一致，不宣称 S2 生产发布或物理打印通过。
- 输出 TIFF 均为 Orientation=1；0/30/90 层 S 占用与对应新切片 TIFF 原样零差异，不需另加镜像。W 仍受 RIP 工艺变换，不拿它做一对一空间保真判断。日志中的 ExtraSamples 告警来自外置 RIP 生成 TIFF，既有规范化/校验流程完成，未当作实机证据。
- `PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly` 退出 0，宿主 `STAGE14E02_SELF_TEST_PASS spi=1 calls=6`、RIP 模块自检通过，31 个场景资源验证通过。
- 目录移动遇占用，脚本使用既有“仅同步程序文件并保留 output”的回退；未关闭用户程序，原包 manifest SHA256 与修复前证据一致。
- Runtime 的 `slicer_module.dll`、`slicer_worker.exe`、`slicer_cli.exe`、`rip_reader_test.exe`、`slicer_ui_host_sim.exe` 均与刚构建的对应文件 SHA256 一致。CLI 版本 `0.2.423-dev+0008f335e2d1.dirty.release.msvc-x64-md.x64-windows.tiff-libtiff.openvdb-off`。
- 部署后再次通过 runtime 模块 SPI 验证 7 组 W/S，均与新 TIFF 原样零差异：`output/tiffview/deployed-preview/evidence.json`。构建、部署、RIP 日志分别为 `output/tiffview/build-final.log`、`deploy.log`、`rip.log`。
- 新切片任务直接使用新行序；历史任务不自动重写。供核对的修正副本位于 `output/tiffview/ND002/package`，其中保留 `orientationAudit.reSliced=false` 来源说明。
- 上述修复与部署阶段尚未提交；后续提交状态见第 8 节。无关 model/analysis 内容保持原状。

## 8. 提交与文档收口

- 2026-09-08 用户授权后，修复提交 `47ed7f3`、回归测试提交 `6a5c48c`；任务清单、详细报告、总结及索引在独立的 `docs(tiffview): 【专项收口】整理朝向统一总结与提交验证记录` 提交中收口。未推送。
- 本轮没有继续修改生产代码，没有重编译、重部署或重复实模重编码；复用第 7 节已成功构建的 Release 程序，重新执行相同的 23 项定向 CTest，23/23 PASS，12.01 秒。日志：`output/tiffview/ctest-precommit.log`。
- 本轮源文件规模门禁相对提交前 `HEAD=0008f33` PASS（61 项既有警告），`git diff --check` 与暂存检查通过。没有把定向回归描述为全仓回归。
- 无关模型素材与 `analysis/` 继续保留在工作树，不包含在本专项提交中；原生产包、源模型及既有验证产物未覆盖。部署版本仍是第 7.4 节所记构建版本，不伪称已包含新 Git 提交号。
