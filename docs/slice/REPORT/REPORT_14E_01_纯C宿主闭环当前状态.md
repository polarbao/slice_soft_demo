# REPORT_14E-01 纯 C 宿主闭环当前状态

> 状态：COMPLETE / M-MVP PASS
> 日期：2026-08-07
> 下一任务：14E-02 Qt 参考宿主与 `ModuleClient`

## 1. 任务目标

14E-01 用一个不链接 `slicer_core`、`slicer_base`、`slicer_engine`、
`slicer_module` 或 Qt 的纯 C 控制台宿主，模拟打印软件通过公开 DLL ABI 集成切片能力包。

宿主只在运行时加载 `slicer_module.dll`，解析冻结的 11 个 `pm_*` 导出，并完成：

```text
模块版本/自述/自检
  -> 未知能力 fail-closed
  -> model.import
  -> scene.apply_operation
  -> slice.rgbwsv（Worker）
  -> package.verify
  -> model.release
```

## 2. 实现内容

| 模块 | 交付物 | 说明 |
|---|---|---|
| 纯 C 宿主 | `apps/slicer_host_sim/` | C11；运行时 `LoadLibraryW/GetProcAddress`；无 Qt、无内部 C++ 头文件 |
| ABI 装载 | `HostModuleApi.*` | 精确解析 SPI v1 的 11 个导出；统一实现缓冲三态读取 |
| 请求构建 | `HostRequestBuilder.*` | 构建单模型 scene、有效 Profile、自哈希及 untextured OBJ 资源身份 |
| JSON 边界 | `JsonText.*` | 只承载宿主所需的结构化读取、转义和格式化，不引入内部 JSON 实现 |
| 路径边界 | `HostPath.*` | UTF-16/UTF-8 转换与输出目录递归创建 |
| 自动门禁 | `tests/stage14e_01/ValidatePureCHost.py` | 禁止内部模块/Qt include 和链接，检查 11 个运行时导出入口 |
| 构建/测试 | `CMakeLists.txt` | 新增 `slicer_host_sim` 和两个 Stage 14E-01 CTest |

所有新增 C/C++ 源文件均不超过 500 行，未进入 source-size allowlist。

## 3. 关键合同处理

1. Profile hash 按项目 `Json::dump(0)` 的规范化结果计算，而不是对请求原文直接求哈希。
2. 模型导入与 Worker 重载使用一致的 `autoOrient` 语义，避免 scene 几何边界漂移。
3. `sourceTransformIdentity` 使用模型绝对路径，满足 Legacy 实例来源身份校验。
4. 生产 scene 使用导入模型的 source hash、资源身份和 Commit 返回的权威有效边界。
5. 输出固定保持 `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`；本任务未修改 TIFF 协议。
6. 未知能力必须由模块返回稳定 `PM-SLICER-*` 错误，宿主不做假成功或内部 fallback。

## 4. 验证结果

### 4.1 Debug

```text
cmake --build build --config Debug --target slicer_host_sim --parallel 4
ctest --test-dir build -C Debug -R "stage14e01" --output-on-failure

2/2 PASS
```

### 4.2 Release

```text
cmake --build build --config Release --target slicer_host_sim --parallel 4
ctest --test-dir build -C Release -R "stage14e01" --output-on-failure

2/2 PASS
```

### 4.3 合同与源码门禁

```text
ValidateCapabilityDtos.py       PASS（15 capabilities）
ValidateThreeLaneContract.py    PASS
ValidatePureCHost.py            PASS
ValidateSourceSizeGuard.py      PASS（仅既有 G4/G5 warning）
git diff --check                PASS
```

Debug/Release 均生成 3 层 `p0.rgbwsv.2` Package，并由公开 `package.verify` 能力验证通过。

## 5. 阶段结论

```text
M-MVP-CANDIDATE = PASS（既有 14C-06 + 14D-05）
14E-01           = PASS
M-MVP            = PASS
14E-02           = UNLOCKED / PREPARED
```

14E-01 证明打印侧可以不链接切片内部库，仅凭公开 C ABI 完成最小生产闭环。
它不代表 Qt 参考宿主、双视图纹理交互或 14E 全阶段已经完成；这些内容从 14E-02 起继续实现。

## 6. 冻结边界

- SPI 仍为 v1，导出仍精确为 11 个，能力仍为 15 项。
- 未修改现有 `slicer_debug_ui`，未把宿主代码并入切片核心。
- 未新增 TIFF 通道、未修改位深、极性、通道顺序或 Package schema。
- 未把 Worker 重能力改回进程内执行。
- 14A-03 与 14A-04-R1 的打印侧书面 ACK 仍独立跟踪，不影响本次切片侧 M-MVP 证据。
