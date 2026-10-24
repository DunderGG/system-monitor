#pragma once

#include <cstdint>

namespace verdandi::platform
{

/**
 * Lets the process at the server end of a named pipe set the foreground
 * window, if this process may set it, so that a running instance can bring
 * its window to the front when a later start asks it to (DC-142). Without
 * that permission, Windows only flashes the window's taskbar button. pipe is
 * the client end's handle, as QLocalSocket::socketDescriptor() returns it on
 * Windows. Logs at debug, and does nothing else, if the permission cannot be
 * passed on.
 */
void allowPipeServerToSetForegroundWindow(std::intptr_t pipe);

} // namespace verdandi::platform
