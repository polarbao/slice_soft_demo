#pragma once

#include "slicer_core/api/Cancellation.h"

#include <chrono>
#include <filesystem>
#include <string>

namespace slicesoft::tests::stage14b02
{

class CancelToken final : public slicer_core::api::ICancelToken
{
public:
    explicit CancelToken(const bool cancelled = false)
        : m_cancelled(cancelled)
    {
    }

    bool IsCancelRequested() const noexcept override
    {
        return m_cancelled;
    }

private:
    bool m_cancelled{false};
};

class TemporaryDirectory final
{
public:
    explicit TemporaryDirectory(const std::string& name)
    {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path()
            / ("slicesoft_stage14b_02_" + name + "_"
               + std::to_string(suffix));
        std::filesystem::create_directories(m_path);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& Path() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

}  // namespace slicesoft::tests::stage14b02
