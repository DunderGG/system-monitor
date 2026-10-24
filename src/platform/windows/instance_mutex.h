#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace verdandi::platform
{

/**
 * A named mutex whose existence marks the running instance of an application
 * for the current user in the current logon session (DC-142). The process that
 * creates it holds it until the InstanceMutex is destroyed, or until the
 * process ends, normally or not, when Windows closes its handles; the next
 * start then creates it again. Nothing waits on the mutex or acquires it: only
 * whether it exists matters.
 */
class InstanceMutex
{
public:
    /** What the constructor found. */
    enum class State
    {
        // This process created the mutex, and holds it.
        Created,
        // Another process holds it: Windows reported that it exists, or denied
        // this process access to it, as it can when an elevated instance
        // created it.
        HeldElsewhere,
        // The name could not be read, or the mutex could not be created or
        // opened; problem() says why.
        Failed,
    };

    /**
     * Creates the mutex Local\<name()>, or finds that another process holds
     * it. name() is application, the logon session's ID and the user's SID,
     * joined by hyphens, such as "Verdandi-1-S-1-5-21-...-1001". The SID tells
     * users apart, and the session ID one user's sessions, which the Local\
     * namespace does for the mutex but not for a named pipe of the same name.
     */
    explicit InstanceMutex(std::string_view application);

    [[nodiscard]] State state() const;

    /** The mutex's name without its namespace, in UTF-8; empty if it could not be read. */
    [[nodiscard]] const std::string& name() const;

    /** Why the state is Failed, with the Windows error; empty otherwise. */
    [[nodiscard]] const std::string& problem() const;

private:
    struct HandleCloser
    {
        void operator()(void* handle) const noexcept;
    };

    std::unique_ptr<void, HandleCloser> m_handle{};
    State m_state{State::Failed};
    std::string m_name{};
    std::string m_problem{};
};

} // namespace verdandi::platform
