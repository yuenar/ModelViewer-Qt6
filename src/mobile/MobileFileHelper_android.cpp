// Copyright 2026 ModelViewer Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// @brief MobileFileHelper 的 Android 实现（SAF + JNI）
///
/// 流程：
/// 1. 构造 ACTION_OPEN_DOCUMENT intent（OPENABLE + 多 MIME 过滤）；
/// 2. QtAndroidPrivate::startActivity 等待系统文件选择器；
/// 3. RESULT_OK 时拿到 content:// URI；
/// 4. ContentResolver.openInputStream 循环拷贝到应用 cache
///    （Assimp 无法直接读 content URI）；
/// 5. emit fileSelected(本地缓存路径)，recents 记录原始显示名。
///
/// SAF 的 OPEN_DOCUMENT 不需要任何存储权限（对应 android/
/// AndroidManifest.xml 未声明 READ_EXTERNAL_STORAGE）。

#if defined(Q_OS_ANDROID)

#include "MobileFileHelper.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJniEnvironment>
#include <QJniObject>
// QNativeInterface::QAndroidApplication（context()）声明于此头
#include <QtCore/qcoreapplication_platform.h>

#include <QtCore/private/qandroidextras_p.h>

namespace {

constexpr int kPickModelRequestCode = 24680;

/// @brief 与 ModelLoader::isAssimpFormat 的常用格式子集对齐（小写）
bool isKnownModelExtension(const QString& ext) {
  static const QStringList kExtensions = {
      QStringLiteral("obj"),  QStringLiteral("stl"),
      QStringLiteral("fbx"),  QStringLiteral("dae"),
      QStringLiteral("ply"),  QStringLiteral("x"),
      QStringLiteral("3ds"),  QStringLiteral("blend"),
      QStringLiteral("ase"),  QStringLiteral("gltf"),
      QStringLiteral("glb"),  QStringLiteral("bvh"),
      QStringLiteral("csm"),  QStringLiteral("md2"),
      QStringLiteral("md3"),  QStringLiteral("md5mesh"),
      QStringLiteral("md5anim"), QStringLiteral("smd"),
      QStringLiteral("ac"),   QStringLiteral("b3d"),
      QStringLiteral("ms3d"), QStringLiteral("lwo"),
      QStringLiteral("lws")};
  return kExtensions.contains(ext);
}

/// @brief 从 content URI 的 lastPathSegment 尽力提取后缀名
QString extensionFromUri(const QJniObject& uri) {
  const QString segment =
      uri.callObjectMethod<jstring>("getLastPathSegment")
          .toString();
  if (segment.isEmpty()) {
    return QString();
  }
  // 常见形态："primary:Download/bunny.obj"、
  // "raw:/storage/emulated/0/Download/bunny.obj"、"42"（垃圾 ID）
  const QString tail = segment.mid(segment.lastIndexOf(QLatin1Char('/')) + 1);
  const QString ext = QFileInfo(tail).suffix().toLower();
  if (ext.size() >= 1 && ext.size() <= 8 && isKnownModelExtension(ext)) {
    return ext;
  }
  return QString();  // 让 Assimp 按内容探测（CanRead）
}

/// @brief 经 ContentResolver 把 content URI 流拷贝到本地文件
bool copyContentUriToFile(const QJniObject& uri, const QString& destPath) {
  // 公开 API（qcoreapplication_platform.h，Qt 6.2+）：取应用上下文
  const QJniObject context =
      QNativeInterface::QAndroidApplication::context();
  if (!context.isValid()) {
    qWarning("copyContentUriToFile: no android context");
    return false;
  }
  const QJniObject resolver = context.callObjectMethod(
      "getContentResolver", "()Landroid/content/ContentResolver;");
  if (!resolver.isValid()) {
    return false;
  }

  QJniObject stream = resolver.callObjectMethod(
      "openInputStream", "(Landroid/net/Uri;)Ljava/io/InputStream;",
      uri.object<jobject>());
  QJniEnvironment env;
  if (!stream.isValid() || env->ExceptionCheck()) {
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
    qWarning("copyContentUriToFile: openInputStream failed");
    return false;
  }

  QFile dest(destPath);
  if (!dest.open(QIODevice::WriteOnly)) {
    qWarning("copyContentUriToFile: cannot open %s",
             destPath.toUtf8().constData());
    stream.callMethod<void>("close");
    return false;
  }

  constexpr jint kBufferSize = 64 * 1024;
  const jbyteArray buffer = env->NewByteArray(kBufferSize);
  bool ok = true;
  while (true) {
    const jint read = stream.callMethod<jint>("read", "([B)I", buffer);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      ok = false;
      break;
    }
    if (read < 0) {
      break;  // EOF
    }
    jboolean isCopy = JNI_FALSE;
    jbyte* raw = env->GetByteArrayElements(buffer, &isCopy);
    if (!raw) {
      ok = false;
      break;
    }
    if (dest.write(reinterpret_cast<const char*>(raw), qint64(read)) !=
        qint64(read)) {
      ok = false;
    }
    env->ReleaseByteArrayElements(buffer, raw, JNI_ABORT);
    if (!ok) {
      break;
    }
  }
  env->DeleteLocalRef(buffer);
  stream.callMethod<void>("close");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
  dest.close();
  if (!ok) {
    dest.remove();
  }
  return ok;
}

}  // namespace

void MobileFileHelper::pickModel() {
  // 1) ACTION_OPEN_DOCUMENT + OPENABLE + 类型过滤
  QJniObject intent(
      "android/content/Intent", "(Ljava/lang/String;)V",
      QJniObject::fromString(QStringLiteral("android.intent.action.OPEN_DOCUMENT"))
          .object<jstring>());
  intent.callMethod<void>(
      "addCategory", "(Ljava/lang/String;)V",
      QJniObject::fromString(QStringLiteral("android.intent.category.OPENABLE"))
          .object<jstring>());
  intent.callMethod<void>(
      "setType", "(Ljava/lang/String;)V",
      QJniObject::fromString(QStringLiteral("*/*")).object<jstring>());

  {
    // EXTRA_MIME_TYPES：多类型过滤（setType 只允许单一 MIME）
    static const char* kMimeTypes[] = {
        "model/obj",          "model/stl",       "model/gltf+json",
        "model/gltf-binary",  "model/vnd.collada+xml", "application/octet-stream",
        "application/x-binary"};
    QJniEnvironment env;
    const jclass stringClass = env.findClass("java/lang/String");
    if (stringClass) {
      const int count = sizeof(kMimeTypes) / sizeof(kMimeTypes[0]);
      const jobjectArray mimeArray =
          env->NewObjectArray(count, stringClass, nullptr);
      for (int i = 0; i < count; ++i) {
        env->SetObjectArrayElement(
            mimeArray, i,
            QJniObject::fromString(QString::fromLatin1(kMimeTypes[i]))
                .object<jstring>());
      }
      intent.callObjectMethod(
          "putExtra", "(Ljava/lang/String;[Ljava/lang/String;)Landroid/content/Intent;",
          QJniObject::fromString(
              QStringLiteral("android.intent.extra.MIME_TYPES")).object<jstring>(),
          mimeArray);
      env->DeleteLocalRef(mimeArray);
    }
  }

  // 2) 启动系统文件选择器（回调在 GUI 线程）
  QtAndroidPrivate::startActivity(
      intent, kPickModelRequestCode,
      [this](int /*requestCode*/, int resultCode, const QJniObject& data) {
        constexpr int kResultOk = -1;  // Activity.RESULT_OK
        if (resultCode != kResultOk || !data.isValid()) {
          emit filePickCancelled();
          return;
        }

        const QJniObject uri = data.callObjectMethod(
            "getData", "()Landroid/net/Uri;");
        if (!uri.isValid()) {
          emit filePickCancelled();
          return;
        }

        // 3) content:// → cache 本地文件（Assimp 需要真实路径）
        const QString ext = extensionFromUri(uri);
        const QString cacheRoot =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
            QStringLiteral("/modelview_import");
        QDir().mkpath(cacheRoot);
        const QString stamp = QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd_HHmmsszzz"));
        const QString dest = cacheRoot + QLatin1Char('/') + stamp +
                             (ext.isEmpty() ? QString()
                                            : QLatin1Char('.') + ext);

        if (!copyContentUriToFile(uri, dest)) {
          qWarning("MobileFileHelper: failed to copy selected uri");
          emit filePickCancelled();
          return;
        }
        saveToRecents(dest);
        emit fileSelected(dest);
      });
}

#endif  // Q_OS_ANDROID
