#include "PluginManager.h"

#include "ModelViewerPlugin.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <cstdio>

PluginManager::PluginManager(QObject* parent) : QObject(parent) {}

PluginManager::~PluginManager() {
    unloadAll();
}

int PluginManager::loadAll(QObject* api) {
    unloadAll();

    const QDir directory(QCoreApplication::applicationDirPath() + "/plugins");
    if (!directory.exists()) {
        return 0;
    }

    const QStringList entries = directory.entryList(QDir::Files);
    for (const QString& entry : entries) {
        const QString suffix = QFileInfo(entry).suffix().toLower();
        if (suffix != "dll" && suffix != "so" && suffix != "dylib") {
            continue;
        }
        if (entry.contains(".qsb", Qt::CaseInsensitive)) {
            continue;
        }

        auto* loader = new QPluginLoader(directory.filePath(entry), this);
        QObject* instance = loader->instance();
        auto* plugin = qobject_cast<ModelViewerPlugin*>(instance);
        if (!plugin) {
            m_failed.append(entry);
            std::printf("NATIVE_PLUGIN_FAILED: %s (%s)\n",
                        entry.toUtf8().constData(),
                        loader->errorString().toUtf8().constData());
            loader->unload();
            delete loader;
            continue;
        }

        if (plugin->activate(api)) {
            m_names.append(plugin->name());
            m_loaders.append(loader);
            std::printf("NATIVE_PLUGIN: %s\n",
                        plugin->name().toUtf8().constData());
        } else {
            m_failed.append(entry);
            loader->unload();
            delete loader;
        }
    }

    std::fflush(stdout);
    return m_names.size();
}

void PluginManager::unloadAll() {
    for (QPluginLoader* loader : m_loaders) {
        if (loader) {
            loader->unload();
            delete loader;
        }
    }
    m_loaders.clear();
    m_names.clear();
    m_failed.clear();
}

QString PluginManager::statusText() const {
    if (m_names.isEmpty()) {
        return m_failed.isEmpty()
                   ? QString("No native plugins loaded")
                   : QString("Native plugins failed: %1")
                         .arg(m_failed.join(", "));
    }
    return QString("Native plugins: %1").arg(m_names.join(", "));
}
