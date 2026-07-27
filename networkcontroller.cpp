#include "networkcontroller.h"
#include <QDebug>

NetworkController::NetworkController(AppMode mode, QObject *parent)
    : QObject(parent), m_mode(mode), m_server(nullptr), m_clientSocket(nullptr),
      m_heartbeatTimer(new QTimer(this))
{
    connect(m_heartbeatTimer, &QTimer::timeout, this, &NetworkController::onHeartbeatTimeout);
    m_heartbeatTimer->start(10000);
}

void NetworkController::startServer(quint16 port)
{
    m_server = new QTcpServer(this);
    if (m_server->listen(QHostAddress::Any, port)) {
        connect(m_server, &QTcpServer::newConnection, this, &NetworkController::onServerNewConnection);
        qDebug() << "[Net] Server started on port" << port;
    } else {
        emit error("Failed to start server: " + m_server->errorString());
    }
}

void NetworkController::connectToServer(const QString &ip, quint16 port)
{
    m_clientSocket = new QTcpSocket(this);
    connect(m_clientSocket, &QTcpSocket::connected, this, &NetworkController::onClientConnected);
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &NetworkController::onClientReadyRead);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &NetworkController::onClientDisconnected);
    connect(m_clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            [this](QAbstractSocket::SocketError) { emit error(m_clientSocket->errorString()); });
    m_clientSocket->connectToHost(ip, port);
}

bool NetworkController::isConnected() const {
    return m_mode == AppMode::Server ? (m_server && m_server->isListening())
                                     : (m_clientSocket && m_clientSocket->state() == QTcpSocket::ConnectedState);
}

void NetworkController::broadcastConfig(const QByteArray &configData)
{
    if (!m_server) return;
    for (QObject *obj : m_server->findChildren<QTcpSocket*>()) {
        QTcpSocket *sock = qobject_cast<QTcpSocket*>(obj);
        if (sock && sock->state() == QTcpSocket::ConnectedState) {
            sock->write(configData);
        }
    }
}

void NetworkController::onServerNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        m_buffers[socket] = QByteArray();
        m_states[socket] = {QDateTime::currentMSecsSinceEpoch(), 0, false};
        connect(socket, &QTcpSocket::readyRead, this, &NetworkController::onServerClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &NetworkController::onServerClientDisconnected);
    }
}

void NetworkController::onServerClientReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    m_states[socket].lastActivity = QDateTime::currentMSecsSinceEpoch();
    QByteArray &buf = m_buffers[socket];
    buf.append(socket->readAll());
    QJsonObject obj;
    while (parsePacket(buf, obj)) {
        if (obj["type"] == "pong") { m_states[socket].pingPending = false; continue; }
        qDebug() << "[NetServer] Received:" << obj;
    }
}

void NetworkController::onServerClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) { m_buffers.remove(socket); m_states.remove(socket); socket->deleteLater(); }
}

void NetworkController::onClientConnected() { qDebug() << "[NetClient] Connected to server."; }

void NetworkController::onClientReadyRead()
{
    if (!m_clientSocket) return;
    m_buffers[m_clientSocket].append(m_clientSocket->readAll());
    QJsonObject obj;
    while (parsePacket(m_buffers[m_clientSocket], obj)) {
        if (obj["type"] == "ping") {
            QJsonObject pong; pong["type"] = "pong";
            m_clientSocket->write(serializePacket(pong));
        } else if (obj["type"] == "config") {
            emit configReceived(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        }
    }
}

void NetworkController::onClientDisconnected()
{
    if (m_clientSocket) { m_buffers.remove(m_clientSocket); m_clientSocket->deleteLater(); m_clientSocket = nullptr; }
}

void NetworkController::onHeartbeatTimeout()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QTcpSocket*> staleClients;

    if (m_mode == AppMode::Server) {
        // --- СЕРВЕРНАЯ ЛОГИКА: проверяем всех подключенных клиентов ---
        for (QObject *obj : m_server->findChildren<QTcpSocket*>()) {
            QTcpSocket *sock = qobject_cast<QTcpSocket*>(obj);
            if (!sock || sock->state() != QTcpSocket::ConnectedState) continue;

            ClientState &st = m_states[sock];
            if (!st.pingPending && (now - st.lastActivity > 15000)) {
                QJsonObject pingMsg;
                pingMsg["type"] = "ping";
                sock->write(serializePacket(pingMsg));
                st.pingPending = true;
                st.pingSent = now;
            } else if (st.pingPending && (now - st.pingSent > 10000)) {
                staleClients.append(sock);
            }
        }
    } else {
        // --- КЛИЕНТСКАЯ ЛОГИКА: проверяем только соединение с сервером ---
        if (m_clientSocket && m_clientSocket->state() == QTcpSocket::ConnectedState) {
            ClientState &st = m_states[m_clientSocket];
            if (!st.pingPending && (now - st.lastActivity > 15000)) {
                QJsonObject pingMsg;
                pingMsg["type"] = "ping";
                m_clientSocket->write(serializePacket(pingMsg));
                st.pingPending = true;
                st.pingSent = now;
            } else if (st.pingPending && (now - st.pingSent > 10000)) {
                staleClients.append(m_clientSocket);
            }
        }
    }

    // --- ОБЩАЯ ЛОГИКА: отключение зависших клиентов ---
    for (QTcpSocket *sock : staleClients) {
        qWarning() << "[Heartbeat] Connection stale, disconnecting.";
        sock->disconnectFromHost();

        m_buffers.remove(sock);
        m_states.remove(sock);

        // Если это клиентский режим, сбрасываем указатель, чтобы приложение знало о разрыве
        if (m_mode == AppMode::Client && sock == m_clientSocket) {
            m_clientSocket = nullptr;
        }
    }
}

QByteArray NetworkController::serializePacket(const QJsonObject &obj)
{
    QByteArray payload;
    QDataStream outP(&payload, QIODevice::WriteOnly);
    outP.setVersion(QDataStream::Qt_5_15);
    outP << obj;

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << static_cast<quint32>(payload.size());
    packet.append(payload);

    return packet;
}

bool NetworkController::parsePacket(QByteArray &buffer, QJsonObject &outObj)
{
    if (buffer.size() < 4) {
        return false;
    }

    QDataStream in(buffer);
    in.setVersion(QDataStream::Qt_5_15);
    quint32 size;
    in >> size;

    if (buffer.size() < 4 + static_cast<int>(size)) {
        return false;
    }

    QByteArray payload = buffer.mid(4, size);
    QDataStream inP(payload);
    inP.setVersion(QDataStream::Qt_5_15);
    inP >> outObj;

    buffer.remove(0, 4 + size);
    return true;
}
