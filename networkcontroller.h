#ifndef NETWORKCONTROLLER_H
#define NETWORKCONTROLLER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QTimer>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDataStream>

enum class AppMode { Server, Client };

struct ClientState {
    qint64 lastActivity = 0;
    qint64 pingSent = 0;
    bool pingPending = false;
};

class NetworkController : public QObject
{
    Q_OBJECT
public:
    explicit NetworkController(AppMode mode, QObject *parent = nullptr);
    void startServer(quint16 port = 5000);
    void connectToServer(const QString &ip, quint16 port);
    void broadcastConfig(const QByteArray &configData);
    bool isConnected() const;

    static QByteArray serializePacket(const QJsonObject &obj);
    static bool parsePacket(QByteArray &buffer, QJsonObject &outObj);

signals:
    void configReceived(const QByteArray &data);
    void error(const QString &message);

private slots:
    void onServerNewConnection();
    void onServerClientReadyRead();
    void onServerClientDisconnected();
    void onClientConnected();
    void onClientReadyRead();
    void onClientDisconnected();
    void onHeartbeatTimeout();

private:
    AppMode m_mode;
    QTcpServer *m_server;
    QTcpSocket *m_clientSocket;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QHash<QTcpSocket*, ClientState> m_states;
    QTimer *m_heartbeatTimer;
};

#endif // NETWORKCONTROLLER_H
