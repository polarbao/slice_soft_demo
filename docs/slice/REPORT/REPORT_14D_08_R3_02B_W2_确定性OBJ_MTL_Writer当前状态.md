# REPORT 14D-08-R3-02B-W2 确定性 OBJ/MTL Writer 当前状态

> 更新日期：2026-08-06
>
> 状态：`COMPLETE`

## 1. 完成内容

- 新增项目内确定性 OBJ/MTL Writer，不引入 Assimp exporter 或 3MF Writer；
- 按索引稳定写出顶点，并按三角形角点稳定写出 UV；
- 保留三角形 material assignment、MTL diffuse 和纹理引用；
- 将纹理按稳定编号复制到 job-owned `resources/`，保持资源字节不变；
- 缺失纹理、非法材质名、无效顶点索引、取消和输出冲突均 fail-closed；
- 失败时清理由本次 Writer 创建的 OBJ、MTL 和纹理文件。

## 2. 冻结边界

- 首版只输出 OBJ；
- Writer 只负责 staging 资产写出，不负责原子发布；
- Writer 不执行 strict recheck，不授予生产准入；
- 资产发布、跨进程取消和崩溃恢复仍由 `14D-05` 与后续 Worker 子卡负责。

## 3. 验证

```text
Debug stage14d08_r3_obj_writer_tests   PASS
Release stage14d08_r3_obj_writer_tests PASS
ValidateStage14BTargetGraph            PASS
ValidateCapabilityDtos                 PASS
ValidateFileContract                   PASS
ValidateThreeLaneContract              PASS
```

验证覆盖确定性 OBJ/MTL 字节、UV/material assignment、纹理字节复制，以及缺失纹理失败清理。

## 4. 下一任务

`14D-08-R3-02B-S1`：用同一 effective Profile 重导入 staged OBJ，执行完整单模型 strict recheck，
并形成资源、属性和严格拓扑证据。
