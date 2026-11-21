#include "ToolEditor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

const char* kToolTemplate =
    "// 自定义工具脚本\n"
    "// 可用 API:\n"
    "//   viewer.setRenderMode(0-11)\n"
    "//   viewer.setDisplayStyle(0-3)\n"
    "//   viewer.setBackground(r, g, b)\n"
    "//   viewer.viewPreset(0-8)\n"
    "//   viewer.setMeshColor(i, r, g, b)\n"
    "//   viewer.duplicateMesh(i) / viewer.deleteMesh(i)\n"
    "//   viewer.setAnimationTime(t) / viewer.playAnimation(i)\n"
    "//   console.log(...)\n"
    "viewer.setRenderMode(6);\n"
    "console.log('tool executed');\n";

}  // namespace

ToolEditor::ToolEditor(const QString& toolDirectory, ScriptRunner runner,
                       QWidget* parent)
    : QDialog(parent), m_directory(toolDirectory), m_runner(std::move(runner)) {
    setWindowTitle("Custom Tool Editor");
    resize(900, 600);

    QDir().mkpath(m_directory);

    auto* layout = new QVBoxLayout(this);
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* leftPanel = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel("Tools (*.js)", leftPanel));
    m_list = new QListWidget(leftPanel);
    leftLayout->addWidget(m_list, 1);
    auto* listButtons = new QHBoxLayout();
    auto* newButton = new QPushButton("New", leftPanel);
    auto* reloadButton = new QPushButton("Reload", leftPanel);
    listButtons->addWidget(newButton);
    listButtons->addWidget(reloadButton);
    leftLayout->addLayout(listButtons);
    splitter->addWidget(leftPanel);

    auto* rightPanel = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_editor = new QPlainTextEdit(rightPanel);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    rightLayout->addWidget(m_editor, 1);

    auto* buttons = new QHBoxLayout();
    auto* saveButton = new QPushButton("Save", rightPanel);
    auto* runButton = new QPushButton("Save && Run", rightPanel);
    auto* deleteButton = new QPushButton("Delete", rightPanel);
    buttons->addWidget(saveButton);
    buttons->addWidget(runButton);
    buttons->addWidget(deleteButton);
    rightLayout->addLayout(buttons);

    m_status = new QLabel("Ready", rightPanel);
    rightLayout->addWidget(m_status);

    m_log = new QPlainTextEdit(rightPanel);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(200);
    m_log->setFixedHeight(100);
    rightLayout->addWidget(m_log);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter, 1);

    connect(newButton, &QPushButton::clicked, this, &ToolEditor::newTool);
    connect(reloadButton, &QPushButton::clicked, this,
            &ToolEditor::refreshList);
    connect(m_list, &QListWidget::currentRowChanged, this,
            &ToolEditor::loadCurrent);
    connect(saveButton, &QPushButton::clicked, this, &ToolEditor::saveCurrent);
    connect(runButton, &QPushButton::clicked, this, &ToolEditor::runCurrent);
    connect(deleteButton, &QPushButton::clicked, this,
            &ToolEditor::deleteCurrent);

    refreshList();
}

void ToolEditor::refreshList() {
    m_list->clear();
    m_currentPath.clear();

    const QDir dir(m_directory);
    const QStringList files =
        dir.entryList({"*.js"}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        m_list->addItem(name);
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    } else {
        m_editor->clear();
    }
    m_status->setText(
        QString("%1 tool(s) in %2").arg(files.size()).arg(m_directory));
}

void ToolEditor::loadCurrent() {
    const QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return;
    }

    const QString path = QDir(m_directory).filePath(item->text());
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog("无法打开: " + path);
        return;
    }
    m_currentPath = path;
    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    setWindowTitle("Custom Tool Editor - " + item->text());
}

void ToolEditor::newTool() {
    bool ok = false;
    QString name = QInputDialog::getText(
        this, "New Tool", "Tool name:", QLineEdit::Normal, "my_tool", &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    name = name.trimmed();
    if (!name.endsWith(".js", Qt::CaseInsensitive)) {
        name += ".js";
    }
    const QString path = QDir(m_directory).filePath(name);
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, "New Tool", "Tool already exists.");
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
        file.write(kToolTemplate) < 0 || !file.commit()) {
        appendLog("创建失败: " + path);
        return;
    }

    refreshList();
    const auto items = m_list->findItems(name, Qt::MatchExactly);
    if (!items.isEmpty()) {
        m_list->setCurrentItem(items.first());
    }
    appendLog("已创建: " + name);
}

void ToolEditor::saveCurrent() {
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

void ToolEditor::runCurrent() {
    if (m_currentPath.isEmpty()) {
        return;
    }

    saveCurrent();
    if (m_runner && m_runner(m_currentPath)) {
        appendLog("已运行: " + QFileInfo(m_currentPath).fileName());
        m_status->setText("Tool executed");
    } else {
        appendLog("运行失败: " + QFileInfo(m_currentPath).fileName());
        m_status->setText("Tool execution failed");
    }
}

void ToolEditor::deleteCurrent() {
    if (m_currentPath.isEmpty()) {
        return;
    }
    const QString name = QFileInfo(m_currentPath).fileName();
    if (QMessageBox::question(this, "Delete Tool",
                              QString("Delete %1?").arg(name)) !=
        QMessageBox::Yes) {
        return;
    }
    if (!QFile::remove(m_currentPath)) {
        appendLog("删除失败: " + m_currentPath);
        return;
    }
    appendLog("已删除: " + name);
    refreshList();
}

void ToolEditor::appendLog(const QString& text) {
    if (m_log) {
        m_log->appendPlainText(text);
    }
}
