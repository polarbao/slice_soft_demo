# HOSTFLOW H-E E3 批量导入与白区预检实施准备

> 状态：**H-E-02 COMPLETE / H-E-06 READY**  
> 日期：2026-08-10  
> 范围：E3 批次，仅覆盖批量导入和 Stage 15 白区预检接入。

## 1. 批次准入

H-E-01/03/04/05 已完成，E1/E2 Gate 均为 PASS。宿主 Profile 已能表达支撑、材料、
生产纹理和 Stage 15 按需补白字段，因此 E3 不需要新增 ABI、能力或内部配置入口。

## 2. H-E-02 原子语义

批量导入采用以下顺序：

1. 宿主先校验选择数量、场景 22 实例容量、文件存在性和扩展名；
2. 按用户选择顺序执行 `model.import` 和快速预检，但尚不修改场景；
3. 任一资源失败时释放本批已导入的 model resource，场景 revision 和实例集合保持不变；
4. 全部资源通过后，以一次 `scene.apply_operation` 提交全部 `addInstance`；
5. 成功后 revision 只增加一次，UI 按选择顺序登记实例并只刷新一次双视图。

该合同避免“前两个模型已经入场、第三个失败”的半场景。批次失败不使用补偿式
`removeInstance`，因此不会产生额外 revision。

## 3. H-E-06 实施合同

H-E-06 必须复用 Stage 15 已冻结的纯白纹理判定和建议语义，不在宿主重新发明第二套阈值。
预检结果必须绑定：

- `sceneHandle`；
- `sceneRevision`；
- 当前有效 Profile 的 `contentHash`。

模型、场景 revision 或 Profile hash 任一变化后，旧结果必须丢弃并回到待预检状态。预检只
提供保守告警和配置建议，不修改模型、不自动切换材料策略，也不绕过生产包材料闭合校验。

## 4. 验证门禁

| 门禁 | 结果 |
|---|---|
| Debug 批量导入、失败原子性、UI smoke、源码尺寸 | PASS（3/3） |
| Release 批量导入、失败原子性、UI smoke、源码尺寸 | PASS（3/3） |
| H-E-06 身份过期、白区正负例、切片前阻断 | READY / NOT RUN |

H-E-02 已完成，H-E-06 可按本文合同进入实现。
