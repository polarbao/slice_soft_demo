#include "FileLogSink.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <array>
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace slicesoft::diagnostics
{
namespace
{
// The library's filename ABI stays unchanged. MSVC's filesystem::path stream
// overload opens Unicode paths without an ANSI code-page conversion.
class WideRotatingSink final : public spdlog::sinks::base_sink<std::mutex>
{
public:
    WideRotatingSink(std::filesystem::path path, std::size_t limit, std::size_t count)
        : m_path(std::move(path)), m_limit(limit), m_count(count)
    {
        Open();
    }
private:
    std::filesystem::path Archive(std::size_t index) const
    {
        auto path = m_path;
        path += "." + std::to_string(index);
        return path;
    }
    void Open()
    {
        m_file.open(m_path, std::ios::binary | std::ios::app);
        if (!m_file) throw std::runtime_error("Cannot open diagnostic log file");
        m_bytes = std::filesystem::file_size(m_path);
    }
    void Rotate()
    {
        m_file.close();
        for (std::size_t i = m_count; i > 0; --i)
        {
            const auto target = Archive(i);
            const auto source = i == 1 ? m_path : Archive(i - 1);
            if (std::filesystem::exists(target)) std::filesystem::remove(target);
            if (std::filesystem::exists(source)) std::filesystem::rename(source, target);
        }
        m_file.clear();
        Open();
    }
    void sink_it_(const spdlog::details::log_msg& message) override
    {
        const auto size = message.payload.size() + 1;
        if (m_bytes > 0 && m_bytes + size > m_limit) Rotate();
        m_file.write(message.payload.data(), static_cast<std::streamsize>(message.payload.size()));
        m_file.put('\n');
        m_file.flush();
        if (!m_file) throw std::runtime_error("Diagnostic log write failed");
        m_bytes += size;
    }
    void flush_() override { m_file.flush(); }
    std::filesystem::path m_path;
    std::ofstream m_file;
    std::uintmax_t m_bytes{0};
    std::size_t m_limit, m_count;
};

class FileBackend final
{
public:
    explicit FileBackend(const LogSessionOptions& options)
    {
        if (options.directory.empty()) throw std::runtime_error("Diagnostic directory is empty");
        std::filesystem::create_directories(options.directory);
        const std::array<const char*, 3> files{"app.log", "slicer_module.log", "rip.log"};
        for (std::size_t i = 0; i < files.size(); ++i)
        {
            auto sink = std::make_shared<WideRotatingSink>(
                options.directory / files[i], options.maximumFileBytes, options.archiveCount);
            m_loggers[i] = std::make_shared<spdlog::logger>(files[i], std::move(sink));
            m_loggers[i]->set_level(spdlog::level::trace);
            m_loggers[i]->set_error_handler([this](const std::string& error) { m_error = error; });
        }
    }
    void Write(LogChannel channel, int level, std::string_view event)
    {
        spdlog::level::level_enum severity = spdlog::level::info;
        switch (level)
        {
        case 0: severity = spdlog::level::trace; break;
        case 1: severity = spdlog::level::debug; break;
        case 2: severity = spdlog::level::info; break;
        case 3: severity = spdlog::level::warn; break;
        case 4: severity = spdlog::level::err; break;
        case 5: severity = spdlog::level::critical; break;
        default: return;
        }
        m_error.clear();
        m_loggers.at(static_cast<std::size_t>(channel))->log(
            severity, "{}", event);
        if (!m_error.empty()) throw std::runtime_error(m_error);
    }
private:
    std::array<std::shared_ptr<spdlog::logger>, 3> m_loggers;
    std::string m_error;
};
}
HostLogSink CreateFileLogSink(const LogSessionOptions& options)
{
    auto backend = std::make_shared<FileBackend>(options);
    return [backend](LogChannel channel, int level, std::string_view event)
    {
        backend->Write(channel, level, event);
    };
}
}
