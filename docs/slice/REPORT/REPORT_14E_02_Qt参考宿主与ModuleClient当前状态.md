# REPORT_14E-02 Qt 参考宿主与 ModuleClient 当前状态

> 状态：COMPLETE
> 日期：2026-08-07
> 前置：14E-01 PASS / M-MVP PASS
> 下一任务：14E-03 三车道交互控制器与 Stale 回滚

## 1. 任务目标

建立独立 `apps/slicer_ui_host_sim/` Qt 参考宿主，使打印软件可以直接参考一条不依赖
切片内部实现的集成路径。宿主仅通过运行时 C ABI 使用 `slicer_module.dll`，不得链接
`slicer_core`、`slicer_base`、`slicer_engine` 或 `slicer_module`。

## 2. 实现内容

| 模块 | 交付物 | 说明 |
|---|---|---|
| Qt 参考宿主 | `apps/slicer_ui_host_sim/` | 独立 Qt5 Widgets target；未修改既有 `slicer_debug_ui` |
| ABI 客户端 | `ModuleClient.*` | `LoadLibraryW/GetProcAddress` 解析冻结 11 个导出，校验 SPI v1 与 15 项能力 |
| 作业入口 | `ModuleClient` 公共方法 | submit/poll/cancel/result/release、自检、错误读取与调用计数均有可读示例 |
| 最小界面 | `HostMainWindow.*` | 显示模块路径、自述和自检状态；DLL 缺失时保留可启动界面 |
| 自动门禁 | `tests/stage14e_02/` | 源码/CMake 边界、PE 导入表、缺 DLL 负例与真实模块 smoke |

新增源文件均不超过 500 行，不进入 source-size allowlist。

## 3. 边界证据

三重依赖守卫均已自动化：

1. CMake target 仅链接 `Qt5::Widgets`，不链接任何切片内部 target。
2. 新宿主源码禁止 include 或引用 `slicer_core/slicer_base/slicer_engine`。
3. PE Import Table 不含 `slicer_module.dll` 或内部切片库；模块只在运行时加载。

`ModuleClient` 预留单调 ABI 调用计数器，供 14E-03 的 UI-M1 和后续 UI-M7
验证“拖拽/相机操作期间跨 DLL 调用恒为 0”。本任务尚未声明这些交互指标已经通过。

## 4. 验证结果

### 4.1 Debug

```text
cmake --build build --config Debug --target slicer_ui_host_sim --parallel 4
ctest --test-dir build -C Debug -R "stage14e02" --output-on-failure

3/3 PASS
```

### 4.2 Release

```text
cmake --build build --config Release --target slicer_ui_host_sim --parallel 4
ctest --test-dir build -C Release -R "stage14e02" --output-on-failure

3/3 PASS
```

三项测试分别覆盖：

- 源码/CMake/PE Import Table 依赖边界；
- 缺失 DLL 时稳定返回 `MODULE_LOAD_FAILED`，无崩溃；
- 真实 Debug/Release `slicer_module.dll` 装载、自述、15 能力与自检。

`ValidateSourceSizeGuard.py --base-ref HEAD` PASS，仅保留既有 G4/G5 warning。

## 5. 阶段结论

14E-02 已完成参考宿主的公开 ABI 装载外壳，但尚未实现模型交互、三车道提交、
纹理 ViewData、切片作业界面或双视图渲染。下一步只能按冻结顺序进入 14E-03，
由 `SceneInteractionController` 与 `TransformCommitPolicy` 关闭 UI-M1/UI-M4。

## 6. 冻结边界

- SPI 保持 v1，导出保持 11 个，能力保持 15 项。
- 生产 Package、RGBWSV TIFF、位深、极性和 Worker 唯一路径均未修改。
- 既有 `apps/slicer_debug_ui/` 未修改。
- 14E-02 不选择渲染后端，不直接调用 OpenGL/D3D，也不读取切片内部对象。
