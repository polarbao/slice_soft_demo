#pragma once

#include "IRenderBackend.h"

#include <memory>

/**
 * @brief Deterministic Stage 14E reference renderer with no graphics API.
 */
class CpuRasterBackend final : public slicer::render::IRenderBackend
{
public:
    /** @brief Creates an empty deterministic software renderer. */
    CpuRasterBackend();

    /** @brief Releases all cached host rendering resources. */
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
