#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <cstdint>
#include <cwchar>
#include <limits>

namespace slicesoft::diagnostics::crash_detail {

constexpr std::uint32_t ProtocolMagic = 0x53444352;
constexpr std::uint32_t ProtocolVersion = 1;
constexpr std::size_t PathCapacity = 32768;
constexpr std::size_t HandleCount = 6;
enum HandleIndex : std::size_t { Mapping, Request, Ready, Complete, Stop, Target };
enum CaptureState : LONG { Initializing, Armed, Capturing, Succeeded, Failed };

struct SharedState {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t pointerSize;
    DWORD processId;
    DWORD threadId;
    std::uint64_t exceptionPointers;
    DWORD exceptionCode;
    DWORD systemError;
    volatile LONG state;
    wchar_t dumpPath[PathCapacity];
};

class Handle final {
public:
    Handle() = default;
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() { Reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE Get() const { return value_; }
    bool Valid() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
    void Reset(HANDLE value = nullptr) {
        if (Valid()) CloseHandle(value_);
        value_ = value;
    }
private:
    HANDLE value_{nullptr};
};

class MappedState final {
public:
    ~MappedState() { Reset(); }
    bool Map(HANDLE mapping) {
        Reset();
        data_ = static_cast<SharedState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS,
            0, 0, sizeof(SharedState)));
        return data_ != nullptr;
    }
    void Reset() { if (data_) UnmapViewOfFile(data_); data_ = nullptr; }
    SharedState* Get() const { return data_; }
private:
    SharedState* data_{nullptr};
};

inline bool ParseHandles(int argc, wchar_t** argv, std::array<HANDLE, HandleCount>& handles) {
    if (argc != 8 || std::wcscmp(argv[1], L"--monitor-v1") != 0) return false;
    for (std::size_t i = 0; i < handles.size(); ++i) {
        wchar_t* end = nullptr;
        const auto value = std::wcstoull(argv[i + 2], &end, 10);
        if (!value || !end || *end || value > std::numeric_limits<std::uintptr_t>::max()) return false;
        handles[i] = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(value));
        DWORD flags = 0;
        if (!GetHandleInformation(handles[i], &flags)) return false;
    }
    return true;
}

inline bool ValidateState(const SharedState& state, HANDLE target) {
    return state.magic == ProtocolMagic && state.version == ProtocolVersion
        && state.size == sizeof(SharedState) && state.pointerSize == sizeof(void*)
        && state.processId != 0 && GetProcessId(target) == state.processId
        && state.dumpPath[0] && state.dumpPath[PathCapacity - 1] == L'\0';
}

} // namespace slicesoft::diagnostics::crash_detail
