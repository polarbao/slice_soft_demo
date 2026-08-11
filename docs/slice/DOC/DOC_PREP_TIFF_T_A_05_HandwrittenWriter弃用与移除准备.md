# DOC_PREP_TIFF_T_A_05 Handwritten Writer 弃用与移除准备

> 状态：`T-A-05A=COMPLETE` / `T-A-05B-01=COMPLETE` / `T-A-05B_REMOVAL_GATE=WAITING`
> 日期：2026-08-11
> 上游：`TASKS_TIFF_默认后端切换与对齐根治任务清单.md`

## 1. 目标

T-A-05 分为两个不可混淆的交付：

1. `T-A-05A`：对显式 handwritten 构建发出弃用告警，阻止其继续被误认为生产回滚轨道；
2. `T-A-05B`：迁移所有真实消费方后，删除 handwritten Writer、写侧内部头和构建分支。

读取侧 `tiff_io.cpp`、`TiffReadApi`、`TiffPackBitsReadInternal` 属于 `slicer_base`，不在删除范围。

## 2. 稳定周期证据

默认 LibTIFF 主轨道已经完成一次完整验收周期：

- Release TIFF contract/alignment/build-info/backend/equivalence `5/5 PASS`；
- 默认语义 Package 与 `rip_reader_test --quiet` PASS；
- Qt Runtime staging 包含 `tiff.dll`、许可证、版本和 SHA-256；
- Stage 14 能力包依赖、校验和与纯 C Host smoke PASS；
- handwritten 遗留轨道 contract、known-failure alignment、build-info `3/3 PASS`。

上述证据满足弃用告警准入，但不自动授权删除文件。

## 3. 消费方审计

| 消费方 | 当前事实 | T-A-05B 处置 |
|---|---|---|
| `TiffWriterFactory.cpp` | 默认 LibTIFF 已不再回退；小于 16 或非 16 倍数的 tiled 尺寸返回 `InvalidInput` | T-A-05B-01 COMPLETE |
| `tiff_writer_backend` | 已把 8x4 回退断言改为 fail-closed、既有目标保留和无临时文件断言；仍直接创建 handwritten 做迁移期正向对照 | T-A-05B-01 COMPLETE；直接创建部分留给 T-A-05B-02 |
| `tiff_writer_equivalence` | 逐组合比较两个 Writer | 在删除前保留最后一次对照证据；删除后由语义 Golden + strict Reader 接管 |
| `tiff_backend_build_info` | 断言 `handwrittenAvailable=true` | 移除字段或冻结为 false 前必须审查诊断 JSON 消费方 |
| 03D/03E 历史脚本 | 构建并比较 handwritten 车道 | 转为历史证据脚本或显式报“后端已移除”，不得继续作为当前 Gate |
| 样例与生产配置 | tiled 样例使用 256x256；默认也是 256x256 | 当前没有仓库内生产配置依赖非标准 tile |

## 4. T-A-05A 实施合同

- `SLICESOFT_TIFF_BACKEND=handwritten` 配置成功但输出 CMake `DEPRECATION`；
- 告警必须明确“仅迁移证据、禁止生产”；
- 默认 `libtiff` 配置不得输出 handwritten 弃用告警；
- 不删除代码、不改变 TIFF 协议、不改变默认压缩。

### 4.1 验证结果（2026-08-11）

- `cmake --preset slicesoft-main`：配置成功，未出现 handwritten 弃用告警；
- `cmake --preset slicesoft-handwritten-legacy`：配置成功并出现预期 `DEPRECATION` 告警；
- 默认 LibTIFF Release TIFF Gate：`5/5 PASS`；
- handwritten 遗留 Release Gate：`3/3 PASS`，其中奇偏移已知失败继续由 `WILL_FAIL` 捕获。

## 5. T-A-05B 准入条件

只有以下条件全部满足，才可执行破坏性删除：

1. 非标准 tiled 请求改为稳定错误，并有错误码/原子发布回归；
2. 直接 handwritten 测试和历史脚本完成迁移；
3. `handwrittenAvailable` 诊断字段处置完成合同审查；
4. 默认 LibTIFF 再跑完整 Release、Runtime、能力包和 RIP strict；
5. 用户明确确认删除 Writer 源文件和遗留 preset。

### 5.1 T-A-05B-01 验证结果（2026-08-11）

- 默认 LibTIFF `tiff_writer_backend_unit_tests`：PASS；
- handwritten 遗留 `tiff_writer_backend_unit_tests`：PASS；
- 8 x 4 tiled 请求在默认轨道稳定返回 `TiffWriterErrorCode::InvalidInput`；
- 失败前已有目标文件保持不变，且没有遗留 `.tmp.*` 文件。

## 6. 结论

`T-A-05A` 与非标准 tiled 回退迁移 `T-A-05B-01` 已完成。下一张非破坏性卡为
`T-A-05B-02`；最终删除 `T-A-05B-03` 仍为 `WAITING`，不得用“默认已经是 LibTIFF”
代替消费方迁移和删除确认。
