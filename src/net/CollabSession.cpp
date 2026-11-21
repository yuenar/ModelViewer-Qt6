#include "CollabSession.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>

namespace {
constexpr int kMaxBufferedBytes = 8 * 1024 * 1024;
}

CollabSession::CollabSession(QObject* parent) : QObject(parent) {}

CollabSession::~CollabSession() {
    m_shuttingDown = true;

    // 析构阶段不做信号通知与延迟删除，交给 QObject 父子关系清理
    if (m_server) {
        m_server->close();
    }
    if (m_client) {
        m_client->abort();
    }
    for (QTcpSocket* socket : m_clients) {
        if (socket) {
            socket->abort();
        }
    }
}

bool CollabSession::host(quint16 port, const QString& token) {
    disconnectSession();

    m_token = token;
    m_hosting = true;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this,
            &CollabSession::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, port)) {
        setStatus(QString("Host failed: %1").arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        m_hosting = false;
        return false;
    }

    setStatus(QString("Hosting on port %1").arg(m_server->serverPort()));
    return true;
}

bool CollabSession::join(const QString& hostName, quint16 port,
                         const QString& token) {
    disconnectSession();

    m_token = token;
    m_hosting = false;
    m_client = new QTcpSocket(this);
    connect(m_client, &QTcpSocket::readyRead, this,
            &CollabSession::onReadyRead);
    connect(m_client, &QTcpSocket::stateChanged, this,
            &CollabSession::onClientStateChanged);
    m_client->connectToHost(hostName, port);
    setStatus(QString("Connecting to %1:%2...").arg(hostName).arg(port));
    return true;
}

void CollabSession::disconnectSession() {
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }

    for (QTcpSocket* socket : m_clients) {
        if (socket) {
            socket->disconnect(this);
            socket->close();
            socket->deleteLater();
        }
    }
    m_clients.clear();
    m_buffers.clear();

    if (m_client) {
        m_client->disconnect(this);
        m_client->close();
        m_client->deleteLater();
        m_client = nullptr;
    }

    m_hosting = false;
    if (!m_status.isEmpty()) {
        setStatus("Collaboration disconnected");
    }
}

bool CollabSession::isActive() const {
    if (m_server && m_server->isListening()) {
        return true;
    }
    return m_client &&
           m_client->state() == QAbstractSocket::ConnectedState;
}

void CollabSession::sendState(const QJsonObject& state) {
    if (!isActive()) {
        return;
    }

    QJsonObject message;
    message.insert("type", QString("state"));
    message.insert("token", m_token);
    message.insert("state", state);

    QByteArray payload =
        QJsonDocument(message).toJson(QJsonDocument::Compact);
    payload.append('\n');

    if (m_hosting) {
        broadcast(payload);
    } else if (m_client &&
               m_client->state() == QAbstractSocket::ConnectedState) {
        m_client->write(payload);
    }
}

void CollabSession::onNewConnection() {
    if (!m_server) {
        return;
    }

    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (!socket) {
            continue;
        }
        m_clients.append(socket);
        m_buffers.insert(socket, QByteArray());

        connect(socket, &QTcpSocket::readyRead, this,
                &CollabSession::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            removeSocket(socket);
        });

        setStatus(QString("Collaboration: %1 peer(s)").arg(m_clients.size()));
        emit peerChanged();
    }
}

void CollabSession::onReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }
    processSocket(socket);
}

void CollabSession::processSocket(QTcpSocket* socket) {
    QByteArray& buffer = m_buffers[socket];
    buffer.append(socket->readAll());

    if (buffer.size() > kMaxBufferedBytes) {
        buffer.clear();
        setStatus("Collaboration: message too large, buffer cleared");
        return;
    }

    int newline = buffer.indexOf('\n');
    while (newline >= 0) {
        const QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);

        if (!line.trimmed().isEmpty()) {
            const QJsonDocument document = QJsonDocument::fromJson(line);
            if (document.isObject()) {
                const QJsonObject message = document.object();
                if (message.value("type").toString() == "state") {
                    // 会话令牌校验（空令牌表示不启用鉴权）
                    const QString token = message.value("token").toString();
                    if (!m_token.isEmpty() && token != m_token) {
                        setStatus("Collaboration: authentication failed");
                        if (m_hosting) {
                            socket->disconnectFromHost();
                        } else if (m_client) {
                            m_client->disconnectFromHost();
                        }
                        return;
                    }

                    const QJsonObject state = message.value("state").toObject();
                    if (!state.isEmpty()) {
                        // 主机转发给其他成员，避免环形广播
                        if (m_hosting) {
                            QByteArray payload =
                                QJsonDocument(message).toJson(
                                    QJsonDocument::Compact);
                            payload.append('\n');
                            broadcast(payload, socket);
                        }
                        emit sceneStateReceived(state);
                    }
                }
            }
        }

        newline = buffer.indexOf('\n');
    }
}

void CollabSession::broadcast(const QByteArray& message,
                              QTcpSocket* exclude) {
    for (QTcpSocket* socket : m_clients) {
        if (!socket || socket == exclude) {
            continue;
        }
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->write(message);
        }
    }
}

void CollabSession::onClientStateChanged() {
    if (!m_client) {
        return;
    }

    switch (m_client->state()) {
        case QAbstractSocket::ConnectedState:
            setStatus("Collaboration connected");
            break;
        case QAbstractSocket::UnconnectedState:
            setStatus("Collaboration disconnected");
            break;
        default:
            break;
    }
}

void CollabSession::removeSocket(QTcpSocket* socket) {
    m_clients.removeAll(socket);
    m_buffers.remove(socket);
    if (socket) {
        socket->deleteLater();
    }
    if (m_server && m_server->isListening()) {
        setStatus(QString("Collaboration: %1 peer(s)").arg(m_clients.size()));
        emit peerChanged();
    }
}

void CollabSession::setStatus(const QString& text) {
    m_status = text;
    if (!m_shuttingDown) {
        emit statusChanged(text);
    }
}
