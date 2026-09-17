#pragma once

// 层预览写出的对外接口（F-09 第 1 步从 slicer.cpp 搬出）。
// 这四个是全部：原文件里 17 个函数中只有它们被簇外调用，
// 且调用者全是且仅是 `run_slicer`（各 1 处）。
//
// 注意与 `src/slicer_core/preview/` 区分：那里是 base 层，读【已产出的包】供 UI 预览
// （MaterialPreviewComposer / TiffLayerCache / TiffLayerSource）；本单元是 engine 层，
// 在【切片过程中】写出层预览图。两者同名不同层，CMake 分层也把它们分在两侧。

#include "slicer_core/config.h"                 // PreviewConfig
#include "slicer_core/geometry/SliceGridSpec.h"  // GridSpec
#include "slicer_core/json_value.h"              // Json::Array

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core::preview {

std::string layer_file_name(const int layer_index);

std::string canonical_preview_channel(const std::string& channel);

Json::Array write_layer_previews(
    const PreviewConfig& preview_config,
    const std::filesystem::path& package_dir,
    const GridSpec& grid,
    const int layer_index,
    const std::vector<std::uint8_t>& layer,
    const std::vector<std::uint8_t>* texture_preview_mask,
    const bool transfer_enabled);

bool should_write_preview(
    const PreviewConfig& preview, const int layer_index, const int layer_count);

}  // namespace slicer_core::preview
