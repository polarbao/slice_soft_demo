# UNIPATH 中文路径跨代码页兼容修复总结

日期：2026-09-08。范围：封装宿主的模型导入、场景保存/提交、Worker 切片与生产包读取预览。
任务状态真源：[UNIPATH 任务清单](../../codex_task/current/TASKS_UNIPATH_中文路径跨代码页兼容修复.md)。

## 结论与根因

本专项功能修复与本机隔离代码页验证完成，已更新 `runtime/slicesoft/Release`。未修改系统区域设置，不要求开启 Windows 的 UTF-8 测试选项。

SPI/JSON 的路径本来就是 UTF-8，但部分 C++ 边界把这些字节直接当作 Windows ANSI 字符串构造 `std::filesystem::path`；反向的 `string()/generic_string()` 又依赖进程代码页。因此本机 ACP65001 掩盖了错误：ACP936 会把路径错解为另一组字符，ACP1252 对中文路径反向转换还可能直接抛异常。问题不仅在首次打开，还分布于资源哈希、源身份比对、材质预检、工艺报告与 TIFF 预览索引。

未修改的原部署 DLL 在 ACP936 隔离进程实际复现 `PM-SLICER-INPUT-0001 / model file was not found`，即磁盘上确有中文模型但导入失败。证据：`output/unipath/baseline-acp936.log`。

## 实现边界

- `Utf8Path.h` 明确 UTF-8 文本与 native filesystem path 的双向转换；临时后缀直接拼接 native path，不先窄化整个路径。
- 导入、中文显示名、场景 JSON、源/资源身份、配置、Worker 请求和结果、材料报告及 TIFF 包/预览缓存键统一采用该规则。
- Windows Worker 使用 `wmain` 接收宽命令行，再转换成 UTF-8，避免中文用户名和 TEMP 路径在 CRT 生成窄 `argv` 时丢失。
- OBJ 的 `mtllib` 与 MTL 的 `map_Kd` 优先解读 UTF-8；仅无效 UTF-8 字节保留 Windows 本机 ANSI 历史兼容。有效 UTF-8 但文件不存在时，不猜测其他编码、不选择其他同名素材。SPI/JSON 禁止 ANSI 回退。
- 3MF 文件名传给现有 miniz 的 UTF-8 文件接口；Windows 下该库内部已使用宽文件打开，不新增依赖或 ZIP 实现。
- 同步修复生产前严格预检的路径/纹理资源键，以及显式 Global 路径的两处报告身份转换。几何、排版、朝向、工艺、RGBWSV 通道与 `p0.rgbwsv.2` 合同均未改变。

## 实际验证

### 跨代码页端到端矩阵

`tests/unit/unicode_paths/RunCodePageMatrix.ps1` 只对隔离副本注入 manifest，不修改生产程序 manifest。宿主与 Worker 同时固定代码页，并设置中文 TEMP/TMP。

| 进程 ACP | 中文 OBJ/MTL/贴图 | 中文 STL/3MF 名称 | 中文目录写包、校验、预览 | 真实 ND002 导入 |
| --- | --- | --- | --- | --- |
| 936 | PASS | PASS | PASS，25 层 | PASS，102396 面 |
| 1252 | PASS | PASS | PASS，25 层 | PASS，102396 面 |
| 65001 | PASS | PASS | PASS，25 层 | PASS，102396 面 |

测试设置 `texture.missingTexturePolicy=fail_fast`，不能靠纹理丢失后退化为单色通过。三个代码页的全部实际 TIFF 层文件逐文件 SHA256 拼接摘要完全相同：

```text
fefcbf9cf159761fd671f1c8f6f2544031fd359f9364122bfb61555a9a01ab81
```

证据：`output/unipath/codepages-33642a37051042528825af1091427021/{936,1252,65001}/test.log`。
真实资产：`C:/Users/admin/Downloads/黄晨晨ND002.obj`，SHA256 为 `3202e6f88806afbaf8a32b5b477f3c802f347f1979ff756f76db240c56e5733e`。原文件未修改。

复验入口（Windows 11，ManifestTool 按实际 SDK 路径填写）：

```powershell
cmake --build build-slicesoft/main --config Release --target unicode_path_contract_tests
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R '^unicode_path_contract_tests$'
& tests/unit/unicode_paths/RunCodePageMatrix.ps1 `
  -ManifestTool 'C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/mt.exe' `
  -RealModel 'C:/Users/admin/Downloads/黄晨晨ND002.obj'
```

### 回归与部署

- Release 定向构建通过；新增 Unicode CTest 1/1 PASS。
- 既有定向回归 20/21 PASS。唯一失败是 `scene_layer_adapters_unit_tests::legacy_adapter_applies_admitted_instance_transform`，断言 `translation preserves local layer bytes and dimensions`。与 PRESET 任务清单 §13.2、FRAME 任务清单已记录的既有失败一致；未修改/放宽该断言，不宣称全仓回归全绿。日志：`output/unipath/regression-21.log`。
- 通过项覆盖导入/法线、MTL、纹理取色、场景生命周期、场景/切片 Facade、Worker 合同/执行/物化/预检、生产写包和 TIFF 行序/等价/读取。
- 源码规模门禁 PASS，61 条既有警告；`git diff --check` PASS。
- 用户确认关闭软件后，执行 `PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly` 成功。目录仍被其他句柄占用，脚本采用既有原位发布机制，保留历史 `output`。
- 宿主自检 `STAGE14E02_SELF_TEST_PASS spi=1 calls=6`、RIP 模块自检通过。部署后的 UI、CLI、Worker、模块 DLL 均与构建产物 SHA256 一致；CLI 报告 `0.2.427-dev`。
- 修复和测试已按任务拆分提交，文档独立收口，未推送。工作过程中出现的其他 MEMFLOW 文档提交与无关模型修改未回退、未改写。

## 提交记录

| 任务 | 提交 | 范围 |
| --- | --- | --- |
| UP-01 | `d85c619` | UTF-8/native helper、模型导入、材质引用及场景资源路径 |
| UP-02 | `42507df` | Worker 命令行、预检、生产写包、报告及结果预览 |
| UP-03 自动化 | `5e06dd7` | CMake 测试入口、中文 fixture 与三代码页矩阵 |
| UP-03 文档 | 本文所在文档提交 | 任务清单、总结与两级索引收口 |

2026-09-08 按用户授权执行上述拆分，各组提交前暂存差异检查通过。仅纳入本专项修改，不包含 `gubao05-dingwei.mtl/.obj`、`model/obj/multi-material/xx04/`、`analysis/` 或测试生成的 `cache/`。

验证与部署使用拆分前的完整修复工作树，构建来源为 `af6b33c29366` 加本专项未提交修复，CLI 完整版本为 `0.2.427-dev+af6b33c29366.dirty.release.msvc-x64-md.x64-windows.tiff-libtiff.openvdb-off`。拆分提交未改变已验证代码，也未为更新版本号再次构建或部署；因此现有运行包仍保留该构建来源标识，不等同于提交后的 HEAD。

## 未覆盖项

真实 ND002 本轮验证的是导入；完整 25 层切片使用确定性中文资源 fixture，不把两者混称为真实 ND002 整包切片测试。不同电脑的人工操作、Windows 10 实机和物理打印尚未验证。其他电脑需要同步此次更新的完整运行包，不能继续使用旧 DLL/Worker。

本轮不是全仓所有工具的编码迁移：独立修复能力、CLI 的任意中文命令行参数以及任意跨机器 ANSI 编码的 OBJ/MTL 内容不在完成声明内；中文路径与素材文件内容编码是两类不同问题。也未调整 Windows MAX_PATH 配置或已有长路径处理策略。
