#include "MainWindow.h"
#include "../renderer/RhiRenderer.h"
#include "MaterialDialog.h"
#include "LightDialog.h"
#include <QFileDialog>
#include <QColorDialog>
#include <QInputDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QtMath>

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
    
    // 设置焦点到 RhiWidget，使其能够接收键盘事件
    m_rhiWidget->setFocus();
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
    
    m_actionPBR = viewMenu->addAction("&PBR Rendering");
    m_actionPBR->setCheckable(true);
    connect(m_actionPBR, &QAction::toggled, this, &MainWindow::onTogglePBR);
    
    m_actionShadowMapping = viewMenu->addAction("&Shadow Mapping");
    m_actionShadowMapping->setCheckable(true);
    connect(m_actionShadowMapping, &QAction::toggled, this, &MainWindow::onToggleShadowMapping);
    
    m_actionSkybox = viewMenu->addAction("&Skybox");
    m_actionSkybox->setCheckable(true);
    connect(m_actionSkybox, &QAction::toggled, this, &MainWindow::onToggleSkybox);
    
    m_actionSelection = viewMenu->addAction("&Selection Mode");
    m_actionSelection->setCheckable(true);
    connect(m_actionSelection, &QAction::toggled, this, &MainWindow::onToggleSelection);
    
    m_actionClippingPlane = viewMenu->addAction("&Clipping Plane");
    m_actionClippingPlane->setCheckable(true);
    connect(m_actionClippingPlane, &QAction::toggled, this, &MainWindow::onToggleClippingPlane);
    
    m_actionSplitScreen = viewMenu->addAction("&Split Screen");
    m_actionSplitScreen->setCheckable(true);
    connect(m_actionSplitScreen, &QAction::toggled, this, &MainWindow::onToggleSplitScreen);
    
    viewMenu->addSeparator();
    
    m_actionOrtho = viewMenu->addAction("&Orthographic Projection");
    m_actionOrtho->setCheckable(true);
    connect(m_actionOrtho, &QAction::toggled, this, &MainWindow::onToggleProjection);
    
    viewMenu->addSeparator();
    
    auto* viewPresetsMenu = viewMenu->addMenu("&View Presets");
    
    auto* frontAction = viewPresetsMenu->addAction("&Front");
    frontAction->setShortcut(QKeySequence("1"));
    connect(frontAction, &QAction::triggered, this, &MainWindow::onViewFront);
    
    auto* backAction = viewPresetsMenu->addAction("&Back");
    backAction->setShortcut(QKeySequence("2"));
    connect(backAction, &QAction::triggered, this, &MainWindow::onViewBack);
    
    auto* leftAction = viewPresetsMenu->addAction("&Left");
    leftAction->setShortcut(QKeySequence("3"));
    connect(leftAction, &QAction::triggered, this, &MainWindow::onViewLeft);
    
    auto* rightAction = viewPresetsMenu->addAction("&Right");
    rightAction->setShortcut(QKeySequence("4"));
    connect(rightAction, &QAction::triggered, this, &MainWindow::onViewRight);
    
    auto* topAction = viewPresetsMenu->addAction("&Top");
    topAction->setShortcut(QKeySequence("5"));
    connect(topAction, &QAction::triggered, this, &MainWindow::onViewTop);
    
    auto* bottomAction = viewPresetsMenu->addAction("&Bottom");
    bottomAction->setShortcut(QKeySequence("6"));
    connect(bottomAction, &QAction::triggered, this, &MainWindow::onViewBottom);
    
    auto* isometricAction = viewPresetsMenu->addAction("&Isometric");
    isometricAction->setShortcut(QKeySequence("7"));
    connect(isometricAction, &QAction::triggered, this, &MainWindow::onViewIsometric);
    
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
    
    auto* modelInfoAction = toolsMenu->addAction("&Model Information...");
    connect(modelInfoAction, &QAction::triggered, this, &MainWindow::onShowModelInfo);
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
        "PNG Files (*.png);;JPEG Files (*.jpg);;BMP Files (*.bmp);;All Files (*)"
    );
    
    if (path.isEmpty()) return;
    
    // 获取文件扩展名
    QString format = QFileInfo(path).suffix().toLower();
    if (format.isEmpty()) format = "png";
    
    // 对于 JPEG，可以选择质量
    int quality = 95;
    if (format == "jpg" || format == "jpeg") {
        bool ok;
        quality = QInputDialog::getInt(
            this,
            "JPEG Quality",
            "Select quality (0-100):",
            95, 0, 100, 1, &ok
        );
        if (!ok) return;
    }
    
    m_rhiWidget->saveScreenshotEx(path, format, quality);
}

void MainWindow::onBackgroundSettings() {
    // 背景颜色设置对话框
    QColor topColor = QColorDialog::getColor(QColor(51, 51, 77), this, "Select Top Background Color");
    if (!topColor.isValid()) return;
    
    QColor botColor = QColorDialog::getColor(QColor(26, 26, 38), this, "Select Bottom Background Color");
    if (!botColor.isValid()) return;
    
    QVector3D topVec(topColor.redF(), topColor.greenF(), topColor.blueF());
    QVector3D botVec(botColor.redF(), botColor.greenF(), botColor.blueF());
    
    qDebug() << "Setting background colors: top=" << topVec << "bot=" << botVec;
    m_rhiWidget->setBackgroundColors(topVec, botVec);
}

void MainWindow::onTogglePBR() {
    if (m_actionPBR->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_actionShadowMapping->setChecked(false);
        m_actionSkybox->setChecked(false);
        m_actionSelection->setChecked(false);
        m_actionClippingPlane->setChecked(false);
        m_actionSplitScreen->setChecked(false);
        m_rhiWidget->setRenderMode(6);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleShadowMapping() {
    if (m_actionShadowMapping->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_actionPBR->setChecked(false);
        m_actionSkybox->setChecked(false);
        m_actionSelection->setChecked(false);
        m_actionClippingPlane->setChecked(false);
        m_actionSplitScreen->setChecked(false);
        m_rhiWidget->setRenderMode(7);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleSkybox() {
    if (m_actionSkybox->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_actionPBR->setChecked(false);
        m_actionShadowMapping->setChecked(false);
        m_actionSelection->setChecked(false);
        m_actionClippingPlane->setChecked(false);
        m_actionSplitScreen->setChecked(false);
        m_rhiWidget->setRenderMode(8);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleSelection() {
    if (m_actionSelection->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_actionPBR->setChecked(false);
        m_actionShadowMapping->setChecked(false);
        m_actionSkybox->setChecked(false);
        m_actionClippingPlane->setChecked(false);
        m_actionSplitScreen->setChecked(false);
        m_rhiWidget->setRenderMode(9);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleClippingPlane() {
    if (m_actionClippingPlane->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_actionPBR->setChecked(false);
        m_actionShadowMapping->setChecked(false);
        m_actionSkybox->setChecked(false);
        m_actionSelection->setChecked(false);
        m_actionSplitScreen->setChecked(false);
        m_rhiWidget->setRenderMode(10);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onToggleSplitScreen() {
    if (m_actionSplitScreen->isChecked()) {
        m_actionWireframe->setChecked(false);
        m_actionNormals->setChecked(false);
        m_actionFaceNormals->setChecked(false);
        m_actionVertexNormals->setChecked(false);
        m_actionFlatShading->setChecked(false);
        m_actionPBR->setChecked(false);
        m_actionShadowMapping->setChecked(false);
        m_actionSkybox->setChecked(false);
        m_actionSelection->setChecked(false);
        m_actionClippingPlane->setChecked(false);
        m_rhiWidget->setRenderMode(11);
    } else {
        m_rhiWidget->setRenderMode(0);
    }
}

void MainWindow::onViewFront() {
    // 前视图：沿 Z 轴看向模型
    Camera& camera = m_rhiWidget->getCamera();
    // 使用 fitToView 逻辑来计算合适的距离
    camera.setTarget(QVector3D(0, 0, 0));
    camera.setPosition(QVector3D(0, 0, 10));
    camera.setUp(QVector3D(0, 1, 0));
    m_rhiWidget->fitToView();
}

void MainWindow::onViewBack() {
    // 后视图：沿 -Z 轴看向模型
    Camera& camera = m_rhiWidget->getCamera();
    camera.setTarget(QVector3D(0, 0, 0));
    camera.setPosition(QVector3D(0, 0, -10));
    camera.setUp(QVector3D(0, 1, 0));
    m_rhiWidget->fitToView();
}

void MainWindow::onViewLeft() {
    // 左视图：沿 X 轴看向模型
    Camera& camera = m_rhiWidget->getCamera();
    camera.setTarget(QVector3D(0, 0, 0));
    camera.setPosition(QVector3D(-10, 0, 0));
    camera.setUp(QVector3D(0, 1, 0));
    m_rhiWidget->fitToView();
}

void MainWindow::onViewRight() {
    // 右视图：沿 -X 轴看向模型
    Camera& camera = m_rhiWidget->getCamera();
    camera.setTarget(QVector3D(0, 0, 0));
    camera.setPosition(QVector3D(10, 0, 0));
    camera.setUp(QVector3D(0, 1, 0));
    m_rhiWidget->fitToView();
}

void MainWindow::onViewTop() {
    // 顶视图：从上往下看
    Camera& camera = m_rhiWidget->getCamera();
    camera.setTarget(QVector3D(0, 0, 0));
    camera.setPosition(QVector3D(0, 10, 0));
    camera.setUp(QVector3D(0, 0, 1));  // 使用 Z 轴作为上向量
    m_rhiWidget->fitToView();
}

void MainWindow::onViewBottom() {
    // 底视图：从下往上看
    Camera& camera = m_rhiWidget->getCamera();
    camera.setTarget(QVector3D(0, 0, 0));
    camera.setPosition(QVector3D(0, -10, 0));
    camera.setUp(QVector3D(0, 0, -1));  // 使用 -Z 轴作为上向量
    m_rhiWidget->fitToView();
}

void MainWindow::onViewIsometric() {
    // 等轴测视图：45 度角观看
    Camera& camera = m_rhiWidget->getCamera();
    camera.setTarget(QVector3D(0, 0, 0));
    camera.setPosition(QVector3D(10, 10, 10));
    camera.setUp(QVector3D(0, 1, 0));
    m_rhiWidget->fitToView();
}


void MainWindow::onShowModelInfo() {
    QString info = m_rhiWidget->getModelStatistics();
    QMessageBox::information(this, "Model Information", info);
}
