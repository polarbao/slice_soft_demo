#include "slicer_core/texture_image.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#endif

namespace slicer_core {
namespace {

std::uint8_t lerp_u8(const std::uint8_t a, const std::uint8_t b, const double t) {
    const double value = static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * t;
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

double address_coord(const double input, const std::string& mode, bool& out_of_range) {
    if (input < 0.0 || input > 1.0) {
        out_of_range = true;
    }
    if (mode == "repeat") {
        double value = input - std::floor(input);
        if (value < 0.0) {
            value += 1.0;
        }
        return value;
    }
    return std::clamp(input, 0.0, 1.0);
}

std::array<std::uint8_t, 4> pixel_at(const TextureImage& image, const int x, const int y) {
    const std::size_t index =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4U;
    return {image.rgba.at(index + 0U), image.rgba.at(index + 1U), image.rgba.at(index + 2U), image.rgba.at(index + 3U)};
}

#ifdef _WIN32
template <typename T>
void release_com(T*& value) {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

struct ComInit {
    HRESULT hr{S_OK};

    ComInit() : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}

    ~ComInit() {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }

    bool usable() const {
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
};
#endif

}  // namespace

TextureImage load_texture_image(const std::filesystem::path& path) {
#ifdef _WIN32
    ComInit com;
    if (!com.usable()) {
        throw std::runtime_error("failed to initialize COM for texture loading");
    }

    IWICImagingFactory* factory{nullptr};
    IWICBitmapDecoder* decoder{nullptr};
    IWICBitmapFrameDecode* frame{nullptr};
    IWICFormatConverter* converter{nullptr};

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        throw std::runtime_error("failed to create WIC imaging factory");
    }

    const std::wstring filename = path.wstring();
    hr = factory->CreateDecoderFromFilename(
        filename.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr)) {
        release_com(factory);
        throw std::runtime_error("failed to decode texture image: " + path.string());
    }

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        release_com(decoder);
        release_com(factory);
        throw std::runtime_error("failed to read first texture frame: " + path.string());
    }

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        release_com(frame);
        release_com(decoder);
        release_com(factory);
        throw std::runtime_error("failed to create texture format converter");
    }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        release_com(converter);
        release_com(frame);
        release_com(decoder);
        release_com(factory);
        throw std::runtime_error("failed to convert texture to RGBA: " + path.string());
    }

    UINT width{0};
    UINT height{0};
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        release_com(converter);
        release_com(frame);
        release_com(decoder);
        release_com(factory);
        throw std::runtime_error("invalid texture dimensions: " + path.string());
    }

    TextureImage image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    const UINT stride = width * 4U;
    const UINT byte_count = stride * height;
    hr = converter->CopyPixels(nullptr, stride, byte_count, image.rgba.data());

    release_com(converter);
    release_com(frame);
    release_com(decoder);
    release_com(factory);

    if (FAILED(hr)) {
        throw std::runtime_error("failed to copy texture pixels: " + path.string());
    }
    return image;
#else
    (void)path;
    throw std::runtime_error("texture loading requires Windows Imaging Component on this build");
#endif
}

std::array<std::uint8_t, 3> sample_texture_rgb(
    const TextureImage& image,
    double u,
    double v,
    const TextureSampleOptions& options,
    bool& uv_out_of_range) {
    if (image.width <= 0 || image.height <= 0 || image.rgba.empty()) {
        throw std::runtime_error("cannot sample empty texture image");
    }

    u = address_coord(u, options.uv_address_mode, uv_out_of_range);
    v = address_coord(options.flip_v ? 1.0 - v : v, options.uv_address_mode, uv_out_of_range);

    const double x = u * static_cast<double>(image.width - 1);
    const double y = v * static_cast<double>(image.height - 1);

    if (options.sampler == "nearest") {
        const int xi = static_cast<int>(std::clamp(std::lround(x), 0L, static_cast<long>(image.width - 1)));
        const int yi = static_cast<int>(std::clamp(std::lround(y), 0L, static_cast<long>(image.height - 1)));
        const auto pixel = pixel_at(image, xi, yi);
        return {pixel.at(0), pixel.at(1), pixel.at(2)};
    }

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, image.width - 1);
    const int y1 = std::min(y0 + 1, image.height - 1);
    const double tx = x - static_cast<double>(x0);
    const double ty = y - static_cast<double>(y0);
    const auto p00 = pixel_at(image, x0, y0);
    const auto p10 = pixel_at(image, x1, y0);
    const auto p01 = pixel_at(image, x0, y1);
    const auto p11 = pixel_at(image, x1, y1);

    std::array<std::uint8_t, 3> result{};
    for (std::size_t channel{0}; channel < result.size(); ++channel) {
        const std::uint8_t top = lerp_u8(p00.at(channel), p10.at(channel), tx);
        const std::uint8_t bottom = lerp_u8(p01.at(channel), p11.at(channel), tx);
        result.at(channel) = lerp_u8(top, bottom, ty);
    }
    return result;
}

}  // namespace slicer_core
