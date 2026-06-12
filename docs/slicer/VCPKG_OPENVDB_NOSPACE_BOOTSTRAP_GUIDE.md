# VCPKG_OPENVDB_NOSPACE_BOOTSTRAP_GUIDE

> 文档版本：v0.1  
> 用途：为 OpenVDB 建立无空格 vcpkg root  
> 建议提交目录：`docs/slicer/`

---

## 1. 目标目录

推荐：

```text
D:\vcpkg-openvdb
```

不要使用包含空格的目录，例如：

```text
D:\Program Files Tools\vcpkg
```

---

## 2. 新建专用 vcpkg root

在 PowerShell 中执行：

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\vcpkg-openvdb

& "D:\vcpkg-openvdb\bootstrap-vcpkg.bat"
```

验证：

```powershell
Test-Path "D:\vcpkg-openvdb\scripts\buildsystems\vcpkg.cmake"

& "D:\vcpkg-openvdb\vcpkg.exe" version
```

预期：

```text
toolchain exists = True
vcpkg version 可输出
```

---

## 3. 设置当前终端环境变量

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"
```

验证：

```powershell
$env:VCPKG_ROOT
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

---

## 4. 使用干净 BuildDir

不要复用：

```text
build-openvdb
build-openvdb-r1
```

使用：

```text
build-openvdb-r2
```

如目录已存在且为失败残留：

```powershell
Remove-Item -Recurse -Force .\build-openvdb-r2
```

---

## 5. 配置 OpenVDB ON 构建

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-r2 `
  -Triplet x64-windows
```

该步骤可能需要较长时间，用于构建 OpenVDB 及传递依赖。

---

## 6. 执行真实 Smoke

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2
```

预期：

```text
OpenVDB smoke passed.
version: 非空
activeVoxels: 27 或其他大于 0 的值
```

---

## 7. OFF 主线回归

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

---

## 8. 失败处理

如果失败，必须记录：

```text
失败 port
vcpkg buildtrees 日志路径
configure/build 错误摘要
VCPKG_ROOT
triplet
CMake generator
OpenVDB port version
```

更新：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

不要绕过失败直接进入 09B。
