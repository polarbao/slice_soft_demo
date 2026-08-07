# DOC_DECISION_RENDER 模型显示后端选型与渲染接口冻结

> 文档状态：✅ **ACTIVE / 接口冻结**（2026-08-06）
> 版本：v1.1 ｜ 日期：2026-08-06
> **任务清单：`docs/codex_task/current/TASKS_RENDER_模型显示与LOD修复补充任务清单.md`**
>
> 🔄 **v1.1 前提变更（2026-08-06）**：用户确认 **Qt 6.8+ 作为长期版本**方向。
> 由此推荐结论从「CPU 光栅为默认 + GPU 可选」修正为 **「QRhiWidget 单后端」**，
> 并含两处自我更正（WARP、像素级 golden 性价比）。详见任务清单 §5 与 §6。
> **定位：独立技术选型专项，不属于 Stage 14 任何任务组，也不占用阶段编号。**
> 14E 只**消费**本文冻结的 `IRenderBackend` 接口，不参与后端选型。
> 上游事实：`DOC_SCHEMA_14_SceneViewData网格DTO规格.md` v1.2、`DOC_DECISION_14_UI_宿主模拟改造专项.md`
> 证据等级：A=已核实事实，B=目标设计，P=判断，E=估算

---

## 1. 为什么单独立项

渲染后端选型与 Stage 14 的交付目标（能力包封装 + 打印软件集成）**没有依赖关系**：

```text
Stage 14 要交付的是   slicer_module.dll + slicer_worker.exe + 契约
渲染后端影响的是      宿主怎么把 ViewData 画出来
两者通过 IRenderBackend 解耦 —— 后端换谁，DLL 与契约一个字节都不变
```

把选型压进 14E 会产生两个坏后果：**其一**，14E 的前置本已是 M-MVP，再叠加后端选型会让 UI 卡在一个与它无关的决策上；**其二**，后端选型牵扯 Qt 版本升级、第三方许可等长周期问题，不该阻塞 Stage 14 收口。

**因此：本文只冻结接口，不锁定实现。** 实施时机与后端选择可延后，且可随时更换而不影响任何已冻结的对外契约。

## 2. 先量化负载 —— 这是全部结论的基础（E）

选型讨论最容易犯的错是跳过这一步直接比库。本项目的实际参数：

```text
幅面        230 × 100 × 60 mm
实例上限    22（13B-03 的 11×2 规则排版）
模型类型    甲片，单个约 20 × 12 mm
```

整板俯视、视口按 1200×800 估：

```text
缩放           ≈ 5.2 px/mm
单甲片占屏     ≈ 104 × 62 px ≈ 6400 像素
22 片总占屏    ≈ 142k 像素  =  视口的【15%】
若单片 50k 三角 → 平均每三角 ≈ 0.13 像素  ← 【严重 sub-pixel】
```

**三条推论决定了后续一切判断：**

| 推论 | 说明 |
|---|---|
| **① GPU 在此场景优势有限** | sub-pixel 三角每个至少占一个 2×2 quad → 50k 三角产生 ~200k 片元着色，而有效像素仅 6400，**约 97% 是 quad overdraw**。GPU 最擅长的大三角填充完全用不上 |
| **② 真正的成本在纹理，不在几何** | 单张 2048² RGBA = 16 MB。多模型多张图 → 几十 MB 的传输、解码、上传。**首帧延迟与内存峰值才是瓶颈** |
| **③ LOD 策略的收益 >> 后端选择的收益** | 整板俯视用 lod2（10k 面）绰绰有余，只有放大到单片检查时才需要 lod0。这一条做对，后端选谁差别都不大 |

## 3. 方案全集（不设第三方限制）

### 3.1 渲染后端层

| 方案 | 依赖 | 对本场景评价 |
|---|---|---|
| **QPainter 2D** | Qt 自带 | ✅ **俯视最优解**。`surfacePreview` 本就是一张图，画 22 个带变换的贴图四边形不需要 3D 管线 |
| **自研 CPU 软光栅** | 无 | ✅ 确定性；工作量 10–18 人日 |
| **QOpenGLWidget** | Qt 自带 | 🟡 Qt5 唯一 GPU 选项；OpenGL 驱动在工控机上不可靠 |
| **QRhiWidget** | Qt **6.7+** 自带 | ✅ Qt6 下 GPU 最优；Windows 走 D3D11，驱动可靠性远高于 OpenGL |
| **bgfx** | 第三方 | 🟡 能力足够，但 Qt6 下被 QRhi 覆盖且多一层依赖；参考实现定位下要求打印侧同步引入 |
| **Diligent Engine / sokol_gfx** | 第三方 | 🟡 同上，无额外优势 |
| **原生 D3D11 / D3D12** | 系统 | ❌ 上限最高但全部生命周期自维护；对 22 甲片是过度设计 |
| **Filament / Ogre / OSG** | 第三方 | ❌ 为 PBR / 大场景设计，对"精确纹理 + 排版"过重 |
| **VTK** | 第三方 | 🟡 自带拾取/测量/切片可视化，功能最全；但体量大、UI 风格陈旧、与 Qt Widgets 集成偏重 |
| **Magnum** | 第三方 | 🟡 模块化、比 bgfx 高一层、自带 scene graph 与 mesh 工具；若确定要引第三方，它比 bgfx 更贴合 |

### 3.2 🔑 放开第三方后，收益最大的其实不在渲染后端

这是本文最重要的判断（P）。按实际收益排序：

#### 第一位 · `meshoptimizer`（模块侧，非 UI 侧）

| | |
|---|---|
| 解决什么 | ViewData 契约定义了 `lod0/lod1/lod2`，但**从未定义怎么生成**。这是当前契约里最大的实现空洞 |
| 能力 | 网格简化（生成 LOD）、顶点缓存优化、**顶点量化**（float32x3 → 16-bit 归一化，体积减半）、索引编码压缩 |
| 直接收益 | 缓解 `maxBytes` 预算压力、减小 blob 传输量、减少分块次数 —— **直接服务已冻结的契约** |
| 依赖成本 | MIT，小体量，无外部依赖 |

> 这条价值高于任何渲染后端选择，因为它作用在**推论②（纹理与传输是瓶颈）**上，而不是作用在几何填充上。

#### 第二位 · 软件 OpenGL 回退（`Mesa llvmpipe` / `SwiftShader`）

**这一条修正了我此前的一个推荐。** 我先前把"工控机无独显"作为自研 CPU 光栅器的主要理由之一 —— 但 llvmpipe / SwiftShader 是工业级软件实现，**GPU 缺失时自动回退，无需写两套渲染代码**。

```text
写一套 QOpenGLWidget / QRhiWidget 代码
  有 GPU  → 硬件加速
  无 GPU  → 加载 llvmpipe / SwiftShader，仍可运行
```

代价：需随包分发（体积增加）；性能低于自研针对性优化的光栅器。

> ⚠️ **但确定性这条它替代不了**：llvmpipe 跨版本、跨 CPU（SIMD 路径不同）不保证逐像素一致。若要做像素级 golden 回归，仍需自研确定性光栅器或锁定 llvmpipe 版本 + 固定 CPU 特性集。

#### 第三位 · `Embree`（CPU 射线求交，用于拾取）

拾取（重叠模型点选）用 CPU 射线求交比 GPU ID-buffer 更精确，且**与渲染后端完全解耦** —— 换后端不影响拾取正确性。对"22 片密排、边缘重叠"的场景是实打实的收益。

#### ⚠️ 明确不建议：BC7/BC3 纹理压缩

看似能把 16 MB 降到 4 MB，但 **BC 是有损压缩**，而 14E-04d 的硬需求之一是「**白/近白纹理对比辅助**」。有损压缩在近白区引入的块伪影会直接干扰这项判断。

**纹理走无损路径（PNG 传输 + RGBA8 上传），不要为省显存牺牲颜色准确性。**

## 4. 决策树

```text
是否因【安全与长期支持】升级 Qt6？（Qt 5.15 开源补丁已停在 5.15.2）
│
├─ 否 → 留 Qt 5.15
│        俯视 QPainter ＋ 3D 自研 CPU 软光栅
│        可选：llvmpipe 作为 QOpenGLWidget 的无 GPU 回退
│
└─ 是 → 升到【6.7 以上】，不要停在 6.5 LTS（QRhi 在 6.5 仍是私有 API）
         俯视 QPainter ＋ 3D QRhiWidget（D3D11）
         保留 CPU 软光栅作为 fallback 与 golden 基准

无论哪条，都应引入：
  meshoptimizer（模块侧 LOD 生成与顶点量化）  ← 优先级最高
  Embree（拾取）                              ← 可选，重叠密排场景收益明显
不建议引入：
  bgfx / Filament / VTK / 原生 D3D            ← 对本负载过度设计
  BC 纹理压缩                                 ← 与白区对比需求冲突
```

---

# 5. 🔒 `IRenderBackend` 接口冻结

**以下接口自 2026-08-06 冻结。** 后端实现可随时增删替换，接口不变。

## 5.1 设计原则

```text
① 资源键【直接复用 ViewData 契约的 identity】，不另造一套 ID 系统
② 上传幂等：同 identity 重复上传直接返回，不重传
   —— 这是「worldMatrix 变化不使网格失效」在渲染侧的落地
③ 接口中不出现任何后端类型（无 GL / D3D / Qt / bgfx 类型泄漏）
④ 拾取与渲染分离：允许 CPU 后端用射线求交、GPU 后端用 ID buffer
⑤ 显式声明确定性：决定该后端能否用于像素级 golden 回归
```

## 5.2 接口定义

```cpp
// apps/common/render/IRenderBackend.h
// 纯 STL + 基础类型，不依赖 Qt、不依赖任何图形 API

namespace slicer::render {

// ---------- 能力自述 ----------
struct BackendCaps {
    std::string  backendId;          // "cpu_raster" / "qrhi_d3d11" / "qopengl" ...
    bool         deterministic;      // true 才可用于像素级 golden 回归
    bool         supportsMsaa;
    bool         supportsIdPicking;  // false 时 Pick() 走 CPU 射线求交
    std::uint32_t maxTextureSizePx;
    std::uint64_t vramBudgetBytes;   // 0 = 未知/不适用（CPU 后端）
};

// ---------- 资源描述（identity 来自 ViewData 契约）----------
struct MeshDesc {
    std::string  meshIdentity;       // "mesh:model-a:lod1:9f3ac21b"
    std::uint32_t vertexCount;
    std::uint32_t triangleCount;
    const void*  position;           // float32x3, little_endian
    const void*  normal;             // float32x3
    const void*  texcoord0;          // float32x2，可为 nullptr
    const void*  index;              // uint16 或 uint32
    bool         indexIs32Bit;
};

struct TextureDesc {
    std::string  textureIdentity;    // "texture:sha256:6d9f..."
    std::uint32_t widthPx;
    std::uint32_t heightPx;
    const void*  rgba8;              // rgba8_unorm / srgb / straight alpha
    bool         rowOriginTopLeft;   // 与契约 rowOrigin 对齐
};

struct MaterialDesc {
    std::string  appearanceIdentity;
    std::string  materialId;
    float        baseColorFactor[4];
    std::string  baseColorTextureIdentity;  // 空 = 无纹理
    std::string  alphaMode;                 // "opaque" | "mask" | "blend"
    float        alphaCutoff;
    bool         doubleSided;
    float        uvTransform[9];            // 3x3 行主序
};

// ---------- 帧描述 ----------
enum class ViewMode { Top, ThreeD };
enum class Projection { Orthographic, Perspective };

struct CameraDesc {
    Projection projection;           // Top 模式【必须】为 Orthographic
    float      viewMatrix[16];       // 行主序
    float      projMatrix[16];
};

struct InstanceDraw {
    std::string  instanceId;
    std::string  meshIdentity;       // Top 模式可为空 → 走 surfacePreview 四边形
    std::string  previewTextureIdentity;  // Top 模式使用
    float        localBoundsMm[4];        // Top 模式的预览四边形 XY 边界
    std::string  appearanceIdentity;
    float        worldMatrix[16];    // 行主序
    bool         selected;
    bool         outOfBuildVolume;   // 越界高亮，判定来自模块，后端只负责画
};

struct SceneDecorDesc {
    bool  showBuildVolume;
    float buildVolumeMm[3];          // X, Y, Z；Z=0 表示未定义则只画平面
    bool  showGrid;
    float gridMinorMm;               // 1 mm
    float gridMajorMm;               // 10 mm
    bool  showAxes;
    bool  showWhiteContrastAid;      // 白/近白纹理对比辅助
};

struct FrameDesc {
    ViewMode                   viewMode;
    std::uint32_t              viewportWidthPx;
    std::uint32_t              viewportHeightPx;
    CameraDesc                 camera;
    std::vector<InstanceDraw>  instances;
    SceneDecorDesc             decor;
};

struct FrameResult {
    bool          ok;
    std::string   errorCode;         // 稳定错误码，空表示成功
    double        cpuMs;
    double        gpuMs;             // CPU 后端为 0
    std::uint32_t drawCallCount;
};

struct PickResult {
    bool        hit;
    std::string instanceId;
    float       worldPosMm[3];
};

struct ImageOut {                    // headless 渲染，供 golden 回归
    std::uint32_t              widthPx;
    std::uint32_t              heightPx;
    std::vector<std::uint8_t>  rgba8;
};

// ---------- 接口 ----------
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual BackendCaps Caps() const = 0;

    // 幂等：identity 已存在则直接返回 true，不重传
    virtual bool UploadMesh(const MeshDesc&) = 0;
    virtual bool UploadTexture(const TextureDesc&) = 0;
    virtual bool UploadMaterial(const MaterialDesc&) = 0;

    // 传入当前存活的 identity 全集，后端按 LRU 回收其余资源
    virtual void ReleaseUnused(const std::vector<std::string>& liveIdentities) = 0;

    virtual FrameResult RenderFrame(const FrameDesc&) = 0;

    // 离屏渲染。Caps().deterministic == true 时结果必须逐像素可复现
    virtual bool RenderToImage(const FrameDesc&, ImageOut& out) = 0;

    virtual PickResult Pick(const FrameDesc&, int xPx, int yPx) = 0;
};

}  // namespace slicer::render
```

## 5.3 冻结条款

| 编号 | 条款 |
|---|---|
| **R-C1** | 资源键**只能**是 ViewData 契约的 `meshIdentity` / `textureIdentity` / `appearanceIdentity`。禁止后端自建 ID 体系 |
| **R-C2** | `UploadMesh` / `UploadTexture` / `UploadMaterial` **必须幂等**。同 identity 重复调用不得重传、不得重新分配 |
| **R-C3** | 接口**不得出现**任何图形 API 或 Qt 类型。违反即视为后端泄漏 |
| **R-C4** | `ViewMode::Top` 时 `camera.projection` **必须** `Orthographic`。透视会使越界与对齐判断产生视觉误判 |
| **R-C5** | `outOfBuildVolume` 等**判定结果由模块给出**，后端只负责绘制，**不得自行判定** |
| **R-C6** | `Caps().deterministic == true` 的后端，`RenderToImage` 同输入必须逐像素一致，否则不得声明为 true |
| **R-C7** | 后端**不得**修改任何 ViewData 内容，也不得影响切片输出 |
| **R-C8** | `RenderFrame` 与 `Pick` 均**不得**发起任何跨 DLL 调用 —— 这是 UI-M1 / UI-M7 成立的前提 |

## 5.4 首批实现约定

| 实现 | `backendId` | `deterministic` | 定位（v1.1 修订）|
|---|---|---|---|
| **QRhiWidget（Qt 6.8+）** | `qrhi_d3d11` | false | ✅ **首实现，生产默认路径** |
| CPU 软件光栅 | `cpu_raster` | true | ⏸ **DEFERRED** —— 仅在出现逐像素审计硬需求时实施 |
| QOpenGLWidget（Qt5）| `qopengl` | false | ❌ 不推荐（Qt 官方方向已转 QRhi；OpenGL 驱动为已知薄弱点）|

**v1.1 修订说明**：v1.0 曾要求「至少实现 `cpu_raster`」。该要求已撤销，两条依据：

```text
① Windows 内置 WARP（D3D11 软件光栅器），无 GPU 时可用系统能力兜底，
   无需额外分发 llvmpipe/SwiftShader，也无需自研 CPU 光栅 —— CPU 后端的
   主要【生产】理由消失
② 像素级 golden 回归可用「FrameDesc 输入确定性断言 + 输出图像容差比对」
   替代，约 2 人日取得 90% 价值，不值 10–18 人日自研光栅器
```

接口本身不变 —— 抽象成本近零，将来需要 CPU 后端可直接加实现，14E 一行不改。
完整论证见任务清单 §5。

---

## 6. 与 Stage 14 / 14E 的边界

```text
本专项负责    后端选型、IRenderBackend 契约、后端实现
14E 负责      调用 IRenderBackend；相机与交互逻辑；三车道；能力覆盖
              —— 14E【不选后端】，只面向接口编程

DLL / Worker / ViewData 契约 —— 本专项【零影响】
```

14E 的验收指标（UI-M1..M13）对任何后端一视同仁；`UI-M8` 的帧率门由具体后端在实施时各自满足。

> **本专项不阻塞 Stage 14 收口。** 即使后端一个都没实现，14E 也可以先用 `cpu_raster` 的最小骨架跑通接口。

## 7. 待定项（不阻塞接口冻结）

| 编号 | 待定 | 决策人 |
|---|---|---|
| RD-01 | 是否因安全与长期支持升级 Qt6（决定 GPU 路径走 QRhiWidget 还是 QOpenGLWidget）| 用户 / 产品 |
| RD-02 | 是否引入 `meshoptimizer`（**推荐引入**，收益最高）| 用户 |
| RD-03 | 是否引入 `Embree` 做拾取（可选）| 用户 |
| RD-04 | 是否随包分发 llvmpipe / SwiftShader 作为无 GPU 回退 | 部署侧 |

RD-02 与其余三项无关，**可以单独先定** —— 它作用在模块侧 LOD 生成，与 UI 后端完全解耦。

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-06 | v1.0 | 首版。独立立项，脱离 14E；量化工作负载并给出三条推论；给出含第三方的方案全集；**提出核心判断：放开第三方后收益最大的是 `meshoptimizer`（模块侧 LOD 与顶点量化）而非渲染后端**；修正"工控机无独显"论据（llvmpipe/SwiftShader 可回退，但替代不了确定性）；明确反对 BC 纹理压缩（与白区对比需求冲突）；**冻结 `IRenderBackend` 接口与 R-C1..C8 八条条款**；登记 RD-01..04 待定项 |
