# XYPAD XY 原点输出画幅补白设计

日期：2026-09-09。任务真源：`docs/codex_task/current/TASKS_XYPAD_XY原点输出画幅补白.md`。

## 原因与决策

现有画幅等于各实例最终 Raster 并集，不等于打印平台。XPAD 只处理 originX，未改变 originY，故即使 X 已补齐，底部仍缺少原点到实际输出 Ymin 的空白。真实十模型源 XYmin 为 (20.07984,1.506456) mm，实际补白还须考虑支撑/定位所贡献的 Raster 范围。

不选择“切完再重写 TIFF”：会增加第二遍 I/O，还需同时重写报告、manifest、校验身份、预览缓存，并可能形成半更新包。沿用 XPAD：几何采样完成后、生产写包之前扩大最终画幅，所有出口读取同一个新网格。

保留“补齐至 X=0”，增加“补齐至 Y=0”，可单独启用或同时启用。两项默认关闭。新增可选 JSON 布尔 `output.scenePadToOriginY`，缺省 false；UI 关闭时不注入该字段，保持旧配置 hash。旧工作区 X=true 只恢复 X，不能悄悄扩大到 Y。

## 坐标合同

每轴独立：enabled 且 origin>0 时，N=ceil(origin/pitch)，新 origin=旧 origin-N*pitch，最终范围为 (-pitch,0]；否则 N=0。像素间距、原模型像素、Z 和层序不变。
先按旧并集原点量化各实例 offset，再加整列/行 N，不能用新原点重新取整，否则半像素会漂移。两轴尺寸、offset 与总字节都执行溢出检查。

内部层按 minY-first，Y 空行放在内部缓冲前部；Writer 已统一 max_y_first，因此实际 TIFF 与结果俯视中空白出现在下侧，不能再做第二次翻转。新增通道字节均 255，preview ownership mask 为 0，既不喷白墨也不生成 T。原画幅已跨零时不重复补白，不裁剪负坐标、不解除越界检查。

## 实施边界

- 六通道：共用单轴 padding 证据与校验，X/Y 各持一份；借用、消费和流式入口保持一致。
- 七通道：LegacyTransferCanvas 扩展为双轴，原位从后向前迁移原行并填充前置空行/左空列，避免额外整层拷贝；同步 outputGrid、预览 mask、统计/报告。
- 宿主：独立复选框、有效 Profile 与 QSettings；不在 Qt 计算几何/补白。
- 不改变 RGBWSV/RGBWSVT 通道顺序、位深、极性、ABI、几何策略、T 单可见实例限制或历史包。
- 默认无压缩时新增字节约为 ((W+Nx)*(H+Ny)-W*H)*层数*通道数；耗时受缓存/磁盘/校验影响，不能将面积百分比当作总耗时增幅。
- `output` 目录名称仍保留，不能缩短为 `out`。

## 验收

全关/仅 X/仅 Y/XY 四态，正负零、各向异性 DPI、半像素 offset、溢出、统计及 mask 对照；原像素平移 Nx/Ny 索引后逐字节一致，外部行序中底部 Ny 行为空。真实十模型禁用自动定向/排版，以同参数全层比较；T 用闭合 fixture 与现有真实 03.obj 作独立路径验证。文档状态只在实际 Gate 结束后更新，不把本地软件验证称作物理打印验收。
