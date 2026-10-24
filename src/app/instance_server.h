#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>

#include <QLocalServer>
#include <QString>

class QLocalSocket;

namespace verdandi::app
{

/**
 * The local server through which a later start of the monitor asks the
 * running one to bring its window to the front (DC-142). On Windows it is a
 * named pipe that only the current user may open. A client connects, passes
 * on its permission to set the foreground window, and sends a one-line
 * request; the server calls its handler, answers with a line of its own, and
 * disconnects. Requests are handled from the event loop of the thread that
 * owns the server, and nothing else is answered.
 */
class InstanceServer
{
public:
    explicit InstanceServer(std::function<void()> bringToFront);

    /** Listens on the server named name. Returns why it cannot, or std::nullopt. */
    [[nodiscard]] std::optional<std::string> listen(const QString& name);

    /**
     * Asks the server named name to bring its window to the front, and waits
     * up to timeout for its answer. Returns whether it answered; false if no
     * server listens on name, or if it closed or did not answer in time.
     * Blocks the calling thread, which needs no event loop.
     */
    [[nodiscard]] static bool askToBringToFront(const QString& name, std::chrono::milliseconds timeout);

private:
    void answer(QLocalSocket& socket);

    // Declared before the server, whose sockets call it, so that it outlives them.
    std::function<void()> m_bringToFront;
    QLocalServer m_server;
};

} // namespace verdandi::app
