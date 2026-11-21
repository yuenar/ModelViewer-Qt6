#include "ScriptConsole.h"

#include "RhiWidget.h"
#include "../core/Material.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVector3D>

ViewerScriptApi::ViewerScriptApi(RhiWidget* view, QObject* parent)
    : QObject(parent), m_view(view) {}

int ViewerScriptApi::meshCount() const {
    return m_view ? m_view->meshCount() : 0;
}

QString ViewerScriptApi::meshName(int index) const {
    return m_view ? m_view->meshName(index) : QString();
}

void ViewerScriptApi::selectMesh(int index) {
    if (m_view) m_view->selectMesh(index);
}

void ViewerScriptApi::setMeshVisible(int index, bool visible) {
    if (m_view) m_view->setMeshVisible(index, visible);
}

void ViewerScriptApi::setRenderMode(int mode) {
    if (m_view) m_view->setRenderMode(mode);
}

void ViewerScriptApi::setDisplayStyle(int style) {
    if (m_view) m_view->setDisplayStyle(style);
}

void ViewerScriptApi::setBackground(double r, double g, double b) {
    if (!m_view) return;
    const QVector3D top{float(r), float(g), float(b)};
    m_view->setBackgroundColors(top, top * 0.55f);
}

void ViewerScriptApi::setAxisVisible(bool visible) {
    if (m_view) m_view->setAxisVisible(visible);
}

void ViewerScriptApi::setGridVisible(bool visible) {
    if (m_view) m_view->setGridVisible(visible);
}

void ViewerScriptApi::setMultiView(bool enabled) {
    if (m_view) m_view->setMultiView(enabled);
}

void ViewerScriptApi::setCulling(bool enabled) {
    if (m_view) m_view->setCullingEnabled(enabled);
}

void ViewerScriptApi::setLod(bool enabled) {
    if (m_view) m_view->setLodEnabled(enabled);
}

void ViewerScriptApi::setMsaa(int samples) {
    if (m_view) m_view->setMsaaSampleCount(samples);
}

void ViewerScriptApi::setPostProcessing(bool fxaa, bool ao) {
    if (m_view) m_view->setPostProcessing(fxaa, ao);
}

void ViewerScriptApi::viewPreset(int preset) {
    if (m_view) m_view->applyViewPreset(preset);
}

void ViewerScriptApi::fitView() {
    if (m_view) m_view->fitToView();
}

void ViewerScriptApi::setMeshColor(int index, double r, double g, double b) {
    if (!m_view) return;
    Material material = m_view->meshMaterial(index);
    material.diffuse = QVector3D(float(r), float(g), float(b));
    m_view->setMeshMaterial(index, material);
}

void ViewerScriptApi::setMeshTransform(int index, double tx, double ty,
                                       double tz, double rx, double ry,
                                       double rz, double sx, double sy,
                                       double sz) {
    if (!m_view) return;
    QMatrix4x4 transform;
    transform.translate(float(tx), float(ty), float(tz));
    transform.rotate(float(rx), 1, 0, 0);
    transform.rotate(float(ry), 0, 1, 0);
    transform.rotate(float(rz), 0, 0, 1);
    transform.scale(float(sx), float(sy), float(sz));
    m_view->setMeshTransform(index, transform);
}

int ViewerScriptApi::duplicateMesh(int index) {
    return m_view ? m_view->duplicateMeshAt(index) : -1;
}

bool ViewerScriptApi::deleteMesh(int index) {
    return m_view ? m_view->removeMeshAt(index) : false;
}

bool ViewerScriptApi::renameMesh(int index, const QString& name) {
    return m_view ? m_view->renameMeshAt(index, name) : false;
}

int ViewerScriptApi::animationCount() const {
    return m_view ? m_view->animationCount() : 0;
}

QString ViewerScriptApi::animationName(int index) const {
    return m_view ? m_view->animationName(index) : QString();
}

void ViewerScriptApi::playAnimation(int index) {
    if (m_view) m_view->playAnimation(index);
}

void ViewerScriptApi::stopAnimation() {
    if (m_view) m_view->stopAnimation();
}

void ViewerScriptApi::setAnimationTime(double seconds) {
    if (m_view) m_view->setAnimationTime(float(seconds));
}

QString ViewerScriptApi::stats() const {
    return m_view ? m_view->renderStatsText() : QString();
}

void ScriptConsoleBridge::log(const QString& text) {
    if (m_console) {
        m_console->appendOutput(text);
    }
}

ScriptConsole::ScriptConsole(RhiWidget* view, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Script Console");
    resize(680, 460);

    auto* layout = new QVBoxLayout(this);

    auto* hint = new QLabel(
        "API: viewer.setRenderMode(0-11), setDisplayStyle(0-3), "
        "setBackground(r,g,b), duplicateMesh(i), deleteMesh(i), "
        "renameMesh(i,name), setAnimationTime(t), ...\n"
        "console.log(...) 输出调试信息；Ctrl+Enter 执行。",
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    layout->addWidget(m_output, 1);

    auto* row = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("viewer.setRenderMode(6)");
    auto* runButton = new QPushButton("Run", this);
    auto* fileButton = new QPushButton("Run File...", this);
    row->addWidget(m_input, 1);
    row->addWidget(runButton);
    row->addWidget(fileButton);
    layout->addLayout(row);

    m_api = new ViewerScriptApi(view, this);
    m_engine.globalObject().setProperty("viewer", m_engine.newQObject(m_api));
    m_engine.globalObject().setProperty(
        "console", m_engine.newQObject(new ScriptConsoleBridge(this)));

    connect(runButton, &QPushButton::clicked, this, [this]() {
        const QString code = m_input->text().trimmed();
        if (!code.isEmpty()) {
            m_input->clear();
            runScript(code, "console");
        }
    });
    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        const QString code = m_input->text().trimmed();
        if (!code.isEmpty()) {
            m_input->clear();
            runScript(code, "console");
        }
    });
    connect(fileButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Run Script", QString(), "JavaScript (*.js);;All Files (*)");
        if (!path.isEmpty()) {
            runFile(path);
        }
    });
}

void ScriptConsole::setView(RhiWidget* view) {
    if (m_api) {
        m_api->setView(view);
    }
}

bool ScriptConsole::runScript(const QString& code, const QString& label) {
    const QJSValue result = m_engine.evaluate(code, label);
    if (result.isError()) {
        appendOutput(QString("Error: %1 (line %2)")
                         .arg(result.toString())
                         .arg(result.property("lineNumber").toInt()));
        return false;
    }
    if (!result.isUndefined() && !result.isNull()) {
        appendOutput(result.toString());
    }
    return true;
}

bool ScriptConsole::runFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendOutput("Cannot open script: " + path);
        return false;
    }
    QTextStream stream(&file);
    appendOutput("> " + QFileInfo(path).fileName());
    return runScript(stream.readAll(), path);
}

void ScriptConsole::appendOutput(const QString& text) {
    if (m_output) {
        m_output->appendPlainText(text);
    }
}
