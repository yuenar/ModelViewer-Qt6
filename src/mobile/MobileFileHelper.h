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

#ifndef MODELVIEWER_SRC_MOBILE_MOBILEFILEHELPER_H_
#define MODELVIEWER_SRC_MOBILE_MOBILEFILEHELPER_H_

#include <QObject>
#include <QString>
#include <QStringList>

/// @brief 跨平台模型文件选择器（QML 单例）
///
/// Android：SAF（Storage Access Framework）OPEN_DOCUMENT 选择文件，
///          content:// URI 无法被 Assimp / QFile 直接读取，先经
///          ContentResolver 流拷贝到应用 cache 目录再 emit fileSelected；
///          该流程不需要任何存储权限。
/// 其他平台：pickModel() 直接发出 filePickCancelled，由 QML 侧
///          （QtQuick.Dialogs 的 FileDialog）兜底，便于桌面调试 QML。
///
/// 最近打开列表持久化在 QSettings（上限 10 条）。
class MobileFileHelper : public QObject {
  Q_OBJECT
 public:
  explicit MobileFileHelper(QObject* parent = nullptr);
  ~MobileFileHelper() override;

 public slots:
  /// @brief 弹出系统文件选择器（Android SAF）
  Q_INVOKABLE void pickModel();

  /// @brief 最近打开的模型路径（最新在前）
  Q_INVOKABLE QStringList recentModels() const;

  /// @brief 清空最近打开列表
  Q_INVOKABLE void clearRecents();

 signals:
  /// @brief 选中文件（Android 上为已拷贝到 cache 的本地路径）
  void fileSelected(const QString& path);
  /// @brief 取消选择或选择失败
  void filePickCancelled();

 private:
  void saveToRecents(const QString& path);

  QStringList m_recents;
};

#endif  // MODELVIEWER_SRC_MOBILE_MOBILEFILEHELPER_H_
