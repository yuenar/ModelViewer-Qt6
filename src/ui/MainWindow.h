#pragma once
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include "RhiWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;
    
private slots:
    void onOpenFile();
    void onFitView();
    void onToggleWireframe();
    void onToggleNormals();
    void onToggleFaceNormals();
    void onToggleVertexNormals();
    void onToggleFlatShading();
    void onToggleProjection();
    void onMaterialSettings();
    void onLightSettings();
    void onScreenshot();
    void onBackgroundSettings();
    
private:
    void setupUI();
    void setupMenus();
    void setupConnections();
    
    RhiWidget* m_rhiWidget = nullptr;
    QAction* m_actionWireframe = nullptr;
    QAction* m_actionNormals = nullptr;
    QAction* m_actionFaceNormals = nullptr;
    QAction* m_actionVertexNormals = nullptr;
    QAction* m_actionFlatShading = nullptr;
    QAction* m_actionOrtho = nullptr;
};
