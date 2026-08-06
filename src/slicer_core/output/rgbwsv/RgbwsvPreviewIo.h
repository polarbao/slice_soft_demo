#pragma once

void WritePpm(
    const std::filesystem::path& path,
    const int widthPx,
    const int heightPx,
    const std::vector<std::array<std::uint8_t, 3>>& pixels,
    const api::ICancelToken* cancelToken)
{
    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error(
            "failed to write RGBWSV preview: " + path.string());
    }
    output << "P6\n" << widthPx << ' ' << heightPx << "\n255\n";
    for (std::size_t index{0U}; index < pixels.size(); ++index)
    {
        if ((index % kCancellationCheckStride) == 0U)
        {
            ThrowIfCancellationRequested(
                cancelToken,
                "preview_ppm_write");
        }
        const auto& pixel = pixels.at(index);
        output.write(
            reinterpret_cast<const char*>(pixel.data()),
            static_cast<std::streamsize>(pixel.size()));
    }
}

void WritePng(
    const std::filesystem::path& path,
    const int widthPx,
    const int heightPx,
    const std::vector<std::array<std::uint8_t, 3>>& pixels,
    const api::ICancelToken* cancelToken)
{
    std::vector<std::uint8_t> raw;
    raw.reserve(
        static_cast<std::size_t>(heightPx)
        * (static_cast<std::size_t>(widthPx) * 3U + 1U));
    for (int y{0}; y < heightPx; ++y)
    {
        ThrowIfCancellationRequested(
            cancelToken,
            "preview_png_encode");
        raw.push_back(0U);
        for (int x{0}; x < widthPx; ++x)
        {
            const auto& pixel = pixels.at(
                static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(widthPx)
                + static_cast<std::size_t>(x));
            raw.insert(raw.end(), pixel.begin(), pixel.end());
        }
    }

    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error(
            "failed to write RGBWSV preview: " + path.string());
    }
    const std::array<std::uint8_t, 8> signature{
        137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};
    output.write(
        reinterpret_cast<const char*>(signature.data()),
        static_cast<std::streamsize>(signature.size()));

    std::vector<std::uint8_t> header;
    AppendBigEndianUint32(header, static_cast<std::uint32_t>(widthPx));
    AppendBigEndianUint32(header, static_cast<std::uint32_t>(heightPx));
    header.insert(header.end(), {8U, 2U, 0U, 0U, 0U});
    WritePngChunk(output, {'I', 'H', 'D', 'R'}, header);
    WritePngChunk(output, {'I', 'D', 'A', 'T'}, EncodeStoredZlibBlocks(raw));
    WritePngChunk(output, {'I', 'E', 'N', 'D'}, {});
}

void WritePreviewImage(
    const std::filesystem::path& path,
    const int widthPx,
    const int heightPx,
    const std::vector<std::array<std::uint8_t, 3>>& pixels,
    const std::string& format,
    const api::ICancelToken* cancelToken)
{
    if (format == "png")
    {
        WritePng(
            path,
            widthPx,
            heightPx,
            pixels,
            cancelToken);
        return;
    }
    WritePpm(path, widthPx, heightPx, pixels, cancelToken);
}
