# BUILD_ENVIRONMENTS_编译调试环境说明

> 文档版本：v0.1  
> 文档状态：Current / VS Code & CMake 使用说明  
> 更新日期：2026-06-05  

## 1. 工程打开方式

当前项目存在两套 VS Code 配置：

```text
slice_test_demo/.vscode/
slice_test_demo/slice_soft_demo/.vscode/
```

两套配置用途不同。

### 1.1 打开外层目录

如果 VS Code 打开：

```text
e:\__Code\__Work\slice_test_demo
```

使用外层配置：

```text
slice_test_demo/.vscode/launch.json
slice_test_demo/.vscode/tasks.json
```

此时 `${workspaceFolder}` 是：

```text
e:\__Code\__Work\slice_test_demo
```

所以程序路径形如：

```text
${workspaceFolder}/slice_soft_demo/build/Debug/slicer_cli.exe
```

工作目录是：

```text
${workspaceFolder}/slice_soft_demo
```

### 1.2 打开内层工程目录

如果 VS Code 打开：

```text
e:\__Code\__Work\slice_test_demo\slice_soft_demo
```

使用内层配置：

```text
slice_test_demo/slice_soft_demo/.vscode/launch.json
slice_test_demo/slice_soft_demo/.vscode/tasks.json
```

此时 `${workspaceFolder}` 是：

```text
e:\__Code\__Work\slice_test_demo\slice_soft_demo
```

所以程序路径形如：

```text
${workspaceFolder}/build/Debug/slicer_cli.exe
```

工作目录是：

```text
${workspaceFolder}
```

注意：

```text
不要把内层配置写成 ${workspaceFolder}/slice_soft_demo。
否则会变成不存在的二级路径。
```

## 2. CMake Targets

当前 CMake target：

```text
slicer_core
slicer_cli
rip_reader_test
```

### 2.1 slicer_core

类型：

```text
library
```

作用：

```text
配置读取
模型导入
切片采样
Relief heightfield
Support / island detection
RGBWSV TIFF 输出
manifest / reports
RIP reader 核心逻辑
```

### 2.2 slicer_cli

类型：

```text
executable
```

作用：

```text
执行切片
执行模型检查
执行 preview-only
生成 output package
```

常见参数：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
build\Debug\slicer_cli.exe --config samples\configs\slice_config_model_0_3.json --inspect-model
build\Debug\slicer_cli.exe --config samples\configs\slice_config_model_0_3.json --preview-only
```

### 2.3 rip_reader_test

类型：

```text
executable
```

作用：

```text
读取切片 package
校验 manifest
校验 RGBWSV TIFF
校验 bitDepth / polarity / channelOrder
校验 layer 文件
执行正向和负向验收
```

常见参数：

```powershell
build\Debug\rip_reader_test.exe --package output\SlicePackage
```

## 3. VS Code Tasks

### 3.1 SliceSoft: Configure

作用：

```text
执行 CMake configure。
```

内层打开时等价于：

```powershell
cmake -S . -B build
```

外层打开时等价于：

```powershell
cmake -S slice_soft_demo -B slice_soft_demo/build
```

何时使用：

```text
首次构建
CMakeLists.txt 改动后
构建目录不存在时
```

### 3.2 SliceSoft: Build Debug

作用：

```text
构建 Debug 版本。
```

内层打开时等价于：

```powershell
cmake --build build --config Debug
```

外层打开时等价于：

```powershell
cmake --build slice_soft_demo/build --config Debug
```

依赖：

```text
SliceSoft: Configure
```

说明：

```text
所有 Debug launch 配置都会先执行这个 task。
```

### 3.3 SliceSoft: Run slicer_cli sample

作用：

```text
运行普通 P0 sample。
```

配置：

```text
samples/configs/slice_config.json
```

输出：

```text
output/SlicePackage
```

适用场景：

```text
验证基础 closed_mesh_scanline
验证普通支撑 bottom_projection
验证 P0 基础闭环
```

### 3.4 SliceSoft: Run rip_reader_test sample

作用：

```text
先运行普通 sample，再校验 output/SlicePackage。
```

适用场景：

```text
验证基础 package 能被 RIP reader 接受。
```

### 3.5 SliceSoft: Run slicer_cli model 0.3

作用：

```text
对 0.3 OBJ 模型执行普通 closed mesh 切片。
```

配置：

```text
samples/configs/slice_config_model_0_3.json
```

适用场景：

```text
对比旧 scanline / bottom projection 行为
排查普通 OBJ 模型切片问题
```

### 3.6 SliceSoft: Inspect model 0.3

作用：

```text
只检查模型导入和 bbox / triangle / material 基础信息。
```

参数：

```text
--inspect-model
```

适用场景：

```text
排查模型是否过高
排查自动旋转是否生效
排查 OBJ / MTL 读取统计
```

### 3.7 SliceSoft: Preview-only model 0.3

作用：

```text
只生成预览，不输出完整生产 TIFF layers。
```

参数：

```text
--preview-only
```

适用场景：

```text
快速查看模型区域 / 支撑区域
避免大模型完整 TIFF 输出耗时
```

### 3.8 SliceSoft: Run slicer_cli relief varnish

作用：

```text
运行 00C / 01 阶段 relief 光油 + 支撑兼容入口。
```

配置：

```text
samples/configs/slice_config_relief_varnish.json
```

输出：

```text
output/SlicePackage_relief_varnish
```

适用场景：

```text
验证 relief_heightfield
验证 V 光油通道
验证 S 下表面支撑
验证 00C_FINAL 兼容入口
```

### 3.9 SliceSoft: Preview-only relief varnish

作用：

```text
只生成 relief varnish preview，不输出完整生产 TIFF layers。
```

适用场景：

```text
快速查看浮雕 V / S 通道预览
排查浮雕高度场切片效果
```

### 3.10 SliceSoft: Run rip_reader_test relief varnish

作用：

```text
先运行 relief varnish 切片，再校验 output/SlicePackage_relief_varnish。
```

适用场景：

```text
验证 relief package 是否满足 RIP reader 输入要求。
```

### 3.11 SliceSoft: Run support unsupported_only

作用：

```text
运行 02 阶段 unsupported_only 支撑样例。
```

配置：

```text
samples/configs/support/support_unsupported_only.json
```

输出：

```text
output/SupportUnsupportedOnly
```

适用场景：

```text
验证 connected component
验证 island detection
验证 unsupported island 支撑
验证 supportTypeStats.unsupported_island
```

### 3.12 SliceSoft: Run support bottom_plus_unsupported

作用：

```text
运行 02 阶段 bottom_projection_plus_unsupported 组合支撑样例。
```

配置：

```text
samples/configs/support/support_bottom_plus_unsupported.json
```

输出：

```text
output/SupportBottomPlusUnsupported
```

适用场景：

```text
验证 bottom_projection 与 unsupported island 同时存在
验证组合支撑统计
验证 supportTypeStats.bottom_projection
验证 supportTypeStats.unsupported_island
```

## 4. VS Code Launch Configurations

Launch 配置和 task 基本一一对应，但 launch 会进入调试器。

### 4.1 Debug slicer_cli sample

调试：

```text
slicer_cli + samples/configs/slice_config.json
```

用途：

```text
基础切片流程断点调试。
```

### 4.2 Debug rip_reader_test sample

调试：

```text
rip_reader_test + output/SlicePackage
```

用途：

```text
RIP reader 正向校验断点调试。
```

### 4.3 Debug slicer_cli model 0.3

调试：

```text
slicer_cli + samples/configs/slice_config_model_0_3.json
```

用途：

```text
普通 0.3 OBJ 模型切片调试。
```

### 4.4 Debug inspect model 0.3

调试：

```text
slicer_cli --inspect-model
```

用途：

```text
模型导入、bbox、自动旋转、材质统计调试。
```

### 4.5 Debug preview-only model 0.3

调试：

```text
slicer_cli --preview-only
```

用途：

```text
普通模型 preview 生成调试。
```

### 4.6 Debug slicer_cli relief varnish

调试：

```text
relief_heightfield + V 光油 + S 支撑
```

用途：

```text
浮雕高度场、V 通道、S 支撑调试。
```

### 4.7 Debug preview-only relief varnish

调试：

```text
relief preview-only
```

用途：

```text
快速调试浮雕 preview。
```

### 4.8 Debug rip_reader_test relief varnish

调试：

```text
rip_reader_test + output/SlicePackage_relief_varnish
```

用途：

```text
浮雕 package 的 RIP reader 校验调试。
```

### 4.9 Debug support unsupported_only

调试：

```text
support.mode = unsupported_only
```

用途：

```text
02 阶段 island detection 与 unsupported 支撑调试。
```

### 4.10 Debug support bottom_plus_unsupported

调试：

```text
support.mode = bottom_projection_plus_unsupported
```

用途：

```text
02 阶段组合支撑模式调试。
```

## 5. 推荐使用顺序

### 5.1 首次打开项目

```text
1. SliceSoft: Configure
2. SliceSoft: Build Debug
3. SliceSoft: Run slicer_cli sample
4. SliceSoft: Run rip_reader_test sample
```

### 5.2 调试普通模型

```text
1. SliceSoft: Inspect model 0.3
2. SliceSoft: Preview-only model 0.3
3. SliceSoft: Run slicer_cli model 0.3
```

### 5.3 调试浮雕模型

```text
1. SliceSoft: Preview-only relief varnish
2. SliceSoft: Run slicer_cli relief varnish
3. SliceSoft: Run rip_reader_test relief varnish
```

### 5.4 调试支撑和孤岛检测

```text
1. SliceSoft: Run support unsupported_only
2. SliceSoft: Run support bottom_plus_unsupported
```

## 6. 常见问题

### 6.1 运行与调试列表为空

可能原因：

```text
VS Code 打开的目录不是 slice_test_demo 或 slice_soft_demo。
```

处理：

```text
打开 slice_test_demo 或 slice_soft_demo。
执行 Developer: Reload Window。
```

### 6.2 找不到 slicer_cli.exe

可能原因：

```text
尚未构建。
build/Debug 目录不存在。
打开外层目录时误用了内层配置。
打开内层目录时误用了外层配置。
```

处理：

```text
运行 SliceSoft: Build Debug。
确认当前打开目录与 .vscode 配置匹配。
```

### 6.3 找不到 config JSON

可能原因：

```text
cwd 不正确。
```

正确规则：

```text
外层打开：cwd = ${workspaceFolder}/slice_soft_demo
内层打开：cwd = ${workspaceFolder}
```

### 6.4 Debug relief 很慢

原因：

```text
relief_nail_arched.obj 体量较大，完整 TIFF + RIP 校验耗时明显。
```

建议：

```text
先使用 preview-only。
需要完整验收时再运行 slicer_cli + rip_reader_test。
```

### 6.5 Debug support 样例更适合断点

原因：

```text
samples/models/support/floating_island.stl 很小。
```

建议：

```text
调试 island detection / supportTypeStats 时优先使用 support 样例。
```
