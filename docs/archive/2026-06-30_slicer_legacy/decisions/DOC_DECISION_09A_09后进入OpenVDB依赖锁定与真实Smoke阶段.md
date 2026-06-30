# DOC_DECISION_09A_09后进入OpenVDB依赖锁定与真实Smoke阶段

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_09 之后  
> 建议提交目录：`docs/slicer/`  
> 推荐分支：`spike/09-openvdb-sdf-kernel` 或从该分支切出 `spike/09A-openvdb-dependency-lock`

---

## 1. 阶段判断

根据 `REPORT_09_OpenVDB_SDF几何内核预研当前状态.md`，09 已完成 OpenVDB / SDF 几何内核第一轮隔离预研：

```text
1. src/slicer_core/geometry 已建立；
2. geometry_kernel_demo target 已建立；
3. DistanceField2D / ShellMask / GeometryKernelReport 已实现；
4. USE_OPENVDB=OFF 默认可构建；
5. heightfield-sdf / surface-shell / openvdb-smoke 在 OFF 下可运行；
6. geometry_kernel_report.json 输出 p0.geometry_kernel_report.1；
7. preview PNG 已生成；
8. production slicer_cli / RGBWSV / SupportShapePipeline 未被替换。
```

但 09 同时暴露一个关键事实：

```text
USE_OPENVDB=ON 配置失败，原因是本机没有可被 CMake 找到的 OpenVDB package config。
```

因此，09 可以收口为：

```text
OpenVDB/SDF 几何内核隔离原型完成
```

但不建议马上进入 `09B/09C` 或直接做生产策略原型。下一步应先进入：

```text
09A：OpenVDB 依赖锁定与真实 Smoke 阶段
```

---

## 2. 为什么需要 09A

项目已经明确后续会使用 OpenVDB 做正式切片几何内核能力。如果 `USE_OPENVDB=ON` 仍无法稳定构建，那么继续做 surface shell / compensated varnish / support clearance 只会停留在纯 C++ prototype 层。

09A 的目标是先解决：

```text
OpenVDB 如何在当前 Windows / CMake / C++20 工程中稳定安装、发现、链接、运行。
```

这比继续扩展 shell prototype 更优先。

---

## 3. 09A 阶段目标

09A 目标：

```text
1. 确定 OpenVDB 依赖接入方案；
2. 让 USE_OPENVDB=ON 在目标开发环境至少有一条可复现成功路径；
3. 完成真实 OpenVDB smoke case；
4. 生成 OpenVDB dependency lock 文档；
5. 不影响 USE_OPENVDB=OFF 默认构建；
6. 不接入 production slicer_cli 输出链路。
```

---

## 4. 推荐依赖路径

优先级建议：

```text
优先 A：vcpkg manifest mode
备选 B：Conan profile
备选 C：源码编译 / 预编译包
```

建议优先尝试：

```text
vcpkg manifest mode
```

原因：

```text
1. Windows/CMake 项目集成相对稳定；
2. 可提交 vcpkg.json 锁定依赖；
3. Codex/CI 更容易复现；
4. 后续可扩展到 OpenVDB + TBB + Blosc + Boost + Imath。
```

---

## 5. 09A 不做什么

```text
1. 不替换当前 production slicer_cli；
2. 不把 VDB/SDF 输出写入 RGBWSV TIFF；
3. 不实现 production surface_shell_texture；
4. 不实现 production compensated_varnish；
5. 不替换 SupportShapePipeline；
6. 不做设备通信；
7. 不做 RIP 半色调；
8. 不做 ICC / CMYK；
9. 不破坏 USE_OPENVDB=OFF 默认构建。
```

---

## 6. 09A 完成后的路线

09A 完成后，如果真实 OpenVDB smoke 成功，可以进入：

```text
09B：SDF surface shell texture prototype
```

如果真实 OpenVDB 仍失败，但失败原因明确且短期无法解决，则可以选择：

```text
09B-alt：基于 pure-cpp DistanceField2D 的 surface shell prototype
```

但这应标记为过渡方案，不应视为最终 OpenVDB 采用完成。
