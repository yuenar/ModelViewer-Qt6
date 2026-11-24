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

#include "MobileFileHelper.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {
constexpr int kMaxRecents = 10;
}

MobileFileHelper::MobileFileHelper(QObject* parent) : QObject(parent) {
  m_recents = QSettings()
                  .value(QStringLiteral("mobile/recents"))
                  .toStringList();
}

MobileFileHelper::~MobileFileHelper() = default;

QStringList MobileFileHelper::recentModels() const {
  return m_recents;
}

void MobileFileHelper::clearRecents() {
  m_recents.clear();
  QSettings().remove(QStringLiteral("mobile/recents"));
}

void MobileFileHelper::saveToRecents(const QString& path) {
  m_recents.removeAll(path);
  m_recents.prepend(path);
  while (m_recents.size() > kMaxRecents) {
    m_recents.removeLast();
  }
  QSettings().setValue(QStringLiteral("mobile/recents"), m_recents);
}

// ── 平台实现 ───────────────────────────────────────────────────────
// Android（SAF + JNI）在 MobileFileHelper_android.cpp 中定义；
// 其他平台由 QML 侧的 FileDialog 兜底，这里只发出取消信号。
#if !defined(Q_OS_ANDROID)
void MobileFileHelper::pickModel() {
  qWarning("MobileFileHelper::pickModel is only implemented on Android; "
           "use the QML FileDialog fallback on this platform");
  emit filePickCancelled();
}
#endif
