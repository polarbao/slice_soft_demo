# DOC_DECISION_RENDER_R_B_00 ViewMesh 复用 DTO 受控修订

> 状态：**ACCEPTED / USER AUTHORIZED / IMPLEMENTED**
> 日期：2026-08-10
> 任务：`R-B-00`（RB-P2）
> 上游：`DOC_ANALYSIS_RENDER_RD_B_前置复核_预算膨胀三处根因.md`
> 受影响合同：`contracts/slicer_capability_dtos.*` v1.8、`DOC_SCHEMA_14_SceneViewData网格DTO规格.md` v1.3

## 1. 决策背景

旧 three_d ViewData 把完整 `ViewMesh` 内联在每个 `instances[].mesh`。同一个 modelId
排版为多个实例时，模块会重复构建相同局部网格，预算按实例重复计算，Adapter 也会重复创建
mesh blob。这与既有“local mesh 可跨实例矩阵复用”的冻结语义冲突，是 RD-B 预算膨胀的
RB-P2 根因。

## 2. 冻结决策

1. 能力 DTO 合同从 v1.7 受控 minor 修订为 v1.8，SPI 仍为 v1。
2. three_d 规范网格集合提升为顶层 `meshes[]`，以 `meshIdentity` 唯一标识。
3. `instances[]` 只通过 `meshIdentity` 引用顶层网格；外观仍通过 `appearanceIdentity` 引用。
4. `meshTransform=local` 时按 modelId 缓存构建结果，同模型、同 LOD 只构建一次。
5. `meshTransform=world` 时矩阵已烘焙到顶点，只按最终 `meshIdentity` 去重。
6. v1.8 暂时保留 `instances[].mesh` 作为 v1.7 兼容别名；别名复用顶层描述符和同一
   `blobId`，禁止重复存储二进制数据。
7. 新宿主优先解析顶层 `meshes[]`，同时保留读取旧内联 mesh 的兼容路径。

## 3. 不变量

```text
PM_SPI_VERSION                  1
pm_* 导出                       11
能力数量                        15
scene.get_viewdata              仍为 query/read_blob
top ViewData                    不携带 meshes[]
three_d ViewData                必须闭合 meshes[] 与实例引用
worldMatrix-only 变化           不使 local meshIdentity 失效
生产 TIFF/RGBWSV/RIP            不变
meshoptimizer                   本任务不引入
```

## 4. 实现边界

模块在候选构建期建立 modelId 到 mesh 索引缓存；预算估算只统计顶层网格一次；闭合校验拒绝
重复 mesh identity、悬空实例引用和未被引用的顶层网格。序列化器每个 identity 只调用一次
blob 存储，兼容别名只复制 JSON 描述符。参考宿主先上传顶层 meshes，再按实例 identity 绑定。

## 5. 验收证据

- 合同验证：`python tests/contracts/ValidateCapabilityDtos.py`。
- Debug/Release：`textured_scene_viewdata_14b03a_unit_tests`。
- Debug/Release：`hostflow_hd02_three_d_canvas`。
- 单元用例证明 local 同模型双实例只有一个 `SceneViewData::meshes`，两个实例引用同一
  `meshIdentity`；world 模式下不同矩阵仍生成不同 mesh identity。

## 6. 后续

R-B-00 完成后进入 R-B-05：按 `position + normal + uv` 共享顶点，但 UV 缝必须保持分裂。
完成 RB-P1/P2/P3 后由 R-A-02 重测 36 个真实模型，再决定是否启动 meshoptimizer。
