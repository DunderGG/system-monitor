#include "platform/windows/instance_mutex.h"

#include <cstddef>
#include <format>
#include <utility>

#include <windows.h>
#include <sddl.h>

#include "platform/windows/token_information.h"
#include "platform/windows/utf8_conversion.h"

namespace verdandi::platform
{

namespace
{

// Frees memory that a Windows API allocated with LocalAlloc.
struct LocalMemoryDeleter
{
    void operator()(wchar_t* memory) const noexcept
    {
        ::LocalFree(memory);
    }
};

// Converts UTF-8 text to UTF-16 for a Windows API. Invalid UTF-8 becomes
// U+FFFD; empty text, or text that cannot be converted, an empty string.
std::wstring utf8ToWide(std::string_view text)
{
    const int length = static_cast<int>(text.size());
    const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
    if (wideLength <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), length, wide.data(), wideLength);
    return wide;
}

// The name InstanceMutex's constructor documents, or why it cannot be read.
struct InstanceName
{
    std::string name{};
    std::string problem{};
};

InstanceName readInstanceName(std::string_view application)
{
    DWORD sessionId = 0;
    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &sessionId)) {
        return {.problem = std::format("ProcessIdToSessionId failed with error {}", ::GetLastError())};
    }

    const auto tokenUser = readTokenInformation(::GetCurrentProcessToken(), TokenUser);
    if (!tokenUser) {
        return {.problem = std::format("GetTokenInformation(TokenUser) failed with error {}", ::GetLastError())};
    }
    wchar_t* rawSid = nullptr;
    if (!::ConvertSidToStringSidW(userSid(*tokenUser), &rawSid)) {
        return {.problem = std::format("ConvertSidToStringSidW failed with error {}", ::GetLastError())};
    }
    const std::unique_ptr<wchar_t, LocalMemoryDeleter> sid(rawSid);

    return {.name = std::format("{}-{}-{}", application, sessionId, wideToUtf8(sid.get()))};
}

} // namespace

void InstanceMutex::HandleCloser::operator()(void* handle) const noexcept
{
    ::CloseHandle(handle);
}

InstanceMutex::InstanceMutex(std::string_view application)
{
    InstanceName instanceName = readInstanceName(application);
    if (!instanceName.problem.empty()) {
        m_problem = std::format("Cannot read the instance's name: {}", instanceName.problem);
        return;
    }
    m_name = std::move(instanceName.name);

    // Not owned: the mutex marks the instance by existing, and Windows closes
    // the handle when the process ends, however it ends.
    const std::wstring fullName = L"Local\\" + utf8ToWide(m_name);
    HANDLE handle = ::CreateMutexW(nullptr, FALSE, fullName.c_str());
    const DWORD error = ::GetLastError();
    if (handle == nullptr) {
        if (error == ERROR_ACCESS_DENIED) {
            m_state = State::HeldElsewhere;
        } else {
            m_problem = std::format("CreateMutexW failed with error {}", error);
        }
        return;
    }

    // A handle to a mutex that already existed is closed at once, so that it
    // does not keep the other instance's mutex in existence after that
    // instance ends.
    m_handle.reset(handle);
    if (error == ERROR_ALREADY_EXISTS) {
        m_handle.reset();
        m_state = State::HeldElsewhere;
        return;
    }
    m_state = State::Created;
}

InstanceMutex::State InstanceMutex::state() const
{
    return m_state;
}

const std::string& InstanceMutex::name() const
{
    return m_name;
}

const std::string& InstanceMutex::problem() const
{
    return m_problem;
}

} // namespace verdandi::platform
