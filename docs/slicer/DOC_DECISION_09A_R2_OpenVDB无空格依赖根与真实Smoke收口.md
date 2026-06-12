# DOC_DECISION_09A_R2_OpenVDB无空格依赖根与真实Smoke收口

> 文档版本：v0.1  
> 文档状态：Decision / 阶段修正决策  
> 适用阶段：REPORT_09A_R1 之后  
> 建议提交目录：`docs/slicer/`  
> 推荐分支：继续使用 `spike/09-openvdb-sdf-kernel`

---

## 1. 阶段判断

09A-R1 已完成真实环境复测，但 OpenVDB 真实 smoke 仍未通过。

已确认：

```text
VCPKG_ROOT = D:\Program Files Tools\vcpkg
vcpkg manifest mode 可进入
openvdb:x64-windows@12.0.1 已被解析
ON configure 最终失败于 hwloc:x64-windows@2.11.2
失败根因是 vcpkg root 路径包含空格
OFF build 与 run_ci_quick.ps1 均通过
```

因此当前不能进入：

```text
09B：SDF surface shell texture prototype
```

下一阶段应为：

```text
09A-R2：OpenVDB 无空格依赖根、真实 ON 构建与 Smoke 收口
```

---

## 2. 为什么需要 09A-R2

当前失败并非 GeometryKernel 或 OpenVdbAdapter 代码逻辑失败，而是 OpenVDB 传递依赖 `hwloc` 在包含空格的 vcpkg root 下无法可靠完成 autotools/libtool/make 构建。

09A-R2 只解决环境与依赖复现问题：

```text
1. 建立无空格的专用 vcpkg root；
2. 重新 bootstrap vcpkg；
3. 使用全新的 build-openvdb-r2；
4. 跑通 USE_OPENVDB=ON configure；
5. 跑通 geometry_kernel_demo ON build；
6. 跑通真实 openvdb-smoke；
7. 确认 activeVoxels > 0；
8. 更新依赖文档；
9. 保持 OFF 主线回归通过。
```

---

## 3. 推荐方案

不建议直接移动当前：

```text
D:\Program Files Tools\vcpkg
```

推荐新建专用无空格 root：

```text
D:\vcpkg-openvdb
```

或：

```text
D:\Tools\vcpkg
```

推荐优先使用：

```text
D:\vcpkg-openvdb
```

以避免影响现有其他项目。

---

## 4. 09B 进入条件

只有满足以下全部条件，才进入 09B：

```text
USE_OPENVDB=ON configure 成功
geometry_kernel_demo ON build 成功
openvdb-smoke 返回 0
openvdb.enabled == true
openvdb.available == true
openvdb.activeVoxels > 0
openvdb.version 非空
OFF run_ci_quick.ps1 仍通过
```

---

## 5. 必须保持不变

```text
p0.rgbwsv.2 不变
R G B W S V 通道顺序不变
8-bit / black_is_print 不变
Model > Support > Empty 不变
SupportType 不进入 TIFF channel
production slicer_cli 不接入 OpenVDB
SupportShapePipeline 不替换
USE_OPENVDB=OFF 默认可构建
```

---

## 6. 完成后路线

09A-R2 成功后进入：

```text
09B：SDF surface shell texture prototype
```

09A-R2 仍失败时：

```text
不进入正式 09B；
生成 REPORT_09A_R2；
根据失败包决定 09A-R3 或企业内部预编译依赖方案。
```
