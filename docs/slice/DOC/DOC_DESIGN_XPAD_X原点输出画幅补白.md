# XPAD X 原点输出画幅补白设计

日期：2026-09-08。任务真源：`docs/codex_task/current/TASKS_XPAD_X原点输出画幅补白.md`。

## 原因与方案

目前切片联合画幅取各实例有效 Raster 的并集，不等于完整打印平台。因此模型摆放坐标仍正确，但 TIFF 第 0 列对应并集最左侧，而非平台 X=0。用户的十个模型源 XY 并集为 `(20.07984,1.506456)..(194.0495,59.67867)` mm；实际 Raster 还包含已有支撑/定位等范围，补白必须依据实际 Raster，不能只依据源模型 bbox。

不采用切完后重写 TIFF：该方案还须重算尺寸、统计、manifest、校验摘要和预览缓存，并承担二次 I/O、失败半包与重复执行风险。采用各实例切片之后、联合画幅合成写出之前补白；复用现有全层合成、空值初始化、统计、写包和严格 Reader。无需引入第三方依赖。

## 合同

- 新增可选 Profile 字段 `output.scenePadToOriginX`，布尔，缺省 false。字段仅控制 Scene 作业（包含单实例 Scene）；独立单模型 CLI 不应用该场景画幅策略。不是 SPI ABI、Worker 格式或 TIFF 协议版本修改。
- 宿主“切片设置”增加默认关闭复选框“补齐至 X=0”。启用时写 true，并重算有效 Profile hash；关闭时不输出新字段，保持旧 Profile hash。持久化旧配置缺字段恢复 false。
- 原点是场景/打印平台 X=0，不是每个模型的局部原点。依据经过用户所有旋转/缩放/平移后的最终位置；自动定向、排版和 Z 触底规则不变。
- 仅当原联合 Raster 左边界 `x > 0` 时补左侧；`x <= 0` 不补、不裁负坐标、不移动模型，原有 buildVolume 越界门保持。Y 范围、Z 起点/层数、DPI、通道和主体数据保持。
- 保持原 Raster 像素相位：像素间距 `p=25.4/dpiX`，补列 `N=ceil(x/p)`，新 originX=`x-N*p`，新宽=`oldWidth+N`。第 N 列开始为原内容，不插值、不旋转、不镜像。新 originX 在 `(-p,0]` 内，意味着覆盖 X=0 的最多不足一像素保守余量；不伪造为精确 0，否则会改变内容物理位置。整数边界按浮点计算真实值处理，不缩减所需覆盖范围。
- 实例 placement 必须先按原联合 originX 求原整数 offset，再加 N；不能直接对新 originX 重新取整。600 DPI 半像素测试实测后一种算法会因浮点舍入令某实例多偏移一列。`SceneCanvasXPadding` 保留原 origin 和整列数，Orchestrator 与 Composer 共用该证据，并检查范围/加法溢出；旧开关关闭时不走新分支。
- 所有层新增区域 RGBWSV/RGBWSVT 各通道均为 255，不会生成白墨、支撑、光油、T 材料或新增打印层。既有 FRAME 定位范围可共存，范围已到达 X=0 时不重复扩展。RGBWSVT 采用独立单实例 run_slicer 路由，在最终七通道输出处补白，不经过六通道 Scene 合成器；T 原有单可见实例准入限制不变。
- 全层一致画幅，manifest origin/宽度、报告统计、TIFF 及结果预览均从同一合成结果生成；原实例资源/变换 hash 不变，只有启用选项的 Profile hash 和输出画幅改变。
- 溢出与非法网格 fail-closed，继续使用现有合成内存校验、取消和 staging 发布保证；不在原有 package 上做事后修改。

## 性能与交付

### T 通道追加合同（2026-09-08 用户授权，替代上述六通道限制）

RGBWSVT Scene 保持现有单可见实例准入，使用相同 `output.scenePadToOriginX` 开关。Legacy 几何 grid、T 归属计划及 V/T 冲突检查不变；独立 `LegacyTransferCanvas` 仅在七通道层已合成后、首次写盘前按整列补 255。保留原采样域，避免对空白区域重新求几何/T 所有权。

输出网格与计算网格分离。manifest、逐层宽度、slice/material process 报告、覆盖率分母、返回尺寸使用输出网格；RGBWSVT 写出后既有严格回读统计自然包含补列。诊断预览采用输出网格，纹理预览 mask 同步补 0；纹理/支撑/closure 等几何诊断仍基于原采样域，不把补列算成材料或缺口。

补列采用原层 vector 扩容后倒序移动行并填空，不保留第二份全层补白缓存，也不事后重写包。关闭、Xmin<=0 无新增逐层扫描。独立 CLI（无 Scene 实例上下文）不应用 scene 专用选项。协议、T 独占规则、8-bit、black_is_print、默认关闭和 T 原有单实例限制均不改变。

本轮用户已授权测试后按任务提交、合入 `product/packaged-slicer`、删除开发分支；这替代上一轮“不合入”交付边界。测时采用同一 Release 二进制、相同模型/工艺/600 DPI，交替开关顺序，首轮热身不计，后续三对报告中位数及波动，不以文件增加比例直接冒充总耗时增加比例。

补白只在全局合成阶段增加空列，不扩大各模型几何采样域；仍增加每层输出缓存、TIFF 写盘及校验成本。默认未压缩时额外 RGBWSV 字节近似为 `N * height * layers * 6`；不为节省空间擅改压缩默认。

开发与验证在 `codex/x-origin-canvas-padding` 隔离完成，五项拆分提交已按本轮授权快进合入 `product/packaged-slicer`，开发分支已删除，未推送；Git 记录见任务清单。T 兼容测试版部署于 `runtime/slicesoft-xpad-t/Release`，旧版 `runtime/slicesoft-xpad/Release` 为第一阶段六通道证据，不作为 T 兼容交付。现用 `runtime/slicesoft/Release` 因程序运行而被部署保护阻止覆盖，未终止用户进程。最终定向 CTest 21/22，唯一既有 adapter 失败及两组真实模型测时详见 [收口总结](../REPORT/REPORT_XPAD_X原点输出画幅补白收口总结.md)。物理打印及 GUI 人工交互未验证。
