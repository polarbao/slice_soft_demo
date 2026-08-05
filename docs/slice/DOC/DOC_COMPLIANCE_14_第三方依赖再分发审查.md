# DOC_COMPLIANCE_14 第三方依赖再分发审查

> 状态：ACTIVE / 14A-07 COMPLETE
> 日期：2026-08-05
> 范围：Stage 14 首版能力包的 assimp、miniz、LibTIFF

## 1. 审查结论

三项依赖均采用允许商业二进制再分发的宽松许可。首版可分发，但发布包必须包含根目录
`THIRD_PARTY_NOTICES.txt` 和完整 `licenses/` 目录。不得使用版权方或贡献者名称为 SliceSoft
背书。每次打包还必须从最终 runtime 目录重新生成依赖清单，不能只依据 `vcpkg.json` 推断。

## 2. 实际链接与分发状态

| 组件 | 当前事实 | 许可证 | 发布动作 |
|---|---|---|---|
| miniz 3.1.0 | `CMakeLists.txt` 将四个 vendored C 文件静态编入 `slicer_core` | MIT + 上游 public-domain dedication | 所有能力包都带 NOTICE 与 `licenses/miniz.txt` |
| LibTIFF | 仅 `SLICESOFT_TIFF_BACKEND=libtiff` 时 `find_package(TIFF)` 并链接/复制 runtime | LibTIFF 宽松许可 + LZW notice | 仅 LibTIFF 构建分发 DLL，但所有包可统一带许可证 |
| Assimp | 仅在 `vcpkg.json` 声明；当前 CMake target 图没有 `find_package(assimp)` 或链接项 | BSD-3-Clause + Poly2Tri notice | 当前不复制 Assimp DLL；将来启用时许可证已经就绪，仍需重新做 runtime inventory |

这一区分很重要：**manifest 声明不等于二进制实际使用**。当前 Stage 14 不得因为 Assimp 已声明
就把其 DLL 盲目复制进包；也不得因为 LibTIFF 是可选后端而漏掉启用后的 runtime 和 notice。

## 3. 许可证来源与完整性

```text
miniz     源码 src/third_party/miniz/miniz.c 的头部 MIT 条款及尾部 Unlicense 文本
LibTIFF   vcpkg_installed/x64-windows/share/tiff/copyright
Assimp    vcpkg_installed/x64-windows/share/assimp/copyright
```

LibTIFF/Assimp 文本取自仓库固定 vcpkg baseline `d13fa75214c258099923cf25a5e6311e58c07f3b`
解析出的本地安装证据。后续更新 baseline 或端口版本时必须重新比对，不得假设许可证文本不变。

## 4. 发布包清单

Stage 14 的 14C/14F 打包门禁必须至少检查：

```text
THIRD_PARTY_NOTICES.txt
licenses/miniz.txt
licenses/libtiff.txt
licenses/assimp.txt
```

若实际 runtime inventory 出现 zlib、zstd、jpeg、lzma、OpenVDB 或其他 DLL，必须先把相应许可证
加入 NOTICE/`licenses`，再允许发布。OpenVDB 实验轨、Qt 宿主应用和操作系统组件不在本卡范围，
不代表它们天然无需审查。

## 5. 风险与后续门禁

| 风险 | 处置 |
|---|---|
| 手工维护 NOTICE 与二进制漂移 | 14C 打包从最终 runtime 目录生成依赖 inventory，并与本清单比对 |
| Assimp 未使用却误带 DLL | 只复制 CMake runtime 依赖，不按 vcpkg manifest 全量复制 |
| LibTIFF 可选构建漏证 | 两种 TIFF backend 都运行 notice 合同测试；LibTIFF 构建另查 DLL |
| 依赖升级后版权年份/附加条款变化 | baseline/端口变更触发合规复审 |

本卡不修改依赖版本、默认 TIFF backend 或 CMake 链接图，不构成启用 Assimp/LibTIFF 的授权。
