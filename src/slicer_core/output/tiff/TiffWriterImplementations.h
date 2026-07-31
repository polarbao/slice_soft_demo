#pragma once

#include "slicer_core/output/tiff/ITiffWriter.h"

#include <memory>

namespace slicer_core::detail
{

std::unique_ptr<ITiffWriter> CreateHandwrittenTiffWriter();
std::unique_ptr<ITiffWriter> CreateLibTiffWriter();

}  // namespace slicer_core::detail
