# TIFFVIEW 生产 TIFF 行序与旧包兼容

日期：2026-09-08。状态：用户授权的受控修订，实施与验证状态见 [任务清单](../../codex_task/current/TASKS_TIFFVIEW_生产文件与场景朝向统一.md)。

## 裁决

生成文件、结果预览和模型的生产俯视必须保持 X 向右、Y 向上。同一场景不得为适配打印设备而在切片文件里隐式镜像。打印设备方向暂不属于本次验收。

旧方案“保持 TIFF 原行序，仅翻转预览”不再适用于新生成包；旧包不原地迁移。新包沿用现有 schema 名称和材料协议，增加以下受控存储约定，不改变 SPI ABI、六/七通道顺序、uint8 或 black_is_print。

| 对象 | 新生成包 | 旧包 |
| --- | --- | --- |
| TIFF 真实首行 | 场景最大 Y | 场景最小 Y |
| TIFF Orientation | 显式 1/top-left | 缺失按 1，或显式 1 |
| TIFF ImageDescription | `RGBWSV;rowOrder=max_y_first` 或 `RGBWSVT;rowOrder=max_y_first` | 原 `RGBWSV` / `RGBWSVT`；六通道历史缺失描述仍按旧合同 |
| manifest.tiff.rowOrder | `max_y_first` | 缺失等同 `min_y_first` |
| Native Reader pixels / Writer 输入 | minY-first 内部规范 | 相同 |

因此写入端实际反转整幅面的行顺序，读取端按标签恢复内部规范；结果渲染再将内部规范转换为显示坐标。不能删除预览翻转而让旧包产生反向错误，也不能只改 Orientation=4 依赖查看器支持。瓦片以整幅面高度映射，而非独立翻转每块。

单独 TIFF 可自描述；包的 rowOrder 必须与层 TIFF 一致，未知标记、Orientation 非 1 或包/层冲突必须报错。禁止用“标签缺失可能是新包”启发式猜测。旧版七通道 Reader 可能拒绝新描述，须与 Writer 同步更新；旧版六通道 Reader 可能忽略描述而误向，不能混用旧软件解释新包的世界坐标。原有像素统计与通道 checksum 不随行翻转改变。

## 上下游边界

- Legacy、Global、Scene 与 bounded/retained 共享生产 Writer，六/七通道一致。
- 诊断 PNG/PPM 也改为 maxY-first；结果页使用 Native Reader，不依赖诊断图片。
- 本地 RIP 输入扫描及输出发布按原始文件行顺序处理，无额外镜像。需用非对称真实样本核验；不能只根据自身 Writer/Reader 往返断言一致。
- 不修改模型坐标、旋转、镜像开关、定位素材排版、层高或 X/Y DPI。不同 DPI 下像素宽高比不等于物理宽高比，这是另一项显示尺度问题，不是本次反射根因。
- TIFF 字节/hash 与旧输出不同属于预期；材料逐像素在统一世界坐标下必须零差异。旧包与新证据包必须并存，不覆盖原包或复制已失效的 RIP 产物。

## 验证门槛

独立 LibTIFF 原始行 oracle、旧包兼容、六/七通道、stripped/tiled、none/PackBits、显式 handwritten、末条带/边缘瓦片、错误元数据；生产 Writer 接线与包严格校验；真实 ND002 全层材料零漂移及实际 SPI 预览朝向一致；Release 构建与部署自检。物理打印不作 PASS 声明。
