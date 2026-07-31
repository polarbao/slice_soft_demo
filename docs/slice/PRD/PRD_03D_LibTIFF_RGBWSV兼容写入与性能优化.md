# PRD_03D LibTIFF RGBWSV 兼容写入与性能优化

> 文档状态：READY FOR IMPLEMENTATION REVIEW
> 日期：2026-07-31
> 优先级：P0

## 1. 目标

在不改变 SliceSoft 生产 TIFF 协议和材料数据的前提下，引入 LibTIFF Writer，降低
TIFF 写入实现维护风险，并通过独立基准判断能否缩短保存时间和降低临时内存。

## 2. 用户价值

```text
同一切片结果在新旧后端中具有相同的六通道生产含义；
保存阶段耗时可独立观察、可量化比较；
依赖或新后端异常时能够回退，不丢失可用生产路径；
运行包包含完整依赖，不要求用户手工配置 DLL。
```

## 3. 功能需求

### FR-03D-01 双后端

迁移期必须同时支持：

```text
handwritten；
libtiff。
```

后端选择首先是构建/测试能力，不作为普通工艺用户的材料配置。

### FR-03D-02 协议等价

对相同 layer buffer，两后端必须产生等价的：

```text
width/height；
六通道像素值；
bit depth/sample format；
stripped/tiled 布局语义；
rowsPerStrip/tile 尺寸；
必要 TIFF tag；
RIP Reader 统计和校验结果。
```

文件 SHA-256 不要求相同，因为合法 TIFF 的 tag 顺序和 offset 可不同。

### FR-03D-03 原子失败

任一 TIFF 写入、关闭或 package 校验失败时：

```text
不得发布残缺 package；
不得留下被 manifest 引用的半文件；
必须返回稳定错误码和可诊断信息。
```

### FR-03D-04 性能反馈

专项报告必须拆分：

```text
writerOpenMs；
tagSetupMs；
pixelWriteMs；
writerCloseMs；
tiffWriteMs；
bytesWritten；
peakWorkingSetBytes；
backend；
storageMode。
```

### FR-03D-05 Runtime

Debug、Release 运行包必须携带正确版本的 LibTIFF 动态依赖，且 Runtime 自检可报告
后端和依赖版本。

## 4. 固定协议

```text
schema=p0.rgbwsv.2
R G B W S V
uint8
black_is_print
0=打印
255=不打印
PLANARCONFIG_CONTIG
COMPRESSION_NONE
Classic TIFF
single IFD
```

## 5. 非目标

```text
TIFF 压缩；
BigTIFF；
planar separate；
多页 TIFF；
新增 alpha/coverage 通道；
改变 preview 或 RIP 业务逻辑；
12G-TCWS 白色/透明策略；
切片核心算法性能优化。
```

## 6. 验收标准

1. stripped/tiled 的像素逐字节解码结果完全相同。
2. 必需 tag 与当前合同一致，未知或缺失 tag fail closed。
3. 所有现有 `rip_reader_test` 正向和负向用例通过。
4. 共享 package Writer、单模型、多模型、Legacy/Global 生产回归通过。
5. Debug/Release Runtime 可启动且不缺 DLL。
6. Writer-only Release benchmark 生成 p50/p95 和内存报告。
7. 默认切换必须满足决策文档性能 Gate；未满足时保持手写默认并诚实报告。

## 7. 发布与回滚

```text
R1-R4：LibTIFF opt-in，手写默认；
R5 GO：LibTIFF 默认，手写保留回滚；
出现协议、部署或稳定性回归：立即恢复手写默认；
回滚不修改配置文件和生产 package schema。
```
