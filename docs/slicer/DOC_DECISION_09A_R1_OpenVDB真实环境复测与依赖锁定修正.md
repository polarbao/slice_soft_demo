# DOC_DECISION_09A_R1_OpenVDB真实环境复测与依赖锁定修正

> 文档版本：v0.1
> 文档状态：Decision / 阶段修正决策
> 适用阶段：REPORT_09A 之后
> 建议提交目录：`docs/slicer/`
> 推荐分支：继续使用 `spike/09-openvdb-sdf-kernel`

## 1. 阶段判断

根据 `REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态.md`，09A 已完成 OpenVDB 依赖接入的工程化准备：

```text
vcpkg.json 已声明 openvdb optional feature
USE_OPENVDB=OFF 默认构建通过
run_ci_quick.ps1 通过
USE_OPENVDB=ON 缺包错误已增强为可操作提示
configure_openvdb_vcpkg.ps1 已新增
run_openvdb_smoke.ps1 已新增
geometry_kernel_report.openvdb 字段已增强
production slicer_cli / RGBWSV / SupportShapePipeline 未被修改
```

但 09A 还有关键未完成项：

```text
真实 USE_OPENVDB=ON configure/build/smoke 尚未通过。
```

因此当前不建议进入：

```text
09B：SDF surface shell texture prototype
```

应先进入：

```text
09A-R1：OpenVDB 真实环境复测与依赖锁定修正
```

## 2. 09A-R1 目标

```text
1. 使用正确 VCPKG_ROOT 重新执行 ON configure；
2. 固化 vcpkg manifest feature 的可复现命令；
3. 修正脚本在带空格路径下的兼容性；
4. 跑通 run_openvdb_smoke.ps1；
5. 更新 OPENVDB_DEPENDENCY_NOTES.md；
6. 生成 REPORT_09A_R1；
7. 不接入 production slicer_cli。
```

## 3. 09B 进入条件

09B 的前置条件是：

```text
USE_OPENVDB=ON configure 成功
geometry_kernel_demo ON build 成功
openvdb-smoke executed=true
activeVoxels > 0
report.openvdb.enabled=true
report.openvdb.available=true
OFF run_ci_quick.ps1 仍通过
```

## 4. 必须保持不变

```text
p0.rgbwsv.2 不变
R G B W S V 通道顺序不变
8-bit / black_is_print 不变
Model > Support > Empty 不变
SupportType 不进入 TIFF channel
USE_OPENVDB=OFF 默认可构建
run_ci_quick.ps1 仍通过
OpenVDB 不成为所有开发环境强制依赖
```

## 5. 完成后路线

如果 09A-R1 跑通真实 ON smoke，则进入：

```text
09B：SDF surface shell texture prototype
```

如果仍无法跑通，但失败原因明确且短期无法解决，则只允许进入：

```text
09B-alt：pure-cpp shell prototype 临时过渡方案
```

并必须明确标记：

```text
09B-alt 不等价于 OpenVDB 采用完成。
```
