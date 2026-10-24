#include "instance_server.h"

#include <utility>

#include <QByteArray>
#include <QDeadlineTimer>
#include <QLocalSocket>
#include <QObject>
#include <spdlog/spdlog.h>

#include "platform/windows/foreground_window.h"

namespace verdandi::app
{

namespace
{

// The request and its answer, one line each.
constexpr char kBringToFrontRequest[] = "bring to front\n";
constexpr char kBroughtToFrontReply[] = "brought to front\n";

// The milliseconds left until deadline, as QLocalSocket's waits take them.
int remainingMilliseconds(const QDeadlineTimer& deadline)
{
    return static_cast<int>(deadline.remainingTime());
}

} // namespace

InstanceServer::InstanceServer(std::function<void()> bringToFront) : m_bringToFront(std::move(bringToFront))
{
    QObject::connect(&m_server, &QLocalServer::newConnection, &m_server, [this] {
        while (QLocalSocket* socket = m_server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [this, socket] { answer(*socket); });
            // The request can arrive with the connection.
            answer(*socket);
        }
    });
}

std::optional<std::string> InstanceServer::listen(const QString& name)
{
    // Windows' default security for a named pipe lets every user open it for
    // reading; only this user may open this one.
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(name)) {
        return m_server.errorString().toStdString();
    }
    return std::nullopt;
}

bool InstanceServer::askToBringToFront(const QString& name, std::chrono::milliseconds timeout)
{
    const QDeadlineTimer deadline(timeout);
    QLocalSocket socket;
    // On Windows, connectToServer() has connected, or failed, when it returns.
    socket.connectToServer(name);
    if (socket.state() != QLocalSocket::ConnectedState) {
        return false;
    }
    // Before the request, so that the server may take the foreground when it
    // answers.
    platform::allowPipeServerToSetForegroundWindow(socket.socketDescriptor());

    // The write starts at once. waitForBytesWritten() is not called, because
    // it returns false once the write has finished.
    socket.write(kBringToFrontRequest);
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(remainingMilliseconds(deadline))) {
            return false;
        }
    }
    return socket.readLine() == kBroughtToFrontReply;
}

void InstanceServer::answer(QLocalSocket& socket)
{
    if (!socket.canReadLine()) {
        return;
    }
    if (socket.readLine() == kBringToFrontRequest) {
        spdlog::info("Another start of Verdandi asked this one to bring its window to the front");
        m_bringToFront();
        socket.write(kBroughtToFrontReply);
    } else {
        spdlog::debug("Ignored a request to the instance server that it does not know");
    }
    socket.disconnectFromServer();
}

} // namespace verdandi::app
