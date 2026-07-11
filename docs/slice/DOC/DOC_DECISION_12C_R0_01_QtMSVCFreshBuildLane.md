# DOC_DECISION_12C-R0-01 Qt/MSVC Fresh Build Lane

> 文档状态：Decision / Implemented
> 日期：2026-07-12
> 阶段：12C-R0-01

## 1. 问题与环境

fresh Qt UI 构建环境：

```text
Visual Studio 18 2026
MSVC compiler 19.51.36248.0
MSVC toolset 14.51.36231
Qt 5.15.2 msvc2019_64
CMake 4.3.1
OpenVDB OFF
```

未修复时，Qt 5.15.2 的 `qcompilerdetection.h` 把容器比较辅助宏展开为：

```text
stdext::make_checked_array_iterator
stdext::make_unchecked_array_iterator
```

MSVC 19.50 及以上已经移除这些扩展，导致 `qvector.h`、`qlist.h` 和 `qvarlengtharray.h` 报 `C3861/C2065`。

## 2. 候选路线比较

### A. 固定旧 MSVC 工具集

本机存在 14.44.35207，但 Visual Studio 18 生成器将平台工具集识别为 `v145`，CMake 拒绝 `v145,version=14.44.35207`。继续该路线需要额外安装并注册 Visual Studio 17 生成器，构建入口将依赖机器级安装状态。

结论：当前不采用。

### B. 项目内最小 compatibility shim

只对 `slicer_debug_ui` 生效：

```text
在 MSVC_VERSION >= 1950 时强制包含 Qt515MsvcCompatibility.h；
补回 Qt 5.15.2 需要的两个 stdext helper；
helper 返回原始 iterator，不修改 Qt 安装目录；
不传播到 slicer_core 公共 API；
为 UI target 启用 /MP，降低重复 Qt 头编译耗时。
```

结论：本阶段采用。

### C. 升级 Qt patch/LTS

升级能够从依赖源头移除兼容问题，但会影响 Qt DLL 部署、开发机安装、许可证审查、UI 全量回归和后续发布基线。

结论：作为长期路线保留；12C-R0-01 不升级依赖。

## 3. 实现

```text
apps/slicer_debug_ui/compat/Qt515MsvcCompatibility.h
apps/slicer_debug_ui/CMakeLists.txt
scripts/Configure12CQtUi.ps1
.vscode/tasks.json
.vscode/launch.json
```

PowerShell 入口：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Configure12CQtUi.ps1 -BuildDir build-12c-ui -Config Debug
```

VS Code 入口：

```text
Task: SliceSoft: Build 12C Fresh Qt UI
Launch: SliceSoft: Debug 12C Fresh Qt UI
```

脚本优先读取 `Qt5_DIR`，未设置时使用当前项目验证过的 Qt 5.15.2 默认安装位置。OpenVDB 明确保持 OFF。

## 4. 验证证据

最终从不存在的 `build-12c-ui` 目录执行：

```text
fresh configure：PASS
fresh slicer_debug_ui Debug build：PASS
compiler：MSVC 19.51.36248.0
output：build-12c-ui/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
fresh elapsed：259.8 s
```

fresh binary：

```powershell
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

结果：

```text
PASS startup
PASS experimental-report-summary
```

## 5. 存储事件说明

复测期间承载 E/F 分区的 Samsung SSD 990 EVO Plus 曾出现 NTFS Event 50/140 和 Disk Event 51，造成随机读取失败和伪删除状态。2026-07-12 00:20 仍记录过一次 E/F 写入失败；其后当前仓库完成：

```text
多文件重复读取：PASS；
git status：恢复为预期的 5 项 R0-01 改动；
git fsck --full --no-progress：exit 0，仅有 dangling objects；
build-12c-ui 删除后重新 fresh configure/build：PASS。
```

该事件不是 Qt compatibility shim 的逻辑错误，但属于独立的高风险硬件/文件系统残留问题。若相同 NTFS/Disk 事件再次出现，应立即停止写入并先处理存储设备。

## 6. 边界

```text
不修改本机 Qt 安装目录；
不升级 Qt；
不修改 slicer_core 公共 API；
不修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print；
不默认启用 OpenVDB；
不改变 legacy production slicer。
```

## 7. 决策结果

```text
12C-R0-01：COMPLETE
Selected lane：project-local Qt 5.15.2 / MSVC 19.50+ compatibility shim
Fresh build gate：PASS
Next task：12C-R0-02 UI Self-Test 与 Smoke 基线
```
