#include "MainWindow.h"
#include "RhiWidget.h"
#include "MaterialDialog.h"
#include "LightDialog.h"
#include "ScriptConsole.h"
#include "../renderer/RhiRenderer.h"
#include "../loader/ModelLoader.h"
#include "../loader/ModelExporter.h"
#include "../core/ParametricSurface.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QtMath>

namespace {

/// @brief 对象变换撤销命令
class TransformCommand : public QUndoCommand {
 public:
  TransformCommand(MainWindow* window, int index,
                   const MainWindow::MeshParams& oldParams,
                   const MainWindow::MeshParams& newParams)
      : m_window(window), m_index(index),
        m_old(oldParams), m_new(newParams) {
    setText("Transform Object");
  }

  void undo() override { m_window->applyMeshParams(m_index, m_old); }
  void redo() override { m_window->applyMeshParams(m_index, m_new); }

 private:
  MainWindow* m_window = nullptr;
  int m_index = -1;
  MainWindow::MeshParams m_old;
  MainWindow::MeshParams m_new;
};

}  // namespace

namespace {

QString modeName(int mode) {
    switch (mode) {
        case 0: return "Phong";
        case 1: return "Wireframe";
        case 2: return "Vertex Normals";
        case 3: return "Face Normals";
        case 4: return "Vertex Normals Visualization";
        case 5: return "Flat Shading";
        case 6: return "PBR";
        case 7: return "Shadow Mapping";
        case 8: return "Skybox";
        case 9: return "Selection";
        case 10: return "Clipping Plane";
        case 11: return "Split Screen";
        default: return "Unknown";
    }
}

QString displayStyleName(int style) {
    switch (style) {
        case 0: return "Shaded";
        case 1: return "Wireframe";
        case 2: return "Wire-Shaded";
        case 3: return "Realistic";
        default: return "Unknown";
    }
}

QString toolName(int tool) {
    switch (tool) {
        case 0: return "Rotate";
        case 1: return "Pan";
        case 2: return "Zoom";
        case 3: return "Select";
        default: return "Unknown";
    }
}

}  // namespace

namespace {

/// @brief 从对象变换矩阵反解出平移/旋转/缩放参数
MainWindow::MeshParams decomposeParams(const QMatrix4x4& m) {
    MainWindow::MeshParams params;
    params.translate = QVector3D(m(0, 3), m(1, 3), m(2, 3));

    QVector3D sx(m(0, 0), m(1, 0), m(2, 0));
    QVector3D sy(m(0, 1), m(1, 1), m(2, 1));
    QVector3D sz(m(0, 2), m(1, 2), m(2, 2));
    params.scale = QVector3D(sx.length(), sy.length(), sz.length());
    if (params.scale.x() < 1e-6f) params.scale.setX(1.0f);
    if (params.scale.y() < 1e-6f) params.scale.setY(1.0f);
    if (params.scale.z() < 1e-6f) params.scale.setZ(1.0f);

    // 归一化旋转部分，按 Rx * Ry * Rz 顺序反解欧拉角
    const float r00 = m(0, 0) / params.scale.x();
    const float r01 = m(0, 1) / params.scale.y();
    const float r02 = m(0, 2) / params.scale.z();
    const float r11 = m(1, 1) / params.scale.y();
    const float r12 = m(1, 2) / params.scale.z();
    const float r21 = m(2, 1) / params.scale.y();
    const float r22 = m(2, 2) / params.scale.z();

    const float sinY = qBound(-1.0f, r02, 1.0f);
    const float y = qRadiansToDegrees(qAsin(sinY));
    float x = 0.0f;
    float z = 0.0f;
    if (qAbs(sinY) < 0.9999f) {
        x = qRadiansToDegrees(qAtan2(-r12, r22));
        z = qRadiansToDegrees(qAtan2(-r01, r00));
    } else {
        x = qRadiansToDegrees(qAtan2(r21, r11));
    }

    params.rotate = QVector3D(x, y, z);
    return params;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUI();
    setupMenus();
    setupObjectDock();

    connect(&m_loadWatcher, &QFutureWatcher<ModelAsset>::finished,
            this, &MainWindow::onLoadFinished);

    setRenderMode(0);
    statusBar()->showMessage("Ready");
    setWindowTitle("ModelViewer Qt6 RHI");
    resize(1280, 860);

    loadPlugins();
}

void MainWindow::setupUI() {
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::activateDocument);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeDocument);

    createDocument("Untitled");
}

int MainWindow::createDocument(const QString& label) {
    if (m_currentDoc >= 0) {
        saveCurrentDocumentState();
    }

    Document doc;
    doc.view = new RhiWidget(m_tabs);
    doc.undoStack = new QUndoStack(this);
    m_documents.append(doc);

    const int index = m_documents.size() - 1;
    RhiWidget* view = doc.view;

    m_tabs->blockSignals(true);
    m_tabs->addTab(view, label);
    m_tabs->blockSignals(false);

    connect(view, &RhiWidget::meshesChanged, this, [this, view]() {
        if (view == m_rhiWidget) {
            refreshObjectList();
        }
    });
    connect(view, &RhiWidget::projectionChanged, this,
            [this, view](bool orthographic) {
                if (view == m_rhiWidget) {
                    onProjectionChanged(orthographic);
                }
            });

    m_tabs->setCurrentIndex(index);
    if (m_currentDoc != index) {
        activateDocument(index);
    }
    return index;
}

void MainWindow::activateDocument(int index) {
    if (index < 0 || index >= m_documents.size()) {
        return;
    }
    if (index == m_currentDoc && m_rhiWidget == m_documents[index].view) {
        return;
    }

    if (m_currentDoc >= 0 && m_currentDoc < m_documents.size()) {
        saveCurrentDocumentState();
    }

    m_currentDoc = index;
    loadDocumentState(index);

    if (m_tabs->currentIndex() != index) {
        const QSignalBlocker blocker(m_tabs);
        m_tabs->setCurrentIndex(index);
    }

    syncUiToCurrentDocument();
    refreshObjectList();
    m_rhiWidget->setFocus();
}

void MainWindow::closeDocument(int index) {
    if (index < 0 || index >= m_documents.size()) {
        return;
    }

    if (m_pendingDoc == index) {
        m_pendingDoc = -1;
    }

    saveCurrentDocumentState();

    if (m_currentDoc == index) {
        m_currentDoc = -1;
    } else if (m_currentDoc > index) {
        --m_currentDoc;
    }

    m_tabs->blockSignals(true);
    m_tabs->removeTab(index);
    m_tabs->blockSignals(false);

    RhiWidget* view = m_documents[index].view;
    QUndoStack* stack = m_documents[index].undoStack;
    m_documents.removeAt(index);

    delete view;
    delete stack;

    if (m_pendingDoc > index) {
        --m_pendingDoc;
    }

    if (m_documents.isEmpty()) {
        m_currentDoc = -1;
        m_rhiWidget = nullptr;
        createDocument("Untitled");
        return;
    }

    const int next = qBound(0, index, m_documents.size() - 1);
    m_currentDoc = -1;
    m_tabs->setCurrentIndex(next);
    activateDocument(next);
}

void MainWindow::saveCurrentDocumentState() {
    if (m_currentDoc < 0 || m_currentDoc >= m_documents.size()) {
        return;
    }
    Document& doc = m_documents[m_currentDoc];
    doc.filePath = m_currentFilePath;
    doc.paramsSource = m_paramsSource;
    doc.params = m_meshParams;
    doc.committed = m_committedParams;
}

void MainWindow::loadDocumentState(int index) {
    if (index < 0 || index >= m_documents.size()) {
        return;
    }
    const Document& doc = m_documents[index];
    m_rhiWidget = doc.view;
    m_currentFilePath = doc.filePath;
    m_paramsSource = doc.paramsSource;
    m_meshParams = doc.params;
    m_committedParams = doc.committed;
    m_undoStack = doc.undoStack;
}

void MainWindow::syncUiToCurrentDocument() {
    if (!m_rhiWidget) {
        return;
    }

    setRenderModeInternal(m_rhiWidget->renderMode());
    applyDisplayStyle(m_rhiWidget->displayStyle());
    setMultiView(m_rhiWidget->isMultiView());
    setAxisVisible(m_rhiWidget->isAxisVisible());
    setGridVisible(m_rhiWidget->isGridVisible());
    setToolMode(int(m_rhiWidget->toolMode()));
    onProjectionChanged(m_rhiWidget->isOrthographic());
}

void MainWindow::setupMenus() {
    // ── File ────────────────────────────────────────────────
    QMenu* fileMenu = menuBar()->addMenu("&File");
    m_openAction = fileMenu->addAction("&Open Model...");
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    QAction* newTabAction = fileMenu->addAction("New &Tab");
    newTabAction->setShortcut(QKeySequence("Ctrl+T"));
    connect(newTabAction, &QAction::triggered, this, [this]() {
        createDocument("Untitled");
    });

    QAction* closeTabAction = fileMenu->addAction("&Close Tab");
    closeTabAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeTabAction, &QAction::triggered, this, [this]() {
        closeDocument(m_currentDoc);
    });

    QMenu* surfaceMenu = fileMenu->addMenu("New &Parametric Surface");
    const QStringList surfaceNames = {"Plane", "Sphere", "Torus", "Cylinder", "Cone"};
    for (int i = 0; i < surfaceNames.size(); ++i) {
        QAction* action = surfaceMenu->addAction(surfaceNames[i]);
        connect(action, &QAction::triggered, this, [this, i]() { onNewSurface(i); });
    }

    fileMenu->addSeparator();

    QAction* saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveModel);

    QAction* exportAction = fileMenu->addAction("Save &As / Export...");
    exportAction->setShortcut(QKeySequence::SaveAs);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportModel);

    m_recentMenu = fileMenu->addMenu("Recent &Files");
    QSettings settings;
    m_recentFiles = settings.value("recentFiles").toStringList();
    updateRecentMenu();

    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // ── Edit ────────────────────────────────────────────────
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    QAction* undoAction = editMenu->addAction("&Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this]() { m_undoStack->undo(); });
    QAction* redoAction = editMenu->addAction("&Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this]() { m_undoStack->redo(); });

    editMenu->addSeparator();
    QAction* duplicateAction = editMenu->addAction("&Duplicate Object");
    duplicateAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(duplicateAction, &QAction::triggered, this,
            &MainWindow::onDuplicateObject);
    QAction* renameAction = editMenu->addAction("&Rename Object...");
    renameAction->setShortcut(QKeySequence("F2"));
    connect(renameAction, &QAction::triggered, this, &MainWindow::onRenameObject);
    QAction* deleteAction = editMenu->addAction("De&lete Object");
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteObject);

    // ── View ────────────────────────────────────────────────
    QMenu* viewMenu = menuBar()->addMenu("&View");

    QAction* fitAction = viewMenu->addAction("&Fit to View");
    fitAction->setShortcut(QKeySequence("F"));
    connect(fitAction, &QAction::triggered, this, &MainWindow::onFitView);

    QAction* resetAction = viewMenu->addAction("&Reset View");
    resetAction->setShortcut(QKeySequence("Space"));
    connect(resetAction, &QAction::triggered, this, &MainWindow::onResetView);

    viewMenu->addSeparator();

    m_modeGroup = new QActionGroup(this);
    m_modeGroup->setExclusive(true);

    const struct {
        const char* text;
        int mode;
    } modeDefs[] = {
        {"&Phong Shading", 0},
        {"&Wireframe", 1},
        {"&Vertex Normals", 2},
        {"&Face Normals", 3},
        {"Vertex Normals &Visualization", 4},
        {"F&lat Shading", 5},
        {"&PBR Rendering", 6},
        {"&Shadow Mapping", 7},
        {"S&kybox", 8},
        {"Se&lection Mode", 9},
        {"&Clipping Plane", 10},
        {"S&plit Screen", 11},
    };

    for (const auto& def : modeDefs) {
        QAction* action = viewMenu->addAction(QString::fromLatin1(def.text));
        action->setCheckable(true);
        action->setData(def.mode);
        m_modeGroup->addAction(action);
        m_modeActions.append(action);
    }

    connect(m_modeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        setRenderModeInternal(action->data().toInt());
    });

    viewMenu->addSeparator();

    QMenu* displayMenu = viewMenu->addMenu("&Display Style");
    m_displayGroup = new QActionGroup(this);
    m_displayGroup->setExclusive(true);
    const char* styleNames[] = {"&Shaded", "&Wireframe", "Wire-&Shaded", "&Realistic"};
    for (int i = 0; i < 4; ++i) {
        QAction* action = displayMenu->addAction(QString::fromLatin1(styleNames[i]));
        action->setCheckable(true);
        action->setData(i);
        m_displayGroup->addAction(action);
        if (i == 0) {
            action->setChecked(true);
        }
    }
    connect(m_displayGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        applyDisplayStyle(action->data().toInt());
    });

    m_actionMultiView = viewMenu->addAction("&Multi-View");
    m_actionMultiView->setCheckable(true);
    m_actionMultiView->setShortcut(QKeySequence("M"));
    connect(m_actionMultiView, &QAction::toggled,
            this, &MainWindow::onToggleMultiView);

    m_actionOrtho = viewMenu->addAction("&Orthographic Projection");
    m_actionOrtho->setCheckable(true);
    m_actionOrtho->setShortcut(QKeySequence("P"));
    connect(m_actionOrtho, &QAction::toggled, this, &MainWindow::onToggleProjection);

    m_actionAxis = viewMenu->addAction("Show &Axis");
    m_actionAxis->setCheckable(true);
    m_actionAxis->setShortcut(QKeySequence("A"));
    connect(m_actionAxis, &QAction::toggled, this, &MainWindow::onToggleAxis);

    m_actionGrid = viewMenu->addAction("Show &Grid");
    m_actionGrid->setCheckable(true);
    m_actionGrid->setShortcut(QKeySequence("G"));
    connect(m_actionGrid, &QAction::toggled, this, &MainWindow::onToggleGrid);

    // ── 抗锯齿（MSAA）──────────────────────────────────────
    QMenu* aaMenu = viewMenu->addMenu("Anti-&Aliasing");
    auto* aaGroup = new QActionGroup(this);
    aaGroup->setExclusive(true);
    const struct {
        const char* text;
        int samples;
    } aaDefs[] = {
        {"&Off", 1}, {"&2x MSAA", 2}, {"&4x MSAA", 4}, {"&8x MSAA", 8},
    };
    for (const auto& def : aaDefs) {
        QAction* action = aaMenu->addAction(QString::fromLatin1(def.text));
        action->setCheckable(true);
        action->setData(def.samples);
        aaGroup->addAction(action);
        if (def.samples == 1) {
            action->setChecked(true);
        }
    }
    connect(aaGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        setMsaa(action->data().toInt());
    });

    // ── 后处理（FXAA / 深度 AO）────────────────────────────
    QMenu* postMenu = viewMenu->addMenu("&Post-Processing");
    m_actionFxaa = postMenu->addAction("&FXAA");
    m_actionFxaa->setCheckable(true);
    connect(m_actionFxaa, &QAction::toggled, this,
            &MainWindow::setFxaaEnabled);
    m_actionAo = postMenu->addAction("Depth &AO (SSAO)");
    m_actionAo->setCheckable(true);
    connect(m_actionAo, &QAction::toggled, this, &MainWindow::setAoEnabled);

    viewMenu->addSeparator();

    QMenu* presetMenu = viewMenu->addMenu("View &Presets");
    const struct {
        const char* text;
        int preset;
        const char* shortcut;
    } presetDefs[] = {
        {"&Front", 0, "1"},
        {"&Back", 1, "2"},
        {"&Left", 2, "3"},
        {"&Right", 3, "4"},
        {"&Top", 4, "5"},
        {"&Bottom", 5, "6"},
        {"&Isometric", 6, "7"},
        {"&Dimetric", 7, "8"},
        {"Tri&metric", 8, "9"},
    };

    for (const auto& def : presetDefs) {
        QAction* action = presetMenu->addAction(QString::fromLatin1(def.text));
        action->setShortcut(QKeySequence(QString::fromLatin1(def.shortcut)));
        const int preset = def.preset;
        connect(action, &QAction::triggered, this, [this, preset]() {
            applyViewPreset(preset);
            statusBar()->showMessage(QString("View preset %1").arg(preset), 2000);
        });
    }

    QMenu* toolMenu = viewMenu->addMenu("Interaction &Tool");
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);
    const struct {
        const char* text;
        int tool;
        const char* shortcut;
    } toolDefs[] = {
        {"&Rotate", 0, "R"},
        {"&Pan", 1, "T"},
        {"&Zoom", 2, "Z"},
        {"&Select", 3, "V"},
    };
    for (const auto& def : toolDefs) {
        QAction* action = toolMenu->addAction(QString::fromLatin1(def.text));
        action->setCheckable(true);
        action->setData(def.tool);
        action->setShortcut(QKeySequence(QString::fromLatin1(def.shortcut)));
        m_toolGroup->addAction(action);
        if (def.tool == 0) {
            action->setChecked(true);
        }
    }
    connect(m_toolGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        setToolMode(action->data().toInt());
    });

    QAction* fullScreenAction = viewMenu->addAction("&Full Screen");
    fullScreenAction->setShortcut(QKeySequence("F11"));
    connect(fullScreenAction, &QAction::triggered, this, &MainWindow::onToggleFullScreen);

    // 模式切换快捷键
    auto addToggleShortcut = [this](const QString& key, int mode) {
        auto* shortcut = new QShortcut(QKeySequence(key), this);
        connect(shortcut, &QShortcut::activated, this, [this, mode]() {
            toggleRenderMode(mode);
        });
    };
    addToggleShortcut("W", 1);
    addToggleShortcut("N", 2);
    addToggleShortcut("S", 7);
    addToggleShortcut("L", 1);

    // ── Tools ───────────────────────────────────────────────
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");

    QAction* materialAction = toolsMenu->addAction("&Material Settings...");
    connect(materialAction, &QAction::triggered, this, &MainWindow::onMaterialSettings);

    QMenu* materialPresetMenu = toolsMenu->addMenu("Material &Presets");
    const QStringList presetNames = {
        "Brass", "Bronze", "Chrome", "Copper", "Emerald", "Gold",
        "Jade", "Obsidian", "Pearl", "Ruby", "Silver", "Turquoise",
    };
    for (const QString& name : presetNames) {
        QAction* action = materialPresetMenu->addAction(name);
        connect(action, &QAction::triggered, this, [this, name]() {
            m_rhiWidget->applyMaterialPreset(name);
            statusBar()->showMessage("Material preset: " + name, 2000);
        });
    }

    QAction* lightAction = toolsMenu->addAction("&Light Settings...");
    connect(lightAction, &QAction::triggered, this, &MainWindow::onLightSettings);

    QAction* bgAction = toolsMenu->addAction("&Background Settings...");
    connect(bgAction, &QAction::triggered, this, &MainWindow::onBackgroundSettings);

    toolsMenu->addSeparator();

    QAction* clipAction = toolsMenu->addAction("&Clipping Plane Settings...");
    connect(clipAction, &QAction::triggered, this, &MainWindow::onClippingSettings);

    QAction* normalAction = toolsMenu->addAction("&Normal Length...");
    connect(normalAction, &QAction::triggered, this, &MainWindow::onNormalLengthSettings);

    // ── 动画 ────────────────────────────────────────────────
    QMenu* animationMenu = toolsMenu->addMenu("&Animation");
    QAction* playAction = animationMenu->addAction("&Play / Pause");
    playAction->setShortcut(QKeySequence("Ctrl+P"));
    connect(playAction, &QAction::triggered, this, [this]() {
        if (!m_rhiWidget) {
            return;
        }
        if (m_rhiWidget->animationCount() <= 0) {
            statusBar()->showMessage("No animation in current model", 3000);
            return;
        }
        if (m_rhiWidget->isAnimationPlaying()) {
            m_rhiWidget->pauseAnimation();
            statusBar()->showMessage("Animation paused", 2000);
        } else {
            m_rhiWidget->playAnimation(m_rhiWidget->currentAnimation());
            statusBar()->showMessage("Animation playing", 2000);
        }
    });
    QAction* stopAction = animationMenu->addAction("&Stop");
    connect(stopAction, &QAction::triggered, this, [this]() {
        if (m_rhiWidget) {
            m_rhiWidget->stopAnimation();
        }
    });
    animationMenu->addSeparator();
    QAction* animInfoAction = animationMenu->addAction("Animation &Info...");
    connect(animInfoAction, &QAction::triggered, this, [this]() {
        if (!m_rhiWidget || m_rhiWidget->animationCount() <= 0) {
            QMessageBox::information(this, "Animation",
                                     "No animation clips in current model.");
            return;
        }
        QStringList lines;
        for (int i = 0; i < m_rhiWidget->animationCount(); ++i) {
            lines << QString("%1. %2  (%3 s)")
                         .arg(i + 1)
                         .arg(m_rhiWidget->animationName(i))
                         .arg(m_rhiWidget->animationDuration(i), 0, 'f', 2);
        }
        QMessageBox::information(this, "Animation Clips", lines.join("\n"));
    });

    toolsMenu->addSeparator();

    QMenu* collabMenu = toolsMenu->addMenu("&Collaboration");
    QAction* exportSceneAction = collabMenu->addAction("&Export Scene State...");
    connect(exportSceneAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, "Export Scene State", "scene_state.json", "JSON (*.json)");
        if (path.isEmpty()) {
            return;
        }
        statusBar()->showMessage(
            exportSceneState(path) ? "Scene state exported"
                                   : "Failed to export scene state",
            3000);
    });
    QAction* importSceneAction = collabMenu->addAction("&Import Scene State...");
    connect(importSceneAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Import Scene State", QString(), "JSON (*.json)");
        if (path.isEmpty()) {
            return;
        }
        statusBar()->showMessage(
            importSceneState(path) ? "Scene state imported"
                                   : "Failed to import scene state",
            3000);
    });
    QAction* syncAction = collabMenu->addAction("Local &Sync (shared JSON)");
    syncAction->setCheckable(true);
    connect(syncAction, &QAction::toggled, this,
            &MainWindow::onToggleLocalSync);

    QAction* scriptConsoleAction = toolsMenu->addAction("&Script Console...");
    connect(scriptConsoleAction, &QAction::triggered, this,
            &MainWindow::onScriptConsole);
    QAction* reloadPluginsAction = toolsMenu->addAction("Reload Script &Plugins");
    connect(reloadPluginsAction, &QAction::triggered, this, [this]() {
        loadPlugins();
        statusBar()->showMessage("Script plugins reloaded", 3000);
    });

    toolsMenu->addSeparator();

    QAction* screenshotAction = toolsMenu->addAction("&Screenshot...");
    screenshotAction->setShortcut(QKeySequence("F12"));
    connect(screenshotAction, &QAction::triggered, this, &MainWindow::onScreenshot);

    QAction* modelInfoAction = toolsMenu->addAction("Model &Information...");
    connect(modelInfoAction, &QAction::triggered, this, &MainWindow::onShowModelInfo);

    // ── Help ────────────────────────────────────────────────
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    QAction* aboutAction = helpMenu->addAction("&About ModelViewer");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About ModelViewer",
                           "ModelViewer Qt6 RHI\n"
                           "Cross-platform 3D model viewer based on Qt RHI.");
    });
}

void MainWindow::setupObjectDock() {
    auto* dock = new QDockWidget("Objects", this);
    dock->setObjectName("ObjectsDock");
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* panel = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);

    m_objectList = new QListWidget(panel);
    m_objectList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_objectList, 1);

    auto* buttonRow = new QHBoxLayout();
    auto* showAllButton = new QPushButton("Show All", panel);
    auto* hideAllButton = new QPushButton("Hide All", panel);
    auto* swapButton = new QPushButton("Swap", panel);
    buttonRow->addWidget(showAllButton);
    buttonRow->addWidget(hideAllButton);
    buttonRow->addWidget(swapButton);
    layout->addLayout(buttonRow);

    auto* editRow = new QHBoxLayout();
    auto* duplicateButton = new QPushButton("Duplicate", panel);
    auto* renameButton = new QPushButton("Rename", panel);
    auto* deleteButton = new QPushButton("Delete", panel);
    editRow->addWidget(duplicateButton);
    editRow->addWidget(renameButton);
    editRow->addWidget(deleteButton);
    layout->addLayout(editRow);

    m_objectLabel = new QLabel("No selection", panel);
    layout->addWidget(m_objectLabel);

    auto* transformGroup = new QGroupBox("Transform", panel);
    auto* grid = new QGridLayout(transformGroup);
    const char* labels[3] = {"X", "Y", "Z"};

    auto createSpin = [this](double minValue, double maxValue, double step,
                             double value) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(minValue, maxValue);
        spin->setSingleStep(step);
        spin->setDecimals(3);
        spin->setValue(value);
        return spin;
    };

    for (int i = 0; i < 3; ++i) {
        m_translateSpin[i] = createSpin(-100000.0, 100000.0, 0.1, 0.0);
        m_rotateSpin[i] = createSpin(-3600.0, 3600.0, 5.0, 0.0);
        m_scaleSpin[i] = createSpin(0.01, 1000.0, 0.1, 1.0);
    }

    grid->addWidget(new QLabel("Translate"), 0, 0);
    grid->addWidget(new QLabel("Rotate (deg)"), 1, 0);
    grid->addWidget(new QLabel("Scale"), 2, 0);
    for (int i = 0; i < 3; ++i) {
        grid->addWidget(new QLabel(QString::fromLatin1(labels[i])), 0, 1 + i * 2);
        grid->addWidget(m_translateSpin[i], 0, 2 + i * 2);
        grid->addWidget(m_rotateSpin[i], 1, 2 + i * 2);
        grid->addWidget(m_scaleSpin[i], 2, 2 + i * 2);
        connect(m_translateSpin[i], &QDoubleSpinBox::valueChanged,
                this, &MainWindow::onTransformChanged);
        connect(m_rotateSpin[i], &QDoubleSpinBox::valueChanged,
                this, &MainWindow::onTransformChanged);
        connect(m_scaleSpin[i], &QDoubleSpinBox::valueChanged,
                this, &MainWindow::onTransformChanged);
        connect(m_translateSpin[i], &QDoubleSpinBox::editingFinished,
                this, &MainWindow::onTransformCommitted);
        connect(m_rotateSpin[i], &QDoubleSpinBox::editingFinished,
                this, &MainWindow::onTransformCommitted);
        connect(m_scaleSpin[i], &QDoubleSpinBox::editingFinished,
                this, &MainWindow::onTransformCommitted);
    }
    layout->addWidget(transformGroup);

    auto* resetTransformButton = new QPushButton("Reset Transform", panel);
    layout->addWidget(resetTransformButton);

    connect(m_objectList, &QListWidget::currentRowChanged,
            this, &MainWindow::onObjectSelectionChanged);
    connect(m_objectList, &QListWidget::itemChanged,
            this, &MainWindow::onObjectItemChanged);
    connect(showAllButton, &QPushButton::clicked, this, &MainWindow::onShowAllObjects);
    connect(hideAllButton, &QPushButton::clicked, this, &MainWindow::onHideAllObjects);
    connect(swapButton, &QPushButton::clicked, this, &MainWindow::onSwapVisibility);
    connect(duplicateButton, &QPushButton::clicked, this,
            &MainWindow::onDuplicateObject);
    connect(renameButton, &QPushButton::clicked, this,
            &MainWindow::onRenameObject);
    connect(deleteButton, &QPushButton::clicked, this,
            &MainWindow::onDeleteObject);
    connect(resetTransformButton, &QPushButton::clicked,
            this, &MainWindow::onResetTransform);

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    refreshObjectList();
}

void MainWindow::refreshObjectList() {
    if (!m_rhiWidget || !m_objectList || m_updatingObjectUi) {
        return;
    }

    const QString source = m_rhiWidget->sourceLabel();
    const int meshCount = m_rhiWidget->meshCount();
    if (m_paramsSource != source) {
        m_paramsSource = source;
        m_meshParams.clear();
        for (int i = 0; i < meshCount; ++i) {
            m_meshParams.append(MeshParams());
        }
        m_committedParams = m_meshParams;
        m_undoStack->clear();
    } else if (m_meshParams.size() != meshCount) {
        // 编辑操作（删除/复制）后按当前对象变换重建参数，避免丢失已有变换
        m_meshParams.resize(meshCount);
        for (int i = 0; i < meshCount; ++i) {
            m_meshParams[i] = decomposeParams(m_rhiWidget->meshTransform(i));
        }
        m_committedParams = m_meshParams;
        m_undoStack->clear();
    }

    m_updatingObjectUi = true;
    m_objectList->clear();

    for (int i = 0; i < m_rhiWidget->meshCount(); ++i) {
        auto* item = new QListWidgetItem(m_rhiWidget->meshName(i), m_objectList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_rhiWidget->isMeshVisible(i) ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, i);
    }

    const int selected = m_rhiWidget->selectedMesh();
    if (selected >= 0 && selected < m_objectList->count()) {
        m_objectList->setCurrentRow(selected);
    }
    m_updatingObjectUi = false;

    updateTransformUi();
}

void MainWindow::updateTransformUi() {
    const int row = m_objectList ? m_objectList->currentRow() : -1;
    const bool valid = (row >= 0 && row < m_meshParams.size());

    m_updatingObjectUi = true;
    if (valid) {
        const MeshParams& params = m_meshParams[row];
        for (int i = 0; i < 3; ++i) {
            m_translateSpin[i]->setValue(params.translate[i]);
            m_rotateSpin[i]->setValue(params.rotate[i]);
            m_scaleSpin[i]->setValue(params.scale[i]);
        }
        m_objectLabel->setText(QString("Selected: %1").arg(m_rhiWidget->meshName(row)));
    } else {
        for (int i = 0; i < 3; ++i) {
            m_translateSpin[i]->setValue(0.0);
            m_rotateSpin[i]->setValue(0.0);
            m_scaleSpin[i]->setValue(1.0);
        }
        m_objectLabel->setText("No selection");
    }
    m_updatingObjectUi = false;
}

void MainWindow::syncMeshParams() {
    if (m_meshParams.size() != m_rhiWidget->meshCount()) {
        m_meshParams.resize(m_rhiWidget->meshCount());
    }
}

void MainWindow::onObjectSelectionChanged() {
    if (m_updatingObjectUi || !m_objectList) {
        return;
    }
    const int row = m_objectList->currentRow();
    if (row >= 0 && row < m_rhiWidget->meshCount()) {
        m_updatingObjectUi = true;
        m_rhiWidget->selectMesh(row);
        m_updatingObjectUi = false;
    }
    updateTransformUi();
}

void MainWindow::onObjectItemChanged(QListWidgetItem* item) {
    if (m_updatingObjectUi || !item) {
        return;
    }
    const int index = item->data(Qt::UserRole).toInt();
    m_updatingObjectUi = true;
    m_rhiWidget->setMeshVisible(index, item->checkState() == Qt::Checked);
    m_updatingObjectUi = false;
    updateTransformUi();
}

void MainWindow::onTransformChanged() {
    if (m_updatingObjectUi || !m_objectList) {
        return;
    }
    const int row = m_objectList->currentRow();
    if (row < 0 || row >= m_meshParams.size()) {
        return;
    }

    MeshParams& params = m_meshParams[row];
    for (int i = 0; i < 3; ++i) {
        params.translate[i] = float(m_translateSpin[i]->value());
        params.rotate[i] = float(m_rotateSpin[i]->value());
        params.scale[i] = float(m_scaleSpin[i]->value());
    }

    QMatrix4x4 transform;
    transform.translate(params.translate);
    transform.rotate(params.rotate.x(), 1, 0, 0);
    transform.rotate(params.rotate.y(), 0, 1, 0);
    transform.rotate(params.rotate.z(), 0, 0, 1);
    transform.scale(params.scale);

    m_updatingObjectUi = true;
    m_rhiWidget->setMeshTransform(row, transform);
    m_updatingObjectUi = false;
    updateTransformUi();
}

void MainWindow::onTransformCommitted() {
    if (m_updatingObjectUi || !m_objectList) {
        return;
    }
    const int row = m_objectList->currentRow();
    if (row < 0 || row >= m_meshParams.size()) {
        return;
    }
    if (m_committedParams.size() != m_meshParams.size()) {
        m_committedParams = m_meshParams;
    }

    const MeshParams oldParams = m_committedParams[row];
    const MeshParams newParams = m_meshParams[row];
    if (oldParams == newParams) {
        return;
    }
    m_undoStack->push(new TransformCommand(this, row, oldParams, newParams));
}

void MainWindow::applyMeshParams(int index, const MeshParams& params) {
    if (!m_rhiWidget || index < 0 || index >= m_rhiWidget->meshCount()) {
        return;
    }

    if (m_meshParams.size() != m_rhiWidget->meshCount()) {
        m_meshParams.resize(m_rhiWidget->meshCount());
    }
    if (m_committedParams.size() != m_meshParams.size()) {
        m_committedParams = m_meshParams;
    }

    m_meshParams[index] = params;
    m_committedParams[index] = params;

    QMatrix4x4 transform;
    transform.translate(params.translate);
    transform.rotate(params.rotate.x(), 1, 0, 0);
    transform.rotate(params.rotate.y(), 0, 1, 0);
    transform.rotate(params.rotate.z(), 0, 0, 1);
    transform.scale(params.scale);

    m_updatingObjectUi = true;
    if (m_objectList && m_objectList->currentRow() != index) {
        m_objectList->setCurrentRow(index);
    }
    m_rhiWidget->setMeshTransform(index, transform);
    m_updatingObjectUi = false;
    updateTransformUi();
}

void MainWindow::onResetTransform() {
    const int row = m_objectList ? m_objectList->currentRow() : -1;
    if (row < 0 || row >= m_meshParams.size()) {
        return;
    }

    m_updatingObjectUi = true;
    m_meshParams[row] = MeshParams();
    for (int i = 0; i < 3; ++i) {
        m_translateSpin[i]->setValue(0.0);
        m_rotateSpin[i]->setValue(0.0);
        m_scaleSpin[i]->setValue(1.0);
    }
    m_rhiWidget->setMeshTransform(row, QMatrix4x4());
    m_updatingObjectUi = false;
    updateTransformUi();
}

void MainWindow::onShowAllObjects() {
    m_rhiWidget->showAllMeshes(true);
    refreshObjectList();
}

void MainWindow::onHideAllObjects() {
    m_rhiWidget->showAllMeshes(false);
    refreshObjectList();
}

void MainWindow::onSwapVisibility() {
    m_rhiWidget->swapMeshVisibility();
    refreshObjectList();
}

void MainWindow::onDuplicateObject() {
    if (!m_rhiWidget) {
        return;
    }
    const int index = m_rhiWidget->selectedMesh();
    if (index < 0) {
        statusBar()->showMessage("Select an object first", 2000);
        return;
    }
    const int copyIndex = m_rhiWidget->duplicateMeshAt(index);
    if (copyIndex >= 0) {
        m_rhiWidget->selectMesh(copyIndex);
        refreshObjectList();
        statusBar()->showMessage(
            QString("Duplicated object %1").arg(index), 2000);
    }
}

void MainWindow::onRenameObject() {
    if (!m_rhiWidget) {
        return;
    }
    const int index = m_rhiWidget->selectedMesh();
    if (index < 0) {
        statusBar()->showMessage("Select an object first", 2000);
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "Rename Object", "Name:", QLineEdit::Normal,
        m_rhiWidget->meshName(index), &ok);
    if (ok && m_rhiWidget->renameMeshAt(index, name)) {
        refreshObjectList();
        statusBar()->showMessage("Object renamed", 2000);
    }
}

void MainWindow::onDeleteObject() {
    if (!m_rhiWidget) {
        return;
    }
    const int index = m_rhiWidget->selectedMesh();
    if (index < 0) {
        statusBar()->showMessage("Select an object first", 2000);
        return;
    }
    if (m_rhiWidget->removeMeshAt(index)) {
        m_meshParams.clear();
        for (int i = 0; i < m_rhiWidget->meshCount(); ++i) {
            m_meshParams.append(MeshParams());
        }
        m_committedParams = m_meshParams;
        refreshObjectList();
        statusBar()->showMessage(
            QString("Deleted object %1").arg(index), 2000);
    }
}

QAction* MainWindow::modeAction(int mode) const {
    for (QAction* action : m_modeActions) {
        if (action->data().toInt() == mode) {
            return action;
        }
    }
    return nullptr;
}

void MainWindow::setRenderMode(int mode) {
    setRenderModeInternal(mode);
}

void MainWindow::setRenderModeInternal(int mode) {
    if (QAction* action = modeAction(mode)) {
        if (!action->isChecked()) {
            const QSignalBlocker blocker(action);
            action->setChecked(true);
        }
    }

    m_rhiWidget->setRenderMode(mode);
    statusBar()->showMessage("Render mode: " + modeName(mode));
}

void MainWindow::toggleRenderMode(int mode) {
    if (!m_rhiWidget) {
        return;
    }
    QAction* action = modeAction(mode);
    if (action && action->isChecked()) {
        setRenderModeInternal(0);
    } else {
        setRenderModeInternal(mode);
    }
}

void MainWindow::applyDisplayStyle(int style) {
    if (m_displayGroup) {
        for (QAction* action : m_displayGroup->actions()) {
            if (action->data().toInt() == style) {
                const QSignalBlocker blocker(action);
                action->setChecked(true);
                break;
            }
        }
    }
    m_rhiWidget->setDisplayStyle(style);
    statusBar()->showMessage("Display style: " + displayStyleName(style), 2000);
}

void MainWindow::setMultiView(bool enabled) {
    if (m_actionMultiView) {
        const QSignalBlocker blocker(m_actionMultiView);
        m_actionMultiView->setChecked(enabled);
    }
    m_rhiWidget->setMultiView(enabled);
}

void MainWindow::setAxisVisible(bool visible) {
    if (m_actionAxis) {
        const QSignalBlocker blocker(m_actionAxis);
        m_actionAxis->setChecked(visible);
    }
    m_rhiWidget->setAxisVisible(visible);
}

void MainWindow::setGridVisible(bool visible) {
    if (m_actionGrid) {
        const QSignalBlocker blocker(m_actionGrid);
        m_actionGrid->setChecked(visible);
    }
    m_rhiWidget->setGridVisible(visible);
}

void MainWindow::setMsaa(int samples) {
    m_rhiWidget->setMsaaSampleCount(samples);
    statusBar()->showMessage(
        samples <= 1 ? QString("MSAA: off")
                     : QString("MSAA: %1x").arg(samples),
        2000);
}

void MainWindow::setFxaaEnabled(bool enabled) {
    if (m_actionFxaa) {
        const QSignalBlocker blocker(m_actionFxaa);
        m_actionFxaa->setChecked(enabled);
    }
    m_rhiWidget->setPostProcessing(enabled, m_rhiWidget->isAoEnabled());
    statusBar()->showMessage(enabled ? "FXAA: on" : "FXAA: off", 2000);
}

void MainWindow::setAoEnabled(bool enabled) {
    if (m_actionAo) {
        const QSignalBlocker blocker(m_actionAo);
        m_actionAo->setChecked(enabled);
    }
    m_rhiWidget->setPostProcessing(m_rhiWidget->isFxaaEnabled(), enabled);
    statusBar()->showMessage(enabled ? "Depth AO: on" : "Depth AO: off", 2000);
}

QString MainWindow::renderStatsText() const {
    return m_rhiWidget ? m_rhiWidget->renderStatsText() : QString();
}

bool MainWindow::hasAnimation() const {
    return m_rhiWidget && m_rhiWidget->animationCount() > 0;
}

int MainWindow::animationCount() const {
    return m_rhiWidget ? m_rhiWidget->animationCount() : 0;
}

void MainWindow::playAnimation(int index) {
    if (m_rhiWidget && m_rhiWidget->animationCount() > 0) {
        m_rhiWidget->playAnimation(
            index < 0 ? qMax(0, m_rhiWidget->currentAnimation()) : index);
    }
}

void MainWindow::pauseAnimation() {
    if (m_rhiWidget) {
        m_rhiWidget->pauseAnimation();
    }
}

void MainWindow::stopAnimation() {
    if (m_rhiWidget) {
        m_rhiWidget->stopAnimation();
    }
}

void MainWindow::setAnimationTime(float seconds) {
    if (m_rhiWidget) {
        m_rhiWidget->setAnimationTime(seconds);
    }
}

QString MainWindow::animationName(int index) const {
    return m_rhiWidget ? m_rhiWidget->animationName(index) : QString();
}

QJsonObject MainWindow::captureSceneState() const {
    QJsonObject root;
    root.insert("version", 1);

    if (!m_rhiWidget) {
        return root;
    }

    root.insert("source", m_rhiWidget->sourceLabel());
    root.insert("renderMode", m_rhiWidget->renderMode());
    root.insert("displayStyle", m_rhiWidget->displayStyle());
    root.insert("multiView", m_rhiWidget->isMultiView());
    root.insert("axis", m_rhiWidget->isAxisVisible());
    root.insert("grid", m_rhiWidget->isGridVisible());
    root.insert("msaa", m_rhiWidget->msaaSampleCount());
    root.insert("fxaa", m_rhiWidget->isFxaaEnabled());
    root.insert("ao", m_rhiWidget->isAoEnabled());

    QJsonArray clipArray;
    const QVector4D clip = m_rhiWidget->clippingPlane();
    clipArray.append(clip.x());
    clipArray.append(clip.y());
    clipArray.append(clip.z());
    clipArray.append(clip.w());
    root.insert("clipPlane", clipArray);

    const Camera& camera = m_rhiWidget->getCamera();
    QJsonObject cameraObject;
    auto vecToArray = [](const QVector3D& v) {
        QJsonArray array;
        array.append(v.x());
        array.append(v.y());
        array.append(v.z());
        return array;
    };
    cameraObject.insert("eye", vecToArray(camera.position()));
    cameraObject.insert("target", vecToArray(camera.target()));
    cameraObject.insert("up", vecToArray(camera.upDirection()));
    cameraObject.insert("orthographic", m_rhiWidget->isOrthographic());
    root.insert("camera", cameraObject);

    QJsonObject animation;
    animation.insert("clip", m_rhiWidget->currentAnimation());
    animation.insert("time", m_rhiWidget->animationTime());
    root.insert("animation", animation);

    QJsonArray objects;
    for (int i = 0; i < m_rhiWidget->meshCount(); ++i) {
        QJsonObject object;
        object.insert("name", m_rhiWidget->meshName(i));
        object.insert("visible", m_rhiWidget->isMeshVisible(i));

        const QMatrix4x4 transform = m_rhiWidget->meshTransform(i);
        QJsonArray matrix;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                matrix.append(transform(row, column));
            }
        }
        object.insert("transform", matrix);

        const Material material = m_rhiWidget->meshMaterial(i);
        QJsonArray color;
        color.append(material.diffuse.x());
        color.append(material.diffuse.y());
        color.append(material.diffuse.z());
        object.insert("color", color);

        objects.append(object);
    }
    root.insert("objects", objects);

    return root;
}

bool MainWindow::applySceneState(const QJsonObject& root, QString* errorMessage) {
    if (!m_rhiWidget) {
        if (errorMessage) *errorMessage = "No active document";
        return false;
    }
    if (!root.contains("objects")) {
        if (errorMessage) *errorMessage = "Invalid scene state (missing objects)";
        return false;
    }

    auto readVec3 = [](const QJsonValue& value, const QVector3D& fallback) {
        const QJsonArray array = value.toArray();
        if (array.size() != 3) {
            return fallback;
        }
        return QVector3D(float(array[0].toDouble()), float(array[1].toDouble()),
                         float(array[2].toDouble()));
    };

    if (root.contains("renderMode")) {
        m_rhiWidget->setRenderMode(root.value("renderMode").toInt());
    }
    if (root.contains("displayStyle")) {
        m_rhiWidget->setDisplayStyle(root.value("displayStyle").toInt());
    }
    if (root.contains("multiView")) {
        m_rhiWidget->setMultiView(root.value("multiView").toBool());
    }
    if (root.contains("axis")) {
        m_rhiWidget->setAxisVisible(root.value("axis").toBool());
    }
    if (root.contains("grid")) {
        m_rhiWidget->setGridVisible(root.value("grid").toBool());
    }
    if (root.contains("msaa")) {
        m_rhiWidget->setMsaaSampleCount(root.value("msaa").toInt());
    }
    if (root.contains("fxaa") || root.contains("ao")) {
        m_rhiWidget->setPostProcessing(root.value("fxaa").toBool(),
                                       root.value("ao").toBool());
    }

    const QJsonArray clip = root.value("clipPlane").toArray();
    if (clip.size() == 4) {
        m_rhiWidget->setClippingPlane(
            QVector4D(float(clip[0].toDouble()), float(clip[1].toDouble()),
                      float(clip[2].toDouble()), float(clip[3].toDouble())));
    }

    const QJsonObject cameraObject = root.value("camera").toObject();
    if (!cameraObject.isEmpty()) {
        Camera& camera = m_rhiWidget->getCamera();
        camera.setPosition(readVec3(cameraObject.value("eye"),
                                    camera.position()));
        camera.setTarget(readVec3(cameraObject.value("target"),
                                  camera.target()));
        camera.setUp(readVec3(cameraObject.value("up"), camera.upDirection()));
        if (cameraObject.contains("orthographic")) {
            m_rhiWidget->setProjectionType(
                cameraObject.value("orthographic").toBool()
                    ? ProjectionType::Orthographic
                    : ProjectionType::Perspective);
        }
    }

    const QJsonArray objects = root.value("objects").toArray();

    // 优先按对象名匹配，找不到时退回索引（便于不同文档间同步）
    QHash<QString, int> nameToIndex;
    for (int i = 0; i < m_rhiWidget->meshCount(); ++i) {
        const QString name = m_rhiWidget->meshName(i);
        if (!name.isEmpty() && !nameToIndex.contains(name)) {
            nameToIndex.insert(name, i);
        }
    }

    for (int i = 0; i < objects.size(); ++i) {
        const QJsonObject object = objects[i].toObject();
        const QString objectName = object.value("name").toString();
        int target = nameToIndex.value(objectName, -1);
        if (target < 0 && i < m_rhiWidget->meshCount()) {
            target = i;
        }
        if (target < 0) {
            continue;
        }

        if (object.contains("visible")) {
            m_rhiWidget->setMeshVisible(target,
                                        object.value("visible").toBool());
        }

        const QJsonArray matrix = object.value("transform").toArray();
        if (matrix.size() == 16) {
            QMatrix4x4 transform;
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    transform(row, column) =
                        float(matrix[row * 4 + column].toDouble());
                }
            }
            m_rhiWidget->setMeshTransform(target, transform);
        }

        const QJsonArray color = object.value("color").toArray();
        if (color.size() == 3) {
            Material material = m_rhiWidget->meshMaterial(target);
            material.diffuse = QVector3D(float(color[0].toDouble()),
                                         float(color[1].toDouble()),
                                         float(color[2].toDouble()));
            m_rhiWidget->setMeshMaterial(target, material);
        }
    }

    const QJsonObject animation = root.value("animation").toObject();
    if (!animation.isEmpty() && m_rhiWidget->animationCount() > 0) {
        const int clipIndex = animation.value("clip").toInt(0);
        if (clipIndex >= 0 && clipIndex < m_rhiWidget->animationCount()) {
            m_rhiWidget->playAnimation(clipIndex);
            m_rhiWidget->pauseAnimation();
            m_rhiWidget->setAnimationTime(
                float(animation.value("time").toDouble(0.0)));
        }
    }

    // 同步对象面板参数，避免与已应用变换不一致
    m_meshParams.resize(m_rhiWidget->meshCount());
    for (int i = 0; i < m_rhiWidget->meshCount(); ++i) {
        m_meshParams[i] = decomposeParams(m_rhiWidget->meshTransform(i));
    }
    m_committedParams = m_meshParams;
    m_undoStack->clear();
    refreshObjectList();
    return true;
}

bool MainWindow::exportSceneState(const QString& path) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QJsonDocument document(captureSceneState());
    file.write(document.toJson(QJsonDocument::Indented));
    return file.commit();
}

bool MainWindow::importSceneState(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    QString error;
    const bool applied = applySceneState(document.object(), &error);
    if (!applied) {
        statusBar()->showMessage(error, 4000);
    }
    return applied;
}

void MainWindow::onToggleLocalSync(bool enabled) {
    if (!m_syncTimer) {
        m_syncTimer = new QTimer(this);
        m_syncTimer->setInterval(700);
        connect(m_syncTimer, &QTimer::timeout, this, &MainWindow::onSyncTick);
    }

    if (!enabled) {
        m_syncTimer->stop();
        statusBar()->showMessage("Local sync stopped", 2000);
        return;
    }

    m_syncPath = QDir::temp().filePath("modelviewer_scene_sync.json");
    m_lastSyncHash.clear();
    m_syncTimer->start();
    statusBar()->showMessage("Local sync: " + m_syncPath, 5000);
}

void MainWindow::onSyncTick() {
    if (!m_rhiWidget || m_syncPath.isEmpty()) {
        return;
    }

    QFile file(m_syncPath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        const QByteArray data = file.readAll();
        file.close();
        const QByteArray hash =
            QCryptographicHash::hash(data, QCryptographicHash::Sha1);
        if (hash != m_lastSyncHash) {
            m_lastSyncHash = hash;
            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(data, &parseError);
            if (parseError.error == QJsonParseError::NoError &&
                document.isObject()) {
                QString error;
                if (applySceneState(document.object(), &error)) {
                    statusBar()->showMessage("Scene synced from shared file",
                                             2000);
                }
            }
        }
    }

    // 将当前场景写回共享文件（哈希未变化时避免重复写入）
    const QJsonDocument local(captureSceneState());
    const QByteArray data = local.toJson(QJsonDocument::Compact);
    const QByteArray hash =
        QCryptographicHash::hash(data, QCryptographicHash::Sha1);
    if (hash != m_lastSyncHash) {
        QFile out(m_syncPath);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            out.write(data);
            out.close();
            m_lastSyncHash = hash;
        }
    }
}

void MainWindow::onScriptConsole() {
    if (!m_scriptConsole) {
        m_scriptConsole = new ScriptConsole(m_rhiWidget, this);
    }
    m_scriptConsole->setView(m_rhiWidget);
    m_scriptConsole->show();
    m_scriptConsole->raise();
    m_scriptConsole->activateWindow();
}

bool MainWindow::runScriptFile(const QString& path) {
    if (!m_scriptConsole) {
        m_scriptConsole = new ScriptConsole(m_rhiWidget, this);
    }
    m_scriptConsole->setView(m_rhiWidget);
    return m_scriptConsole->runFile(path);
}

void MainWindow::loadPlugins() {
    const QDir pluginDir(QCoreApplication::applicationDirPath() + "/plugins");
    if (!pluginDir.exists()) {
        return;
    }
    const QStringList files =
        pluginDir.entryList({"*.js"}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        runScriptFile(pluginDir.filePath(name));
    }
}

void MainWindow::setToolMode(int mode) {
    if (m_toolGroup) {
        for (QAction* action : m_toolGroup->actions()) {
            if (action->data().toInt() == mode) {
                const QSignalBlocker blocker(action);
                action->setChecked(true);
                break;
            }
        }
    }
    m_rhiWidget->setToolMode(static_cast<ToolMode>(mode));
    statusBar()->showMessage("Tool: " + toolName(mode), 2000);
}

void MainWindow::openModel(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    if (m_loadWatcher.isRunning()) {
        QMessageBox::information(this, "Loading", "A model is already being loaded.");
        return;
    }

    int target = m_currentDoc;
    if (target < 0 || target >= m_documents.size() ||
        !m_documents[target].view ||
        m_documents[target].view->meshCount() > 0 ||
        !m_documents[target].filePath.isEmpty()) {
        target = createDocument("Loading...");
    }

    m_pendingDoc = target;
    m_pendingPath = path;
    if (m_openAction) {
        m_openAction->setEnabled(false);
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage("Loading: " + QFileInfo(path).fileName());

    m_loadWatcher.setFuture(QtConcurrent::run([path]() {
        ModelLoader loader;
        return loader.loadCPU(path);
    }));
}

void MainWindow::onLoadFinished() {
    QApplication::restoreOverrideCursor();
    if (m_openAction) {
        m_openAction->setEnabled(true);
    }

    const int target = m_pendingDoc;
    const QString path = m_pendingPath;
    m_pendingDoc = -1;

    const ModelAsset asset = m_loadWatcher.result();
    if (target < 0 || target >= m_documents.size()) {
        return;
    }

    if (asset.meshes.isEmpty()) {
        QMessageBox::warning(this, "Load Model",
                             QString("Failed to load model:\n%1").arg(path));
        statusBar()->showMessage("Load failed", 3000);
        m_tabs->setTabText(target, "Untitled");
        return;
    }

    Document& doc = m_documents[target];
    doc.filePath = path;
    doc.paramsSource.clear();
    doc.params.clear();
    doc.committed.clear();
    doc.undoStack->clear();
    doc.view->setCPUMeshes(asset.meshes, path);
    doc.view->setAnimationClips(asset.animations);

    m_tabs->setTabText(target, QFileInfo(path).fileName());
    m_currentDoc = -1;
    m_tabs->setCurrentIndex(target);
    activateDocument(target);
    addRecentFile(path);

    statusBar()->showMessage(
        asset.animations.isEmpty()
            ? QString("Loaded: %1").arg(QFileInfo(path).fileName())
            : QString("Loaded: %1 (%2 animation)")
                  .arg(QFileInfo(path).fileName())
                  .arg(asset.animations.size()));
    if (m_rhiWidget) {
        m_rhiWidget->setFocus();
    }
    emit modelLoaded();
}

void MainWindow::onOpenFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Model File", QString(),
        "Model Files (*.obj *.stl *.fbx *.dae *.ply *.gltf *.glb *.3ds *.x *.off *.dxf);;"
        "All Files (*)");

    openModel(path);
}

void MainWindow::loadRecentFile(const QString& path) {
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, "Recent File",
                             QString("File no longer exists:\n%1").arg(path));
        m_recentFiles.removeAll(path);
        QSettings settings;
        settings.setValue("recentFiles", m_recentFiles);
        updateRecentMenu();
        return;
    }
    openModel(path);
}

void MainWindow::addRecentFile(const QString& path) {
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 10) {
        m_recentFiles.removeLast();
    }

    QSettings settings;
    settings.setValue("recentFiles", m_recentFiles);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu() {
    if (!m_recentMenu) {
        return;
    }
    m_recentMenu->clear();

    if (m_recentFiles.isEmpty()) {
        QAction* emptyAction = m_recentMenu->addAction("(No recent files)");
        emptyAction->setEnabled(false);
        return;
    }

    for (const QString& path : m_recentFiles) {
        QAction* action = m_recentMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            loadRecentFile(path);
        });
    }
}

void MainWindow::onFitView() {
    m_rhiWidget->fitToView();
}

void MainWindow::onResetView() {
    m_rhiWidget->getCamera().reset();
    m_rhiWidget->fitToView();
    m_rhiWidget->update();
}

void MainWindow::onToggleFullScreen() {
    if (window()->isFullScreen()) {
        window()->showNormal();
    } else {
        window()->showFullScreen();
    }
}

void MainWindow::onToggleProjection(bool orthographic) {
    m_rhiWidget->setProjectionType(orthographic ? ProjectionType::Orthographic
                                                : ProjectionType::Perspective);
}

void MainWindow::onProjectionChanged(bool orthographic) {
    if (!m_actionOrtho) {
        return;
    }
    const QSignalBlocker blocker(m_actionOrtho);
    m_actionOrtho->setChecked(orthographic);
}

void MainWindow::onToggleMultiView(bool enabled) {
    m_rhiWidget->setMultiView(enabled);
    statusBar()->showMessage(enabled ? "Multi-view enabled" : "Multi-view disabled", 2000);
}

void MainWindow::onToggleAxis(bool visible) {
    m_rhiWidget->setAxisVisible(visible);
}

void MainWindow::onToggleGrid(bool visible) {
    m_rhiWidget->setGridVisible(visible);
}

void MainWindow::onMaterialSettings() {
    const int selected = m_rhiWidget->selectedMesh();
    const Material current = (selected >= 0)
        ? m_rhiWidget->meshMaterial(selected)
        : m_rhiWidget->getMaterial();

    MaterialDialog dialog(current, this);
    if (dialog.exec() == QDialog::Accepted) {
        if (selected >= 0) {
            m_rhiWidget->setMeshMaterial(selected, dialog.getMaterial());
            statusBar()->showMessage(
                QString("Material updated: %1").arg(m_rhiWidget->meshName(selected)), 2000);
        } else {
            m_rhiWidget->setMaterial(dialog.getMaterial());
            statusBar()->showMessage("Material updated for all objects", 2000);
        }
    }
}

void MainWindow::onLightSettings() {
    LightDialog dialog(m_rhiWidget->getLight(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_rhiWidget->setLight(dialog.getLight());
    }
}

void MainWindow::onBackgroundSettings() {
    const QColor topColor = QColorDialog::getColor(
        QColor::fromRgbF(0.2f, 0.2f, 0.3f), this, "Select Top Background Color");
    if (!topColor.isValid()) {
        return;
    }
    const QColor botColor = QColorDialog::getColor(
        QColor::fromRgbF(0.1f, 0.1f, 0.15f), this, "Select Bottom Background Color");
    if (!botColor.isValid()) {
        return;
    }

    m_rhiWidget->setBackgroundColors(
        QVector3D(topColor.redF(), topColor.greenF(), topColor.blueF()),
        QVector3D(botColor.redF(), botColor.greenF(), botColor.blueF()));
}

void MainWindow::onClippingSettings() {
    const BoundingBox bbox = m_rhiWidget->sceneBoundingBox();
    if (bbox.isEmpty()) {
        QMessageBox::information(this, "Clipping Plane", "Please load a model first.");
        return;
    }

    const QStringList axes = {"X", "Y", "Z"};
    bool ok = false;
    const QString axis = QInputDialog::getItem(
        this, "Clipping Plane", "Clip axis:", axes, 0, false, &ok);
    if (!ok || axis.isEmpty()) {
        return;
    }

    const double offset = QInputDialog::getDouble(
        this, "Clipping Plane", "Offset (-1 .. 1):", 0.0, -1.0, 1.0, 2, &ok);
    if (!ok) {
        return;
    }

    const QVector3D center = bbox.center();
    const float radius = qMax(bbox.radius(), 0.001f);
    QVector4D plane(-1.0f, 0.0f, 0.0f, 0.0f);

    if (axis == "X") {
        plane = QVector4D(-1.0f, 0.0f, 0.0f, center.x() + float(offset) * radius);
    } else if (axis == "Y") {
        plane = QVector4D(0.0f, -1.0f, 0.0f, center.y() + float(offset) * radius);
    } else {
        plane = QVector4D(0.0f, 0.0f, -1.0f, center.z() + float(offset) * radius);
    }

    m_rhiWidget->setClippingPlane(plane);
    statusBar()->showMessage("Clipping plane updated", 2000);
}

void MainWindow::onNormalLengthSettings() {
    const BoundingBox bbox = m_rhiWidget->sceneBoundingBox();
    const float radius = bbox.isEmpty() ? 1.0f : qMax(bbox.radius(), 0.001f);

    bool ok = false;
    const double length = QInputDialog::getDouble(
        this, "Normal Length", "Normal line length:", radius * 0.15,
        0.001, radius * 10.0, 3, &ok);
    if (!ok) {
        return;
    }
    m_rhiWidget->setNormalLength(float(length));
}

void MainWindow::onScreenshot() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Screenshot", QString(),
        "PNG Files (*.png);;JPEG Files (*.jpg);;BMP Files (*.bmp);;All Files (*)");
    if (path.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString resolution = QInputDialog::getText(
        this, "Screenshot", "Resolution (WxH, empty = current):",
        QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }

    const QRegularExpression regex("(\\d+)\\s*[xX*]\\s*(\\d+)");
    const QRegularExpressionMatch match = regex.match(resolution.trimmed());

    bool success = false;
    if (match.hasMatch()) {
        success = m_rhiWidget->saveScreenshotScaled(
            path, match.captured(1).toInt(), match.captured(2).toInt());
    } else {
        QString format = QFileInfo(path).suffix().toLower();
        if (format.isEmpty()) {
            format = "png";
        }
        int quality = 95;
        if (format == "jpg" || format == "jpeg") {
            quality = QInputDialog::getInt(
                this, "JPEG Quality", "Select quality (0-100):", 95, 0, 100, 1, &ok);
            if (!ok) {
                return;
            }
        }
        success = m_rhiWidget->saveScreenshotEx(path, format, quality);
    }

    if (success) {
        statusBar()->showMessage("Screenshot saved: " + path, 3000);
    } else {
        QMessageBox::warning(this, "Screenshot", "Failed to save screenshot.");
    }
}

bool MainWindow::exportMeshesTo(const QString& path, QString* errorMessage) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QVector<CpuMesh> meshes = m_rhiWidget->cpuMeshes();
    if (meshes.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "No model to export.";
        }
        return false;
    }

    if (suffix == "stl") {
        return ModelExporter::exportSTL(meshes, path, errorMessage);
    }
    return ModelExporter::exportOBJ(meshes, path, errorMessage);
}

void MainWindow::onExportModel() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export Model", m_currentFilePath,
        "Wavefront OBJ (*.obj);;STL (*.stl)");
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".obj";
    }

    QString error;
    if (exportMeshesTo(path, &error)) {
        statusBar()->showMessage("Exported: " + path, 3000);
    } else {
        QMessageBox::warning(this, "Export Model",
                             QString("Export failed:\n%1").arg(error));
    }
}

void MainWindow::onSaveModel() {
    const QString suffix = QFileInfo(m_currentFilePath).suffix().toLower();
    if (!m_currentFilePath.isEmpty() && (suffix == "obj" || suffix == "stl")) {
        QString error;
        if (exportMeshesTo(m_currentFilePath, &error)) {
            statusBar()->showMessage("Saved: " + m_currentFilePath, 3000);
            return;
        }
        QMessageBox::warning(this, "Save Model",
                             QString("Save failed:\n%1").arg(error));
        return;
    }
    onExportModel();
}

void MainWindow::onNewSurface(int type) {
    CpuMesh mesh;
    switch (type) {
        case 0: mesh = ParametricSurface::plane(); break;
        case 1: mesh = ParametricSurface::sphere(); break;
        case 2: mesh = ParametricSurface::torus(); break;
        case 3: mesh = ParametricSurface::cylinder(); break;
        case 4: mesh = ParametricSurface::cone(); break;
        default: return;
    }

    int target = m_currentDoc;
    if (target < 0 || target >= m_documents.size() ||
        !m_documents[target].view ||
        m_documents[target].view->meshCount() > 0 ||
        !m_documents[target].filePath.isEmpty()) {
        target = createDocument(mesh.name);
    }

    Document& doc = m_documents[target];
    doc.filePath.clear();
    doc.paramsSource.clear();
    doc.params.clear();
    doc.committed.clear();
    doc.undoStack->clear();
    doc.view->setCPUMeshes({mesh}, mesh.name);

    m_tabs->setTabText(target, mesh.name);
    m_currentDoc = -1;
    m_tabs->setCurrentIndex(target);
    activateDocument(target);
    statusBar()->showMessage("Created parametric surface: " + mesh.name, 3000);
    emit modelLoaded();
}

void MainWindow::createSurface(int type) {
    onNewSurface(type);
}

bool MainWindow::exportModelTo(const QString& path) {
    QString error;
    if (exportMeshesTo(path, &error)) {
        return true;
    }
    statusBar()->showMessage("Export failed: " + error, 4000);
    return false;
}

void MainWindow::setMeshTransformValues(int index, const QVector3D& translate,
                                        const QVector3D& rotate,
                                        const QVector3D& scale) {
    if (index < 0 || index >= m_rhiWidget->meshCount()) {
        return;
    }

    if (m_meshParams.size() != m_rhiWidget->meshCount()) {
        m_meshParams.resize(m_rhiWidget->meshCount());
    }
    MeshParams& params = m_meshParams[index];
    params.translate = translate;
    params.rotate = rotate;
    params.scale = scale;
    if (m_committedParams.size() != m_meshParams.size()) {
        m_committedParams = m_meshParams;
    }
    m_committedParams[index] = params;

    QMatrix4x4 transform;
    transform.translate(translate);
    transform.rotate(rotate.x(), 1, 0, 0);
    transform.rotate(rotate.y(), 0, 1, 0);
    transform.rotate(rotate.z(), 0, 0, 1);
    transform.scale(scale);

    m_rhiWidget->setMeshTransform(index, transform);
    updateTransformUi();
}

void MainWindow::setMeshColor(int index, const QVector3D& color) {
    if (index < 0 || index >= m_rhiWidget->meshCount()) {
        return;
    }
    Material material = m_rhiWidget->meshMaterial(index);
    material.diffuse = color;
    material.ambient = color * 0.25f;
    m_rhiWidget->setMeshMaterial(index, material);
}

void MainWindow::onShowModelInfo() {
    QMessageBox::information(this, "Model Information", m_rhiWidget->getModelStatistics());
}

void MainWindow::selectMesh(int index) {
    m_updatingObjectUi = true;
    m_rhiWidget->selectMesh(index);
    m_updatingObjectUi = false;
    refreshObjectList();
    updateTransformUi();
}

void MainWindow::applyViewPreset(int preset) {
    m_rhiWidget->applyViewPreset(preset);
}

bool MainWindow::saveScreenshotTo(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QString format = suffix.isEmpty() ? QString("png") : suffix;
    return m_rhiWidget->saveScreenshotEx(path, format, 95);
}
