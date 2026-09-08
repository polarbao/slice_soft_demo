# UNIPATH 中文路径跨代码页兼容修复

日期：2026-09-08。授权：用户要求中文路径、中文模型名称正常导入及切片；提供 `C:/Users/admin/Downloads/黄晨晨ND002.obj`，说明本机启用系统 UTF-8 后正常，而其他电脑失败。

## Implementation Plan

### Problem Type

Windows 文件路径编码边界缺陷，不是模型几何或 UTF-8 编译选项缺失。

### Layer(s) Involved

UTF-8 SPI JSON、模型导入与材质引用、场景持久化、Worker 请求、配置及切片报告。Qt 保持在宿主，核心不引入 Qt。

### Official Documents

`contracts/print_module_spi.h` 明确输入/输出 JSON 为 UTF-8；Stage 14 DEV 与 Worker 文件合同沿用该约定。修复兑现既有合同，不改 schema、ABI、通道语义或采样策略。

### Historical Documents

本机过去可读取中文模型只是 UTF-8 系统代码页下的证据，不等于跨电脑验证。TIFFVIEW、FRAME 与 MEMFLOW 本轮不重新开工。

### AI Workspace Evidence

分支 `product/packaged-slicer`；任务开始时 `gubao05-dingwei.mtl/.obj`、`analysis/` 与 `model/obj/multi-material/xx04/` 有无关修改，保持原状。

### Current Code Reality

`ModelCapabilityAdapter` 将 UTF-8 `modelPath` 字符串直接赋给 `std::filesystem::path`；`MultiModelScene` 读写路径使用窄字符串隐式转换；模型显示名及资源哈希使用 `string()/generic_string()`。这些转换依赖 Windows ACP，与 SPI 的 UTF-8 合同不等价。OBJ 的 mtllib、MTL 的 map_Kd 也有相同隐式转换。

### Current State

已使用未修改的部署 DLL 在 ACP936 独立进程复现 `PM-SLICER-INPUT-0001 / model file was not found`。显式 UTF-8/native 修复已通过 ACP936/1252/65001 的宿主与 Worker 矩阵，包含中文 TEMP/TMP；原 Release 已部署更新，未修改用户系统区域设置。

### Target State

边界文本明确 UTF-8，文件系统内部明确 native path；往返不经 ACP。新测试覆盖中文目录/文件名、材质引用、场景快照及实际 Worker 切片，不仅测 helper 或本机默认代码页。

### Historical State

ASCII 路径行为保持不变；系统 UTF-8 下生成的路径与哈希不变。非 UTF-8 机器过去用本地编码生成的非 ASCII 路径哈希可能不同，不能伪装成原哈希；旧的乱码路径不启发式猜测或静默重写。

### Pending Confirmation

无。用户已提供代表模型并明确要求修复；不同电脑的实机复验仍需区分于本机隔离代码页测试。

### Risk Points

只修导入却遗漏 Worker、材质和场景序列化；只设置 UTF-8 manifest 掩盖 DLL 被其他宿主加载的问题；错误编码造成材质丢失、纹理降级或资源哈希不一致；对旧 ANSI OBJ/MTL 引用的兼容边界需要明确，不修改用户素材。

### Files To Change

新增小型路径转换 helper 与独立代码页测试；仅修复导入到切片相关的 UTF-8/native 路径边界、所需配置与报告路径，不全仓机械替换文本接口，不引入新依赖。

### Verification Plan

非 UTF-8 进程旧实现失败证据；中文目录/模型/MTL/贴图 fixture 导入和场景往返；实际 Worker 生产写包及严格校验；ND002 真实资产验证；相关 Release 定向回归；源码规模、差异检查。构建通过后才能执行对应测试。部署前保留用户进程及历史 output，不把本地软件验证称为打印实机验收。

## 任务清单

| 任务 | 状态 | 完成日期 | 实际验证 |
| --- | --- | --- | --- |
| UP-00 根因与非 UTF-8 复现 | COMPLETE | 2026-09-08 | 旧部署 DLL + ACP936 复现失败，证据 output/unipath/baseline-acp936.log |
| UP-01 导入与场景路径统一 | COMPLETE | 2026-09-08 | 三代码页中文 OBJ/MTL/贴图、STL/3MF、场景往返 PASS；ND002 每次导入 102396 面 |
| UP-02 Worker 切片与材质引用贯通 | COMPLETE | 2026-09-08 | 三代码页中文 TEMP/输出/包名/预览与 25 层写包校验 PASS，全部 TIFF 文件摘要相同；预检/报告/预览索引遗漏已补修 |
| UP-03 回归、真实资产与交付文档 | COMPLETE | 2026-09-08 | 新增 CTest 1/1 PASS；既有定向回归 20/21 PASS，1 个已记录既有失败；规模门禁 PASS；原 Release 部署和自检通过，产物哈希一致；总结已落地 |

## 修订记录

- 2026-09-08：建立修复范围和路径合同，采用原生宽路径与 UTF-8 显式转换，不要求用户开启系统 UTF-8。Windows 11 测试进程 manifest 的代码页设置依据 [Microsoft application manifests](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests#activeCodePage)，仅用于暴露 ACP 依赖，不代替生产修复。
- 2026-09-08：按用户追加授权采用双 Agent 审计。OBJ/MTL 引用优先 UTF-8；仅无效 UTF-8 字节在 Windows 保留本机 ANSI 历史兼容，不对有效 UTF-8 的缺失文件猜测其他编码。SPI/JSON 不允许 ANSI 回退。测试使用 fail_fast 排除贴图静默降级，并比较各代码页实际 TIFF 文件 SHA256；真实 ND002 目前只完成导入验证，不等同于该真实模型整包切片验收。
- 2026-09-08：UP-00..03 收口，完整证据与未覆盖项见 [修复总结](../../slice/REPORT/REPORT_UNIPATH_中文路径跨代码页兼容修复总结.md)。不同电脑人工验收和物理打印未计为 PASS，未改动既有失败断言。
- 2026-09-08：按用户授权拆分提交：UP-01 导入与场景 `d85c619`、UP-02 切片链路 `42507df`、UP-03 自动化回归 `5e06dd7`；本记录与总结、索引作为独立文档收口提交。各组暂存差异检查通过，未推送；无关模型、analysis 与测试 cache 未纳入。构建和运行证据来自拆分前完整修复工作树，本轮仅拆分提交并同步文档，未重新构建或部署。
