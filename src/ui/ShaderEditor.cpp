#include "ShaderEditor.h"

#include "RhiWidget.h"
#include "../renderer/ShaderCompiler.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QTemporaryDir>
#include <QVBoxLayout>

QString ShaderEditor::shaderSourceDirectory() {
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/shaders",
        QDir::currentPath() + "/src/shaders",
        QDir::currentPath() + "/shaders",
    };
    for (const QString& path : candidates) {
        if (QDir(path).exists()) {
            return path;
        }
    }
    return QString();
}

bool ShaderEditor::compileAndApply(RhiWidget* view, const QString& sourcePath,
                                   QString* error) {
    if (!view) {
        if (error) *error = "No active render view";
        return false;
    }

    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = "Cannot open shader: " + sourcePath;
        }
        return false;
    }

    const QString fileName = QFileInfo(sourcePath).fileName();
    QByteArray serialized;
    QString compileError;
    if (!bakeShaderSource(QString::fromUtf8(file.readAll()), fileName,
                          &serialized, &compileError)) {
        if (error) {
            *error = compileError;
        }
        return false;
    }

    // 使用固定临时路径，便于重复覆盖
    const QString tempPath =
        QDir::temp().filePath("modelviewer_override_" + fileName + ".qsb");
    QSaveFile output(tempPath);
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(serialized) != serialized.size() || !output.commit()) {
        if (error) {
            *error = "Cannot write compiled shader: " + tempPath;
        }
        return false;
    }

    view->applyShaderOverride(":/shaders/" + fileName + ".qsb", tempPath);
    return true;
}

ShaderEditor::ShaderEditor(RhiWidget* view, QWidget* parent)
    : QDialog(parent), m_view(view) {
    setWindowTitle("Shader Editor");
    resize(920, 620);

    auto* layout = new QVBoxLayout(this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* leftPanel = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel("Shaders", leftPanel));
    m_list = new QListWidget(leftPanel);
    leftLayout->addWidget(m_list, 1);

    auto* listButtons = new QHBoxLayout();
    auto* reloadButton = new QPushButton("Reload List", leftPanel);
    listButtons->addWidget(reloadButton);
    leftLayout->addLayout(listButtons);
    splitter->addWidget(leftPanel);

    auto* rightPanel = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_editor = new QPlainTextEdit(rightPanel);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlainText(
        "// 选择左侧着色器开始编辑。\n"
        "// 编译成功后会在下一帧重建管线，无需重启程序。");
    rightLayout->addWidget(m_editor, 3);

    auto* buttons = new QHBoxLayout();
    auto* saveButton = new QPushButton("Save", rightPanel);
    auto* compileButton = new QPushButton("Compile && Apply", rightPanel);
    auto* restoreButton = new QPushButton("Restore Original", rightPanel);
    buttons->addWidget(saveButton);
    buttons->addWidget(compileButton);
    buttons->addWidget(restoreButton);
    rightLayout->addLayout(buttons);

    m_status = new QLabel("Ready", rightPanel);
    rightLayout->addWidget(m_status);

    m_log = new QPlainTextEdit(rightPanel);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(400);
    m_log->setFixedHeight(120);
    rightLayout->addWidget(m_log);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter, 1);

    connect(reloadButton, &QPushButton::clicked, this,
            &ShaderEditor::refreshList);
    connect(m_list, &QListWidget::currentRowChanged, this,
            &ShaderEditor::loadCurrent);
    connect(saveButton, &QPushButton::clicked, this, &ShaderEditor::saveCurrent);
    connect(compileButton, &QPushButton::clicked, this,
            &ShaderEditor::compileCurrent);
    connect(restoreButton, &QPushButton::clicked, this,
            &ShaderEditor::restoreCurrent);

    refreshList();
}

void ShaderEditor::refreshList() {
    m_list->clear();
    m_currentPath.clear();

    const QString directory = shaderSourceDirectory();
    if (directory.isEmpty()) {
        appendLog("未找到着色器源码目录（shaders/）。");
        return;
    }

    const QDir dir(directory);
    const QStringList filters = {"*.vert", "*.frag"};
    const QStringList files =
        dir.entryList(filters, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        m_list->addItem(name);
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    m_status->setText(QString("%1 shader(s) in %2")
                          .arg(files.size())
                          .arg(directory));
}

void ShaderEditor::loadCurrent() {
    const QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return;
    }

    const QString path =
        QDir(shaderSourceDirectory()).filePath(item->text());
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog("无法打开: " + path);
        return;
    }

    m_currentPath = path;
    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    setWindowTitle("Shader Editor - " + item->text());
}

void ShaderEditor::saveCurrent() {
    if (m_currentPath.isEmpty()) {
        return;
    }

    QSaveFile file(m_currentPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
        file.write(m_editor->toPlainText().toUtf8()) < 0 ||
        !file.commit()) {
        appendLog("保存失败: " + m_currentPath);
        return;
    }
    appendLog("已保存: " + QFileInfo(m_currentPath).fileName());
}

void ShaderEditor::compileCurrent() {
    if (m_currentPath.isEmpty()) {
        return;
    }

    saveCurrent();

    QString error;
    if (compileAndApply(m_view, m_currentPath, &error)) {
        appendLog("编译成功并已应用: " +
                  QFileInfo(m_currentPath).fileName());
        m_status->setText("Compiled and applied (pipeline rebuild pending)");
    } else {
        appendLog("编译失败:\n" + error);
        m_status->setText("Compilation failed");
    }
}

void ShaderEditor::restoreCurrent() {
    if (m_currentPath.isEmpty()) {
        return;
    }

    const QString resource =
        ":/shaders/" + QFileInfo(m_currentPath).fileName() + ".qsb";
    m_view->clearShaderOverride(resource);
    appendLog("已恢复内置版本: " + QFileInfo(m_currentPath).fileName());
    m_status->setText("Restored built-in shader");
}

void ShaderEditor::appendLog(const QString& text) {
    if (m_log) {
        m_log->appendPlainText(text);
    }
}
