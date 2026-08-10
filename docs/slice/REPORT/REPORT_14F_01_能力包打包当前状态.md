# REPORT_14F-01 能力包打包当前状态

> 状态：SLICER-SIDE COMPLETE / CLEAN-MACHINE EVIDENCE PENDING
> 日期：2026-08-10（R-B-02 依赖清单复测）
> 范围：生成并验证 `modules/slicer/` Release 能力包；不包含打印侧 M1/M2、目标 RIP 或实物联调

## 1. 交付结论

14F-01 已建立可重复执行的 Release 打包与本地隔离验证链路。最终运行时目录为：

```text
output/distribution/Release/modules/slicer/
```

该目录可独立交给打印侧宿主装载。它不包含 Qt 宿主、不包含调试 UI，也不依赖仓库内部
`src/` 源码。原 `SliceSoft:` Runtime 入口、14E 参考宿主入口与 14F 分发入口保持三组独立边界。

## 2. 包内容

```text
modules/slicer/
  slicer_module.dll
  slicer_worker.exe
  module.json
  msvcp140.dll
  vcruntime140.dll
  vcruntime140_1.dll
  runtime_dependencies.json
  checksums.sha256
  THIRD_PARTY_NOTICES.txt
  third_party_distribution_manifest.json
  licenses/
    miniz.txt
    libtiff.txt
    assimp.txt
    meshoptimizer.txt
```

打包脚本不按 `vcpkg.json` 盲目复制 DLL，而是对最终 `slicer_module.dll`、
`slicer_worker.exe` 及其本地依赖递归执行 PE import 审计。Windows 系统组件只登记、不复制；
MSVC Release Runtime 从当前 Visual Studio Redistributable 目录复制；无法解析的非系统 DLL 会使
打包 fail-closed。

当前默认手写 TIFF 后端未动态链接 LibTIFF，Assimp 也未进入 CMake 链接图，因此包内没有二者
的 DLL。R-B-02 的 meshoptimizer 采用静态 `x64-windows-static-md` 链接，因此同样不新增运行时
DLL；四项许可证均按 Stage 14 合规合同统一携带。

## 3. 新增入口

| VSCode 任务 | 作用 |
|---|---|
| `SliceSoft 14F: Package Slicer Module (Release)` | 构建并生成 `modules/slicer/` |
| `SliceSoft 14F: Validate Slicer Module Package (Release)` | 校验结构、哈希、依赖闭包、Worker 合同，并运行纯 C 宿主闭环 |

脚本入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/PackageSlicerModule.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/TestSlicerModulePackage.ps1
```

## 4. 已执行验证

| 验证项 | 结果 |
|---|:---:|
| Release `slicer_module` / `slicer_worker` / `slicer_host_sim` 构建 | PASS |
| `module.json` schema、SPI v1、DLL/Worker 名称与 15 项能力 | PASS |
| PE import 递归审计与 MSVC Runtime 本地闭包 | PASS |
| NOTICE、四份许可证与分发清单 | PASS |
| 全文件 SHA-256 校验 | PASS |
| 打包后 `slicer_worker --contract-info` | PASS |
| 限制 PATH 后由纯 C 宿主加载包内 DLL/Worker | PASS |
| import -> transform -> Worker slice -> RGBWSV package -> verify | PASS |

本地闭环输出：

```text
STAGE14F01_PACKAGE_PASS
STAGE14F01_PACKAGE_VALIDATION_PASS
HOSTFLOW_HA03_PASS sceneHandle=1 revision=3 layers=3
```

## 5. 未关闭边界

本轮没有在一台未安装开发工具、Qt、vcpkg 和 VC Runtime 的独立干净机上执行 D14-F-01，
因此不能把“干净机可装载”写成最终 PASS。当前状态是：

```text
本地隔离依赖闭包 = PASS
真实干净机装载   = NOT RUN
14F-02..05       = BLOCKED_BY_EXTERNAL_PRINT_AND_RIP_INTEGRATION
```

下一步应把本报告中的 `modules/slicer/` 原样复制到打印侧 M1 环境，先执行 manifest 校验、
能力枚举和 `pm_self_test`，取得外部 ACK 后进入 14F-02。不得把本地参考宿主证据冒充打印侧证据。
