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

#ifndef MODELVIEWER_SRC_UI_MAINWINDOW_H_
#define MODELVIEWER_SRC_UI_MAINWINDOW_H_

#include <QMainWindow>
#include <QFutureWatcher>
#include <QStringList>
#include <QVector>
#include <QVector3D>
#include "../core/CpuMesh.h"
#include "../core/Animation.h"

class QAction;
class QActionGroup;
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QTabWidget;
class QUndoStack;
class QTimer;
class QJsonObject;
class ScriptConsole;
class RhiWidget;

/// @brief 应用程序主窗口
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  struct MeshParams {
    QVector3D translate{0, 0, 0};
    QVector3D rotate{0, 0, 0};
    QVector3D scale{1, 1, 1};

    bool operator==(const MeshParams& other) const {
      return translate == other.translate && rotate == other.rotate &&
             scale == other.scale;
    }
    bool operator!=(const MeshParams& other) const { return !(*this == other); }
  };
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override = default;

  void openModel(const QString& path);
  void setRenderMode(int mode);
  bool saveScreenshotTo(const QString& path);
  void selectMesh(int index);
  void applyViewPreset(int preset);
  void applyDisplayStyle(int style);
  void setMultiView(bool enabled);
  void setAxisVisible(bool visible);
  void setGridVisible(bool visible);
  void setToolMode(int mode);
  void createSurface(int type);
  bool exportModelTo(const QString& path);
  void setMeshTransformValues(int index, const QVector3D& translate,
                              const QVector3D& rotate, const QVector3D& scale);
  void setMeshColor(int index, const QVector3D& color);

  /// @brief 抗锯齿 / 后处理
  void setMsaa(int samples);
  void setFxaaEnabled(bool enabled);
  void setAoEnabled(bool enabled);
  QString renderStatsText() const;

  /// @brief 动画控制
  bool hasAnimation() const;
  int animationCount() const;
  void playAnimation(int index = -1);
  void pauseAnimation();
  void stopAnimation();
  void setAnimationTime(float seconds);
  QString animationName(int index) const;

  /// @brief 场景协作（JSON 导入/导出与本地同步）
  bool exportSceneState(const QString& path);
  bool importSceneState(const QString& path);

  /// @brief 脚本/插件
  bool runScriptFile(const QString& path);
  void loadPlugins();

  /// @brief 应用对象变换参数（供撤销/重做命令调用）
  void applyMeshParams(int index, const MeshParams& params);

 signals:
  void modelLoaded();

 private slots:
  void onOpenFile();
  void onFitView();
  void onResetView();
  void onToggleFullScreen();
  void onToggleProjection(bool orthographic);
  void onProjectionChanged(bool orthographic);
  void onToggleMultiView(bool enabled);
  void onToggleAxis(bool visible);
  void onToggleGrid(bool visible);
  void onMaterialSettings();
  void onLightSettings();
  void onBackgroundSettings();
  void onClippingSettings();
  void onNormalLengthSettings();
  void onScreenshot();
  void onShowModelInfo();
  void onExportModel();
  void onSaveModel();
  void onNewSurface(int type);
  void onLoadFinished();
  void onObjectSelectionChanged();
  void onObjectItemChanged(QListWidgetItem* item);
  void onTransformChanged();
  void onTransformCommitted();
  void onResetTransform();
  void onShowAllObjects();
  void onHideAllObjects();
  void onSwapVisibility();
  void onDuplicateObject();
  void onRenameObject();
  void onDeleteObject();

 private:
  struct Document {
    RhiWidget* view = nullptr;
    QString filePath;
    QString paramsSource;
    QVector<MeshParams> params;
    QVector<MeshParams> committed;
    QUndoStack* undoStack = nullptr;
  };

  void setupUI();
  void setupMenus();
  void setupObjectDock();
  int createDocument(const QString& label = "Untitled");
  void activateDocument(int index);
  void closeDocument(int index);
  void saveCurrentDocumentState();
  void loadDocumentState(int index);
  void syncUiToCurrentDocument();
  void setRenderModeInternal(int mode);
  void toggleRenderMode(int mode);
  QAction* modeAction(int mode) const;
  void refreshObjectList();
  void updateTransformUi();
  void syncMeshParams();
  void addRecentFile(const QString& path);
  void updateRecentMenu();
  void loadRecentFile(const QString& path);
  bool exportMeshesTo(const QString& path, QString* errorMessage);

  QJsonObject captureSceneState() const;
  bool applySceneState(const QJsonObject& root, QString* errorMessage);
  void onToggleLocalSync(bool enabled);
  void onSyncTick();
  void onScriptConsole();

  RhiWidget* m_rhiWidget = nullptr;
  QTabWidget* m_tabs = nullptr;
  QVector<Document> m_documents;
  int m_currentDoc = -1;
  int m_pendingDoc = -1;
  QActionGroup* m_modeGroup = nullptr;
  QVector<QAction*> m_modeActions;
  QActionGroup* m_displayGroup = nullptr;
  QActionGroup* m_toolGroup = nullptr;
  QAction* m_actionOrtho = nullptr;
  QAction* m_actionMultiView = nullptr;
  QAction* m_actionAxis = nullptr;
  QAction* m_actionGrid = nullptr;
  QAction* m_actionFxaa = nullptr;
  QAction* m_actionAo = nullptr;
  QAction* m_openAction = nullptr;
  QMenu* m_recentMenu = nullptr;

  QListWidget* m_objectList = nullptr;
  QLabel* m_objectLabel = nullptr;
  QDoubleSpinBox* m_translateSpin[3] = {nullptr, nullptr, nullptr};
  QDoubleSpinBox* m_rotateSpin[3] = {nullptr, nullptr, nullptr};
  QDoubleSpinBox* m_scaleSpin[3] = {nullptr, nullptr, nullptr};

  QVector<MeshParams> m_meshParams;
  QVector<MeshParams> m_committedParams;
  QString m_paramsSource;
  bool m_updatingObjectUi = false;

  QUndoStack* m_undoStack = nullptr;

  QString m_currentFilePath;
  QString m_pendingPath;
  QStringList m_recentFiles;
  QFutureWatcher<ModelAsset> m_loadWatcher;

  QTimer* m_syncTimer = nullptr;
  QString m_syncPath;
  QByteArray m_lastSyncHash;
  ScriptConsole* m_scriptConsole = nullptr;
};

#endif  // MODELVIEWER_SRC_UI_MAINWINDOW_H_
