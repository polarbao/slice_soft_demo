# REPORT R-F-02 真实资产预算重测当前状态

> 状态：**COMPLETE / VALIDATED / R-F CLOSED**  
> 日期：2026-08-11  
> 配置：Windows Release，宿主默认 `maxBytes=128 MiB`

## 1. 结论

- 冻结的 36 个 OBJ 全部进入复测：**22 个 lod0 完整显示、0 个预算拒绝、14 个继续按资产合同显式拒绝**；
- 22 个有效资产聚合后为 **1,005,246 三角 / 665,123 顶点**，仍返回 lod0；
- 宿主解码 mesh 为 **33.17 B/三角**，相对 R-F-01 前的 105.15 B/三角下降 **68.45%**；
- R-B-04 的小型真实纹理 fixture 仍为 **304 B → 176 B（42.1%）**，说明 float16 wire 合同没有被 R-F-01 破坏；
- 聚合宿主资源中纹理占 **75.60%**，网格占 23.92%，俯视预览占 0.48%；屏幕空间纹理分辨率具有明确内存价值，但 R-C-01 仍属受控合同修订，当前结论为 **TECHNICALLY JUSTIFIED / GOVERNANCE DEFERRED**；
- 仓库没有把 `meshIdentity` 的具体哈希值固化为测试常量，真实宿主缓存刷新和显示门禁通过。

## 2. 聚合资源构成

| 资源 | 字节 | MiB | 占比 |
|---|---:|---:|---:|
| Mesh（宿主 float32 解码后） | 33,346,888 | 31.80 | 23.92% |
| Texture（RGBA8） | 105,380,368 | 100.50 | 75.60% |
| Top preview（RGBA8，按 identity 去重） | 666,112 | 0.64 | 0.48% |
| 合计 | 139,393,368 | 132.94 | 100.00% |

该 `meshBytes` 口径等于 CPU 后端持有的 position/normal/UV/index 数据，不等于 DLL wire 的 float16 字节。R-B-04 的 176 B 是另一条“小型 fixture wire 编码”口径，两者均保留，禁止直接相除。

## 3. 单轮同机耗时证据

| 测量项 | 耗时 |
|---|---:|
| Mesh 后端复制上传 | 8.733 ms |
| Texture 后端复制上传 | 20.326 ms |
| three_d 完整 Refresh | 6,211.609 ms |
| top preview 完整 Refresh | 8,391.637 ms |

后端资源复制合计约 29.06 ms，不是 6–8 秒刷新耗时的主要来源；完整 Refresh 还包含模块查询、ViewData 生成、blob 传输、解码和缓存。该数据来自最终验收轮 Release 真实资产矩阵，只用于构成判断，不冒充 p50/p95 性能基线。

## 4. 屏幕空间纹理预算判断

在仅按面积比例估算、mesh 与 preview 不变的情况下：

| 纹理线性尺寸策略 | 估算总资源 | 相对当前减少 |
|---|---:|---:|
| 当前全分辨率 | 132.94 MiB | 0% |
| 宽高各减半（纹理面积 25%） | 57.56 MiB | 56.70% |
| 宽高各降至四分之一（纹理面积 6.25%） | 38.72 MiB | 70.87% |

因此 R-C-01 有明显的内存与传输潜力；但该字段会改变已冻结的 ViewData 请求/响应决策，仍必须经过受控修订和打印侧回签。本任务不提前修改 DTO，也不改变默认纹理分辨率。

## 5. 实际证据

```text
validAssets=22
sceneInstances=22
rendered=true
lod=lod0
vertices=665123
triangles=1005246
meshBytes=33346888
textureBytes=105380368
previewBytes=666112
meshUploadNs=8732800
textureUploadNs=20326000
threeDRefreshNs=6211608900
previewRefreshNs=8391636600
topRendered=true
previewIdentityCount=22
```

Release 验证：

- `hostflow_hd02_real_asset_matrix`：`1/1 PASS`，143.78 s；
- `slicer_stage14e04c_three_d_contract_test`：`1/1 PASS`，继续输出 `floatBytes=304 halfBytes=176`；
- R-F-01 的 Debug 1/1 与 Release 4/4 回归已在前置任务完成。

完整 CSV 与聚合证据位于：

- `build-slicesoft/main/hostflow_hd02_evidence/Release/render_ra02_real_asset_matrix.csv`；
- `build-slicesoft/main/hostflow_hd02_evidence/Release/render_ra02_aggregate_scene.txt`。

## 6. 边界

- 本任务不改变 SPI v1、11 个导出、15 项能力、ViewData DTO 和生产 RGBWSV TIFF；
- 14 个资产拒绝仍是缺纹理、未知材质或 UV 合同问题，不属于 R-F 的法线与预算范围；
- 未建立跨机器性能承诺；Stage 16 若需要性能基线，仍必须按其冻结场景重新测量；
- R-F 线至此关闭，允许进入 Stage 16 的 16-00 准入审计。
