#include "MainWindow.h"
#include "../renderer/RhiRenderer.h"
#include "MaterialDialog.h"
#include "LightDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    setupMenus();
    setupConnections();
    
    setWindowTitle("ModelViewer Qt6 RHI");
    resize(1200, 800);
}

void MainWindow::setupUI() {
    m_rhiWidget = new RhiWidget(this);
    setCentralWidget(m_rhiWidget);
}

void MainWindow::setupMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    
    auto* openAction = fileMenu->addAction("&Open Model...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    fileMenu->addSeparator();
    
    auto* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    auto* viewMenu = menuBar()->addMenu("&View");
    
    auto* fitViewAction = viewMenu->addAction("&Fit to View");
    fitViewAction->setShortcut(QKeySequence("F"));
    connect(fitViewAction, &QAction::triggered, this, &MainWindow::onFitView);
    
    viewMenu->addSeparator();
    
    m_actionWireframe = viewMenu->addAction("&Wireframe");
    m_actionWireframe->setCheckable(true);
    connect(m_actionWireframe, &QAction::toggled, this, &MainWindow::onToggleWireframe);
    
    m_actionNormals = viewMenu->addAction("&Vertex Normals");
    m_actionNormals->setCheckable(true);
    connect(m_actionNormals, &QAction::toggled, this, &MainWindow::onToggleNormals);
    
    m_actionFaceNormals = viewMenu->addAction("&Face Normals");
    m_actionFaceNormals->setCheckable(true);
    connect(m_actionFaceNormals, &QAction::toggled, this, &MainWindow::onToggleFaceNormals);
    
    m_actionVertexNormals = viewMenu->addAction("&Vertex Normals Visualization");
    m_actionVertexNormals->setCheckable(true);
    connect(m_actionVertexNormals, &QAction::toggled, this, &MainWindow::onToggleVertexNormals);
    
    m_actionFlatShading = viewMenu->addAction("&Flat Shading");
    m_actionFlatShading->setCheckable(true);
    connect(m_actionFlatShading, &QAction::toggled, this, &MainWindow::onToggleFlatShading);
    
    viewMenu->addSeparator();
    
    m_actionOrtho = viewMenu->addAction("&Orthographic Projection");
    m_actionOrtho->setCheckable(true);
    connect(m_actionOrtho, &QAction::toggled, this, &MainWindow::onToggleProjection);
    
    auto* toolsMenu = menuBar()->addMenu("&Tools");
    
    auto* materialAction = toolsMenu->addAction("&Material Settings...");
    connect(materialAction, &QAction::triggered, this, &MainWindow::onMaterialSettings);
    
    auto* lightAction = toolsMenu->addAction("&Light Settings...");
    connect(lightAction, &QAction::triggered, this, &MainWindow::onLightSettings);
    
    auto* bgAction = toolsMenu->addAction("&Background Settings...");
    connect(bgAction, &QAction::triggered, this, &MainWindow::onBackgroundSettings);
    
    toolsMenu->addSeparator();
    
    auto* screenshotAction = toolsMenu->addAction("&Screenshot...");
    screenshotAction->setShortcut(QKeySequence("F12"));
    connect(screenshotAction, &QAction::triggered, this, &MainWindow::onScreenshot);
}

void MainWindow::setupConnections() {
    // 连接其他信号槽
}

void MainWindow::onOpenFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open Model File",
        QString(),
        "Model Files (*.obj *.stl *.fbx *.dae *.ply);;All Files (*)"
    );
    
    if (!path.isEmpty()) {
        m_rhiWidget->loadModel(path);
    }
}

void MainWindow::onFitView() {
    m_rhiWidget->fitToView();
}

void MainWindow::onToggleWireframe() {
    if (m_actionWireframe->isChecked()) {
        m_actionNormals->setChecked(false);
        m_rhiWidget->setRenderMode(1);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleNormals() {
    if (m_actionNormals->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_rhiWidget->setRenderMode(2);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleFaceNormals() {
    if (m_actionFaceNormals->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_rhiWidget->setRenderMode(3);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleVertexNormals() {
    if (m_actionVertexNormals->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_rhiWidget->setRenderMode(4);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleFlatShading() {
    if (m_actionFlatShading->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_rhiWidget->setRenderMode(5);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleProjection() {
    m_rhiWidget->toggleProjection();
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

void MainWindow::onScreenshot() {
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Save Screenshot",
        QString(),
        "PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)"
    );
    
    if (!path.isEmpty()) {
        m_rhiWidget->saveScreenshot(path);
    }
}

void MainWindow::onBackgroundSettings() {
    // 简单的背景颜色设置对话框
    // 这里可以扩展为更复杂的对话框
    QMessageBox::information(this, "Background Settings", 
        "Background color settings can be configured through the renderer.\n"
        "Currently using default gradient background.");
}
