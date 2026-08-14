#pragma once

#include "IRenderBackend.h"

#include <memory>

/**
 * @brief 不依赖图形 API 的确定性 Stage 14E 参考渲染器。
 */
class CpuRasterBackend final : public slicer::render::IRenderBackend
{
public:
    /** @brief 创建空的确定性软件渲染器。 */
    CpuRasterBackend();

    /** @brief 释放所有缓存的宿主渲染资源。 */
    ~CpuRasterBackend() override;

    CpuRasterBackend(const CpuRasterBackend&) = delete;
    CpuRasterBackend& operator=(const CpuRasterBackend&) = delete;

    /** @copydoc slicer::render::IRenderBackend::Caps */
    [[nodiscard]] slicer::render::BackendCaps Caps() const override;

    /** @copydoc slicer::render::IRenderBackend::UploadMesh */
    bool UploadMesh(const slicer::render::MeshDesc& mesh) override;

    /** @copydoc slicer::render::IRenderBackend::UploadTexture */
    bool UploadTexture(const slicer::render::TextureDesc& texture) override;

    /** @copydoc slicer::render::IRenderBackend::UploadMaterial */
    bool UploadMaterial(const slicer::render::MaterialDesc& material) override;

    /** @copydoc slicer::render::IRenderBackend::ReleaseUnused */
    void ReleaseUnused(
        const std::vector<std::string>& liveIdentities) override;

    /** @copydoc slicer::render::IRenderBackend::RenderFrame */
    [[nodiscard]] slicer::render::FrameResult RenderFrame(
        const slicer::render::FrameDesc& frame) override;

    /** @copydoc slicer::render::IRenderBackend::RenderToImage */
    bool RenderToImage(
        const slicer::render::FrameDesc& frame,
        slicer::render::ImageOut& output) override;

    /** @copydoc slicer::render::IRenderBackend::Pick */
    [[nodiscard]] slicer::render::PickResult Pick(
        const slicer::render::FrameDesc& frame,
        int xPx,
        int yPx) override;

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};
