#include "MainWindow.h"
#include "RhiWidget.h"
#include "MaterialDialog.h"
#include "LightDialog.h"
#include "../renderer/RhiRenderer.h"
#include "../loader/ModelLoader.h"
#include "../loader/ModelExporter.h"
#include "../core/ParametricSurface.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QDialog>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUI();
    setupMenus();
    setupObjectDock();

    connect(&m_loadWatcher, &QFutureWatcher<QVector<CpuMesh>>::finished,
            this, &MainWindow::onLoadFinished);

    setRenderMode(0);
    statusBar()->showMessage("Ready");
    setWindowTitle("ModelViewer Qt6 RHI");
    resize(1280, 860);
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
    if (m_paramsSource != source || m_meshParams.size() != m_rhiWidget->meshCount()) {
        m_paramsSource = source;
        m_meshParams.clear();
        for (int i = 0; i < m_rhiWidget->meshCount(); ++i) {
            m_meshParams.append(MeshParams());
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

    const QVector<CpuMesh> meshes = m_loadWatcher.result();
    if (target < 0 || target >= m_documents.size()) {
        return;
    }

    if (meshes.isEmpty()) {
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
    doc.view->setCPUMeshes(meshes, path);

    m_tabs->setTabText(target, QFileInfo(path).fileName());
    m_currentDoc = -1;
    m_tabs->setCurrentIndex(target);
    activateDocument(target);
    addRecentFile(path);

    statusBar()->showMessage(
        QString("Loaded: %1").arg(QFileInfo(path).fileName()));
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
    MaterialDialog dialog(m_rhiWidget->getMaterial(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_rhiWidget->setMaterial(dialog.getMaterial());
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
