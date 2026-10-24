#include "platform/windows/foreground_window.h"

#include <windows.h>

#include <spdlog/spdlog.h>

namespace verdandi::platform
{

void allowPipeServerToSetForegroundWindow(std::intptr_t pipe)
{
    ULONG serverProcessId = 0;
    if (!::GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(pipe), &serverProcessId)) {
        spdlog::debug("GetNamedPipeServerProcessId failed with error {}", ::GetLastError());
        return;
    }
    // Fails when this process may not set the foreground window itself, as
    // when the program that started it was not in the foreground.
    if (!::AllowSetForegroundWindow(serverProcessId)) {
        spdlog::debug("AllowSetForegroundWindow for PID {} failed with error {}", serverProcessId, ::GetLastError());
    }
}

} // namespace verdandi::platform
