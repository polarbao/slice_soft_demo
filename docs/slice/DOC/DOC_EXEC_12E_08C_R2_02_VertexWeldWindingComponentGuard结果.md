# DOC_EXEC_12E-08C-R2-02 Vertex Weld、Winding 与组件守门结果

> 文档状态：COMPLETE
> 日期：2026-07-20

## 1. 实现范围

在 R2-01 隔离 candidate 后新增显式 R2-02 operation set：

```text
受约束 vertex weld；
基于共享边奇偶约束的唯一 local winding 传播；
connected component 不隐式 merge；
triangle material/per-corner UV 保持；
output vertex -> ordered source vertices 映射。
```

R2-02 只在 `allowVertexWeld` 或 `allowWindingRepair` 显式开启时运行。`weldToleranceMm=0` 继续表示禁用；
有效焊接阈值取显式配置与 scale-aware position epsilon 的较大值，不存在模型专用硬编码。

## 2. Vertex Weld 守门

候选顶点通过确定性空间桶检索，并且只允许属于同一个原始 edge-connected component 的顶点归并。候选整体
执行后重新检查：

```text
不得产生退化三角形；
组件数不得改变；
boundary/non-manifold/duplicate/opposite duplicate/winding issue 不得增加；
完整 self-intersection 检查必须可用且不得新增 confirmed pair；
任一守门失败，丢弃整个 R2-02 topology candidate。
```

报告新增 `vertexMappings[]`，每个输出顶点记录有序 source vertex id；每个真实 weld group 生成
`weld_vertex` operation。

## 3. Winding 守门

共享边邻接图为每对三角形建立 flip parity 约束。只在传播无冲突、翻转集合唯一且组件非零体积时执行。
翻转同步交换 triangle corner `1/2` 与 UV corner `1/2`。非流形参与、奇偶冲突、等价双解或零体积组件均稳定
返回 `blocked_winding_ambiguity`，不修改输入 candidate。

## 4. Generated Fixture

```text
split-vertex closed box：1 个 weld group，post strict PASS；
两个近邻独立 box：0 weld，组件保持 2；
会产生退化面的 weld：blocked_vertex_weld_guard，候选丢弃；
单面反向 closed box：唯一 flip，UV corner 同步，post strict PASS；
non-orientable strip：blocked_winding_ambiguity，0 flip；
clean Texture2D 3MF：no-op strict PASS。
```

## 5. 真实模型证据

真实模型使用 `weldToleranceMm=0.0001`，每个 case 连续执行两次：

| Case | weld | flip | V before/after | C before/after | 状态 |
|---|---:|---:|---:|---:|---|
| `nai_you_new` | 0 | 0 | 58924 / 58924 | 10 / 10 | `manual_repair_required` |
| `aishen_fudiao` | 0 | 0 | 42193 / 42193 | 10 / 10 | `manual_repair_required` |
| `meigui_fudiao` | 0 | 0 | 34722 / 34722 | 2 / 2 | `manual_repair_required` |
| Texture2D 3MF | 0 | 0 | 8 / 8 | 1 / 1 | `strict_pass_no_repair` |

三个 OBJ 在 adapter 后没有满足当前阈值和组件守门的额外 weld，也没有 local winding issue，因此 R2-02
没有制造模型专用操作；R2-01 的两个 degenerate provenance operation 继续保留。boundary、non-manifold 和
opposite duplicate 仍由后续 Gate 处理。

## 6. 验证入口

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_02|cleanup|contract|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_02_topology_evidence.ps1 -BuildDir build -Config Debug
```

验证结果：

```text
Debug 全量构建：PASS；
Debug CTest：29/29 PASS；
Qt startup self-test：PASS；
Qt experimental-report-summary：PASS；
R2-02 定向 CTest：6/6 PASS；
真实模型 4/4 case 双运行 stable projection：PASS；
run_ci_quick.ps1：FAIL（已知既有 Golden 基线，material_process_top2 widthPx expected=48 actual=226）。
```

Quick CI 失败发生在既有真实模型 Golden 尺寸断言，与本任务只读 mesh repair diagnostic 路径无关；本任务
没有修改模型缩放、切片 grid 或生产输出。该失败不伪装为 PASS，继续作为仓库基线问题记录。

## 7. 阶段结论

R2-02 完成，并未使三个真实 OBJ 获得 strict PASS。R2-03 前置已解除，但 Boundary Loop Repair 只能处理
简单、平面、凸、预算内且新面属性策略明确的闭环；12E-08D 继续 BLOCKED。

固定边界保持不变：OpenVDB optional/OFF；legacy 不调用 repair；不写生产 package/TIFF；
`p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 不变。
