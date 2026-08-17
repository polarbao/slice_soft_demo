# REPORT HOSTFLOW H-F-10 Reality 批量导入碰撞加速

> 状态：**IMPLEMENTED / RUNTIME_DEPLOY_PENDING**
> 日期：2026-08-17
> 对应任务：`H-F-10`

## 1. 样本与结论

诊断目录 `model/obj/reality/2026081411323300004351_2_obj` 含 10 个 OBJ，合计约
15.36 MB、84,232 个顶点、84,232 个 UV、84,232 个法线和 160,944 个三角面。
模型规模本身不足以解释数分钟停顿。

Release 分段测量显示，单模型 Facade 解析约 58-83 ms、源文件哈希约 17-25 ms、
保留场景模型的第二次解析约 60-86 ms、快速预检约 9-13 ms。全部模型完成资源导入和预检后，
进程长时间停留在批量 `addInstance` 的权威场景提交；第一次未加速测量中，导入提交为
267.265 s，而排版、俯视刷新与三维刷新合计约 1 s。因此导入变慢不是文件读取、预检、
视图渲染或纹理加载造成的。

## 2. 根因

批量导入时，10 个实例先以单位变换加入场景，之后才按冻结的单操作合同调用
`applyGridLayout`。首次提交阶段所有实例投影同位，形成 45 个实例 AABB 候选对。

场景准入原实现对每个候选对执行：

```text
左实例全部投影三角形 × 右实例全部投影三角形 × 精确多边形裁剪
```

在每个模型约 1.5-1.8 万三角面的样本上，这一逐三角形笛卡尔积成为绝对热点。
尝试把 add 与排版放入同一批次会违反已冻结的 `applyGridLayout must be the only operation`
原子合同，因此没有采用该绕过方案。

## 3. 修复

`SceneCollisionService` 为右侧投影三角形建立确定性 BVH：节点保存二维 AABB，按跨度较大的轴
和原三角形索引稳定分割；查询先排除不相交的节点和三角形 AABB，只对候选三角形调用原有
`TrianglesOverlapWithPositiveArea` 精确裁剪。

BVH 只承担宽相位筛选。正面积重叠、边界接触、退化三角形、碰撞错误码和 fail-closed 结果仍由
原精确算法决定；未修改场景 DTO、SPI、Profile、TIFF、RGBWSV 或包协议。

## 4. Release 测量

同一目录、同一 Release 模块、同一主机：

| 指标 | 修改前 | 修改后五次复测 | 变化 |
|---|---:|---:|---:|
| 批量导入提交 | 267.265 s | 2.070-4.615 s，中位数 2.277 s | 最慢样本仍 -98.3%，约 57.9 倍加速 |
| 自动排版 | 31 ms | 24-65 ms | 非导入热点 |
| 俯视刷新与渲染 | 391 ms | 356-761 ms | 非导入热点 |
| 三维刷新与渲染 | 588 ms | 443-2,563 ms | 非导入热点；最慢样本存在并发负载 |
| 导入至双视图完成总时长 | 268.276 s | 2.898-8.004 s，中位数 3.119 s | 最慢样本仍 -97.0%，约 33.5 倍加速 |

五次结果保留完整范围，不用单次最快值替代验收结论。最后一次测量时已有运行中的参考宿主，
其三维刷新增至 2.475 s；这不改变批量提交热点已经消除的判断。

## 5. 验证与边界

Release 定向 CTest：

```text
scene_collision_admission_unit_tests   PASS
hostflow_ha02_scene_lifecycle_tests    PASS
hostflow_hb01_model_import             PASS
hostflow_hd02_three_d_canvas            PASS
```

最终 Release 构建与 4 项定向门禁已经通过。部署脚本检测到
`runtime/slicesoft/Release/slicer_ui_host_sim.exe` 正在运行（PID 41972），按保护规则拒绝覆盖；
因此当前 Runtime 目录的模块/宿主 SHA-256 与最终构建不一致，必须在关闭该进程后重新执行部署和
self-test，任务状态保持 `RUNTIME_DEPLOY_PENDING`，不得宣称运行目录已经更新。

扩展门禁 `scene_facade_14b03_unit_tests` 暴露一项既有历史断言：测试仍期望 Grid 首实例从
0 mm 开始，当前 H-F-03 合同和实现为 10 mm 边距，实际得到 10/30 mm 且碰撞为空。本次碰撞
加速未修改排版默认值，也未顺带改写该历史测试。
