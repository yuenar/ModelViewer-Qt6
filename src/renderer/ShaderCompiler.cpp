#include "ShaderCompiler.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QProcess>
#include <QTemporaryDir>

namespace {

/// @brief 查找 Qt 自带的 qsb 着色器编译工具
QString findQsbTool() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString binDir = QLibraryInfo::path(QLibraryInfo::BinariesPath);
    const QStringList candidates = {
#ifdef Q_OS_WIN
        appDir + "/qsb.exe",
        binDir + "/qsb.exe",
#else
        appDir + "/qsb",
        binDir + "/qsb",
#endif
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return QString();
}

}  // namespace

bool shaderStageFromFileName(const QString& fileName, int* stage) {
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (!stage) {
        return false;
    }
    if (suffix == "vert") {
        *stage = 0;
        return true;
    }
    if (suffix == "frag") {
        *stage = 1;
        return true;
    }
    return false;
}

bool bakeShaderSource(const QString& source, const QString& fileName,
                      QByteArray* serialized, QString* error) {
    if (!serialized) {
        return false;
    }
    int stageValue = 0;
    if (!shaderStageFromFileName(fileName, &stageValue)) {
        if (error) {
            *error = QString("Unsupported shader stage: %1").arg(fileName);
        }
        return false;
    }

    const QString qsb = findQsbTool();
    if (qsb.isEmpty()) {
        if (error) {
            *error = "qsb tool not found (expected next to the executable or "
                     "in the Qt bin directory)";
        }
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (error) {
            *error = "Cannot create temporary directory";
        }
        return false;
    }

    const QString inputPath = tempDir.filePath(fileName);
    QFile input(inputPath);
    if (!input.open(QIODevice::WriteOnly | QIODevice::Text) ||
        input.write(source.toUtf8()) < 0) {
        if (error) {
            *error = "Cannot write temporary shader source";
        }
        return false;
    }
    input.close();

    const QString outputPath = tempDir.filePath(fileName + ".qsb");
    QProcess process;
    process.setProgram(qsb);
    process.setArguments({"--glsl", "460", "--hlsl", "50", "--msl", "12",
                          "-o", outputPath, inputPath});
    process.start();
    if (!process.waitForFinished(30000)) {
        process.kill();
        if (error) {
            *error = "qsb timed out";
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        if (error) {
            const QString stderrText =
                QString::fromUtf8(process.readAllStandardError());
            const QString stdoutText =
                QString::fromUtf8(process.readAllStandardOutput());
            *error = (stderrText + "\n" + stdoutText).trimmed();
            if (error->isEmpty()) {
                *error = QString("qsb failed with exit code %1")
                             .arg(process.exitCode());
            }
        }
        return false;
    }

    QFile output(outputPath);
    if (!output.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "qsb produced no output";
        }
        return false;
    }
    *serialized = output.readAll();
    return !serialized->isEmpty();
}
