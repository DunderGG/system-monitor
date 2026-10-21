#include "platform/windows/system_process_information_reader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include <windows.h>
#include <winternl.h>

#include <spdlog/spdlog.h>

namespace verdandi::platform
{

SystemProcessInformationQuery resolveSystemProcessInformationQuery()
{
    // ntdll.dll is mapped into every process, so it never needs loading.
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        spdlog::error("GetModuleHandleW failed for ntdll.dll with error code {}", ::GetLastError());
        return nullptr;
    }

    // decltype only names the type winternl.h declares; nothing links against ntdll.lib.
    using NtQuerySystemInformationFn = decltype(&::NtQuerySystemInformation);
    const auto ntQuerySystemInformation =
        reinterpret_cast<NtQuerySystemInformationFn>(::GetProcAddress(ntdll, "NtQuerySystemInformation"));
    if (ntQuerySystemInformation == nullptr) {
        spdlog::error("GetProcAddress failed for NtQuerySystemInformation in ntdll.dll with error code {}",
                      ::GetLastError());
        return nullptr;
    }

    return [ntQuerySystemInformation](std::span<std::byte> buffer, uint32_t& returnLength) {
        ULONG length = 0;
        const NTSTATUS status = ntQuerySystemInformation(SystemProcessInformation, buffer.data(),
                                                         static_cast<ULONG>(buffer.size()), &length);
        returnLength = length;
        return static_cast<int32_t>(status);
    };
}

SystemProcessInformationReader::SystemProcessInformationReader()
    : SystemProcessInformationReader(resolveSystemProcessInformationQuery())
{}

SystemProcessInformationReader::SystemProcessInformationReader(SystemProcessInformationQuery query,
                                                               std::size_t initialBufferSize)
    : m_query(std::move(query)), m_buffer(std::clamp<std::size_t>(initialBufferSize, 1, kMaxBufferSize))
{}

std::optional<std::span<const std::byte>> SystemProcessInformationReader::read()
{
    if (!m_query) {
        m_failureLog.failure(spdlog::level::err, "NtQuerySystemInformation is unavailable; cannot enumerate processes");
        return std::nullopt;
    }

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        uint32_t returnLength = 0;
        const int32_t status = m_query(m_buffer, returnLength);

        if (status == ntstatus::kSuccess) {
            m_failureLog.success();
            const std::size_t length =
                (returnLength > 0 && returnLength <= m_buffer.size()) ? returnLength : m_buffer.size();
            return std::span<const std::byte>(m_buffer).first(length);
        }

        if (status != ntstatus::kInfoLengthMismatch && status != ntstatus::kBufferTooSmall) {
            m_failureLog.failure(spdlog::level::err,
                                 "NtQuerySystemInformation(SystemProcessInformation) failed with status 0x{:08X}",
                                 static_cast<uint32_t>(status));
            return std::nullopt;
        }

        // Without a usable reported size, double the buffer.
        const bool isSizeReported = returnLength > m_buffer.size();
        const std::size_t required = isSizeReported ? returnLength : m_buffer.size() * 2;
        if (required > kMaxBufferSize) {
            m_failureLog.failure(spdlog::level::err,
                                 "NtQuerySystemInformation(SystemProcessInformation) needs {} bytes, more than the "
                                 "{} byte limit",
                                 required, kMaxBufferSize);
            return std::nullopt;
        }
        // The headroom covers processes and threads created before the retry.
        // Doubling already leaves room.
        m_buffer.resize(std::min(isSizeReported ? required + required / 8 : required, kMaxBufferSize));
    }

    m_failureLog.failure(spdlog::level::err,
                         "NtQuerySystemInformation(SystemProcessInformation) buffer was still too small after {} "
                         "attempts ({} bytes)",
                         kMaxAttempts, m_buffer.size());
    return std::nullopt;
}

std::size_t SystemProcessInformationReader::bufferSize() const
{
    return m_buffer.size();
}

} // namespace verdandi::platform
