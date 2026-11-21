// Copyright 2026 ModelViewer Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MODELVIEWER_SRC_NET_COLLABSESSION_H_
#define MODELVIEWER_SRC_NET_COLLABSESSION_H_

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

/// @brief 局域网实时协作会话（TCP + 换行分隔 JSON）
///
/// 任意一端都可以作为主机监听端口，其他实例加入后双方广播场景状态。
/// 协议：每行一个 JSON，形如 {"type":"state","state":{...}}。
class CollabSession : public QObject {
  Q_OBJECT

 public:
  explicit CollabSession(QObject* parent = nullptr);
  ~CollabSession() override;

  bool host(quint16 port);
  bool join(const QString& hostName, quint16 port);
  void disconnectSession();

  bool isActive() const;
  bool isHosting() const { return m_hosting; }
  QString statusText() const { return m_status; }

  void sendState(const QJsonObject& state);

 signals:
  void sceneStateReceived(const QJsonObject& state);
  void statusChanged(const QString& text);
  void peerChanged();

 private slots:
  void onNewConnection();
  void onReadyRead();
  void onClientStateChanged();

 private:
  void broadcast(const QByteArray& message, QTcpSocket* exclude = nullptr);
  void processSocket(QTcpSocket* socket);
  void removeSocket(QTcpSocket* socket);
  void setStatus(const QString& text);

  QTcpServer* m_server = nullptr;
  QTcpSocket* m_client = nullptr;
  QList<QTcpSocket*> m_clients;
  QHash<QTcpSocket*, QByteArray> m_buffers;
  bool m_hosting = false;
  bool m_shuttingDown = false;
  QString m_status;
};

#endif  // MODELVIEWER_SRC_NET_COLLABSESSION_H_
