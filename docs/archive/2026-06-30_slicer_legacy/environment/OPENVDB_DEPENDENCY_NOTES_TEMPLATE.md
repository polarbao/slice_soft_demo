# OPENVDB_DEPENDENCY_NOTES

> 文档版本：v0.1  
> 文档状态：Dependency Notes  
> 适用阶段：09  
> 建议提交目录：`docs/slicer/`

---

## 1. 目标

记录 OpenVDB 在当前项目中的接入方式、依赖风险和验证结果。

---

## 2. 环境信息

```text
OS:
Compiler:
CMake:
Build type:
vcpkg/conan/source:
OpenVDB version:
```

---

## 3. 依赖项

```text
OpenVDB:
TBB:
Blosc:
Boost:
Imath:
Zlib:
其他:
```

---

## 4. CMake 接入方式

```cmake
find_package(OpenVDB CONFIG REQUIRED)
```

或实际使用方式：

```text
TODO
```

---

## 5. USE_OPENVDB=OFF 结果

```text
Build:
geometry_kernel_demo:
run_geometry_kernel_tests:
run_ci_quick:
```

---

## 6. USE_OPENVDB=ON 结果

```text
Configure:
Build:
openvdb-smoke:
Runtime:
```

---

## 7. 已知问题

```text
TODO
```

---

## 8. 推荐方案

```text
TODO
```
