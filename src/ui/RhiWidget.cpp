#include "RhiWidget.h"
#include "../renderer/RhiRenderer.h"
#include "../loader/ModelLoader.h"
#include "../math/BoundingBox.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QTimer>
#include <QCursor>
#include <QDebug>
#include <QHash>
#include <QtMath>
#include <cmath>
#include <limits>

namespace {

/// @brief 叠加在渲染控件上的 HUD 文字层（2D 文字渲染）
class HudOverlay : public QWidget {
 public:
  explicit HudOverlay(RhiWidget* owner)
      : QWidget(owner), m_owner(owner) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() { update(); });
    timer->start(500);
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    if (!m_owner) {
      return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 120));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

    QFont font = painter.font();
    font.setPointSizeF(9.0);
    painter.setFont(font);
    painter.setPen(QColor(235, 235, 235));

    int y = 16;
    const int lineHeight = 16;
    const QRect area = rect().adjusted(10, 6, -10, -6);

    painter.drawText(area.adjusted(0, y, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                     QString("Mode: %1  |  Style: %2")
                         .arg(m_owner->renderModeName(), m_owner->displayStyleName()));
    y += lineHeight;

    painter.drawText(area.adjusted(0, y, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                     QString("Meshes: %1  |  FPS: %2")
                         .arg(m_owner->meshCount())
                         .arg(QString::number(m_owner->fps(), 'f', 1)));
    y += lineHeight;

    const int selected = m_owner->selectedMesh();
    const QString selectedText = (selected >= 0)
        ? QString("Selected: %1").arg(m_owner->meshName(selected))
        : QString("Selected: -");
    painter.drawText(area.adjusted(0, y, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                     selectedText);
    y += lineHeight;

    painter.drawText(area.adjusted(0, y, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                     QString("Axes: %1  |  Grid: %2  |  Multi-View: %3")
                         .arg(m_owner->isAxisVisible() ? "on" : "off",
                              m_owner->isGridVisible() ? "on" : "off",
                              m_owner->isMultiView() ? "on" : "off"));
    y += lineHeight;

    painter.drawText(area.adjusted(0, y, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                     m_owner->renderStatsText());
  }

 private:
  RhiWidget* m_owner = nullptr;
};

}  // namespace

RhiWidget::RhiWidget(QWidget* parent)
    : QRhiWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    m_fpsTimer.start();

    m_hud = new HudOverlay(this);
    m_hud->setGeometry(10, 10, 400, 100);
    m_hud->raise();

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, &RhiWidget::advanceAnimation);

    m_viewRecordTimer = new QTimer(this);
    m_viewRecordTimer->setSingleShot(true);
    m_viewRecordTimer->setInterval(250);
    connect(m_viewRecordTimer, &QTimer::timeout, this,
            &RhiWidget::recordViewState);

    m_viewAnimTimer = new QTimer(this);
    m_viewAnimTimer->setInterval(16);
    connect(m_viewAnimTimer, &QTimer::timeout, this, [this]() {
        const float t = qBound(
            0.0f, float(m_viewAnimClock.elapsed()) / 320.0f, 1.0f);
        const float s = t * t * (3.0f - 2.0f * t);
        m_camera.setPosition(m_viewAnimFrom.eye * (1.0f - s) +
                             m_viewAnimTo.eye * s);
        m_camera.setTarget(m_viewAnimFrom.target * (1.0f - s) +
                           m_viewAnimTo.target * s);
        m_camera.setUp((m_viewAnimFrom.up * (1.0f - s) +
                        m_viewAnimTo.up * s)
                           .normalized());
        if (t >= 1.0f) {
            m_camera.setPosition(m_viewAnimTo.eye);
            m_camera.setTarget(m_viewAnimTo.target);
            m_camera.setUp(m_viewAnimTo.up);
            m_viewAnimTimer->stop();
            m_viewAnimating = false;
            emit viewHistoryChanged();
        }
        update();
    });

    updateCursorForTool();
}

RhiWidget::~RhiWidget() {
    releaseResources();
    qDeleteAll(m_meshes);
    m_meshes.clear();
}

void RhiWidget::releaseResources() {
    for (RhiMesh* mesh : m_meshes) {
        if (mesh) {
            mesh->setBufferPool(nullptr);
            mesh->releaseGpu();
        }
    }

    delete m_renderer;
    m_renderer = nullptr;
}

void RhiWidget::initialize(QRhiCommandBuffer* cb) {
    if (!m_renderer) {
        m_renderer = new RhiRenderer(rhi(), renderTarget());
        m_renderer->setDiffuseTexture(m_pendingDiffuseTexture);
        m_renderer->setPostProcessing(m_fxaaEnabled, m_aoEnabled);
        m_renderer->initialize(cb);
        m_renderer->setBackgroundColors(m_bgTopColor, m_bgBotColor);
        m_renderer->setNormalLength(qMax(m_sceneBbox.radius() * 0.15f, 0.05f));
        m_renderer->setClipPlane(m_clipPlane);
        m_renderer->setSelectedMesh(m_selectedMesh);
        m_renderer->setRenderMode(m_renderMode);
        m_renderer->setDisplayStyle(m_displayStyle);
        m_renderer->setMultiView(m_multiView);
        m_renderer->setAxisVisible(m_axisVisible);
        m_renderer->setGridVisible(m_gridVisible);
        m_renderer->setCullingEnabled(m_cullEnabled);
        m_renderer->setLodEnabled(m_lodEnabled);
    } else {
        m_renderer->setRenderTarget(renderTarget());
        m_renderer->rebuildPipelines();
    }
}

void RhiWidget::render(QRhiCommandBuffer* cb) {
    if (!m_renderer) {
        return;
    }

    const QSize pixelSize = renderTarget()->pixelSize();
    if (pixelSize.height() > 0) {
        m_camera.setAspectRatio(float(pixelSize.width()) / float(pixelSize.height()));
    }

    m_renderer->render(cb, m_meshes, m_camera, m_light, m_material);

    ++m_frameCounter;
    if (m_fpsTimer.elapsed() >= 1000) {
        m_fps = float(m_frameCounter) * 1000.0f / float(m_fpsTimer.elapsed());
        m_frameCounter = 0;
        m_fpsTimer.restart();
    }
}

void RhiWidget::resizeEvent(QResizeEvent* event) {
    QRhiWidget::resizeEvent(event);
    if (m_hud) {
        m_hud->setGeometry(10, 10, qMin(400, width() - 20), 100);
        m_hud->raise();
    }
}

void RhiWidget::updateFps() {
    ++m_frameCounter;
    if (m_fpsTimer.elapsed() >= 1000) {
        m_fps = float(m_frameCounter) * 1000.0f / float(m_fpsTimer.elapsed());
        m_frameCounter = 0;
        m_fpsTimer.restart();
    }
}

void RhiWidget::loadModel(const QString& filePath) {
    if (filePath.isEmpty()) {
        return;
    }

    ModelLoader loader;
    const ModelAsset asset = loader.loadCPU(filePath);
    setCPUMeshes(asset.meshes, filePath);
    setAnimationClips(asset.animations);
}

void RhiWidget::setCPUMeshes(const QVector<CpuMesh>& meshes, const QString& label) {
    m_animationClips.clear();
    m_animClip = -1;
    m_animTime = 0.0f;
    m_animPlaying = false;
    if (m_animTimer) {
        m_animTimer->stop();
    }

    for (RhiMesh* mesh : m_meshes) {
        delete mesh;
    }
    m_meshes.clear();
    m_sceneBbox = BoundingBox();
    m_selectedMesh = -1;
    m_sourceLabel = label;

    int index = 0;
    for (const CpuMesh& cpuMesh : meshes) {
        auto* mesh = new RhiMesh(nullptr, cpuMesh);
        mesh->setName(cpuMesh.name.isEmpty()
                          ? QString("Mesh %1").arg(index)
                          : cpuMesh.name);
        ++index;
        m_meshes.append(mesh);
        m_sceneBbox.expand(cpuMesh.bbox);
    }

    if (!meshes.isEmpty()) {
        m_material.diffuse = meshes.first().baseColor;
    }

    m_pendingDiffuseTexture = QImage();
    for (const CpuMesh& cpuMesh : meshes) {
        if (!cpuMesh.diffuseTexture.isNull()) {
            m_pendingDiffuseTexture = cpuMesh.diffuseTexture;
            break;
        }
    }
    if (m_renderer) {
        m_renderer->setDiffuseTexture(m_pendingDiffuseTexture);
    }

    m_camera.fitToView(m_sceneBbox);
    m_viewHistory.clear();
    m_viewHistoryIndex = -1;
    recordViewState();

    if (m_renderer) {
        m_renderer->resetMeshUploadFlag();
        m_renderer->setNormalLength(qMax(m_sceneBbox.radius() * 0.15f, 0.05f));
        m_clipPlane = QVector4D(-1.0f, 0.0f, 0.0f, m_sceneBbox.center().x());
        m_renderer->setClipPlane(m_clipPlane);
        m_renderer->setSelectedMesh(-1);
    }

    emit meshesChanged();
    update();
}

QString RhiWidget::meshName(int index) const {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return QString();
    }
    return m_meshes[index]->name();
}

QVector<CpuMesh> RhiWidget::cpuMeshes() const {
    QVector<CpuMesh> result;
    result.reserve(m_meshes.size());
    for (const RhiMesh* mesh : m_meshes) {
        if (mesh) {
            result.append(mesh->cpuMesh());
        }
    }
    return result;
}

bool RhiWidget::isMeshVisible(int index) const {
    return (index >= 0 && index < m_meshes.size() && m_meshes[index])
               ? m_meshes[index]->isVisible()
               : false;
}

void RhiWidget::setMeshVisible(int index, bool visible) {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return;
    }
    m_meshes[index]->setVisible(visible);
    if (!visible && m_selectedMesh == index) {
        m_selectedMesh = -1;
        if (m_renderer) {
            m_renderer->setSelectedMesh(-1);
        }
    }
    emit meshesChanged();
    update();
}

QMatrix4x4 RhiWidget::meshTransform(int index) const {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return QMatrix4x4();
    }
    return m_meshes[index]->transform();
}

void RhiWidget::setMeshTransform(int index, const QMatrix4x4& transform) {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return;
    }

    m_meshes[index]->setTransform(transform);

    m_sceneBbox = BoundingBox();
    for (const RhiMesh* mesh : m_meshes) {
        if (mesh) {
            m_sceneBbox.expand(mesh->cpuMesh().bbox);
        }
    }

    if (m_renderer) {
        m_renderer->resetMeshUploadFlag();
    }
    emit meshesChanged();
    update();
}

void RhiWidget::resetMeshTransform(int index) {
    setMeshTransform(index, QMatrix4x4());
}

void RhiWidget::showAllMeshes(bool visible) {
    for (RhiMesh* mesh : m_meshes) {
        if (mesh) {
            mesh->setVisible(visible);
        }
    }
    if (!visible) {
        m_selectedMesh = -1;
        if (m_renderer) {
            m_renderer->setSelectedMesh(-1);
        }
    }
    emit meshesChanged();
    update();
}

void RhiWidget::swapMeshVisibility() {
    for (RhiMesh* mesh : m_meshes) {
        if (mesh) {
            mesh->setVisible(!mesh->isVisible());
        }
    }
    emit meshesChanged();
    update();
}

bool RhiWidget::removeMeshAt(int index) {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return false;
    }

    RhiMesh* mesh = m_meshes.takeAt(index);
    if (m_renderer) {
        mesh->setBufferPool(nullptr);
    }
    delete mesh;

    if (m_selectedMesh == index) {
        m_selectedMesh = -1;
    } else if (m_selectedMesh > index) {
        --m_selectedMesh;
    }

    m_sceneBbox = BoundingBox();
    for (const RhiMesh* item : m_meshes) {
        if (item) {
            m_sceneBbox.expand(item->cpuMesh().bbox);
        }
    }

    if (m_renderer) {
        m_renderer->setSelectedMesh(m_selectedMesh);
        m_renderer->resetMeshUploadFlag();
    }
    emit meshesChanged();
    update();
    return true;
}

int RhiWidget::duplicateMeshAt(int index) {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return -1;
    }

    RhiMesh* copy = m_meshes[index]->clone();
    copy->setName(m_meshes[index]->name() + " Copy");
    m_meshes.insert(index + 1, copy);

    if (m_renderer) {
        m_renderer->resetMeshUploadFlag();
    }
    emit meshesChanged();
    update();
    return index + 1;
}

bool RhiWidget::renameMeshAt(int index, const QString& name) {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index] ||
        name.trimmed().isEmpty()) {
        return false;
    }
    m_meshes[index]->setName(name.trimmed());
    emit meshesChanged();
    update();
    return true;
}

void RhiWidget::setRenderMode(int mode) {
    m_renderMode = mode;
    if (m_renderer) {
        m_renderer->setRenderMode(mode);
    }
    update();
}

void RhiWidget::setDisplayStyle(int style) {
    m_displayStyle = style;
    if (m_renderer) {
        m_renderer->setDisplayStyle(style);
    }
    update();
}

void RhiWidget::setMultiView(bool enabled) {
    m_multiView = enabled;
    if (m_renderer) {
        m_renderer->setMultiView(enabled);
    }
    update();
}

void RhiWidget::setMaterial(const Material& mat) {
    m_material = mat;
    for (RhiMesh* mesh : m_meshes) {
        if (mesh) {
            mesh->setMaterial(mat);
        }
    }
    update();
}

void RhiWidget::setMeshMaterial(int index, const Material& mat) {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return;
    }
    m_meshes[index]->setMaterial(mat);
    m_material = mat;
    update();
}

Material RhiWidget::meshMaterial(int index) const {
    if (index < 0 || index >= m_meshes.size() || !m_meshes[index]) {
        return m_material;
    }
    return m_meshes[index]->material();
}

void RhiWidget::applyMaterialPreset(const QString& name) {
    struct Preset {
        const char* name;
        QVector3D ambient;
        QVector3D diffuse;
        QVector3D specular;
        float shininess;
        float metallic;
        float roughness;
    };

    static const Preset presets[] = {
        {"Brass",     {0.33f, 0.22f, 0.03f}, {0.78f, 0.57f, 0.11f}, {0.99f, 0.94f, 0.81f}, 27.9f, 1.0f, 0.25f},
        {"Bronze",    {0.21f, 0.13f, 0.05f}, {0.71f, 0.43f, 0.18f}, {0.39f, 0.27f, 0.17f}, 25.6f, 1.0f, 0.35f},
        {"Chrome",    {0.25f, 0.25f, 0.25f}, {0.55f, 0.55f, 0.55f}, {0.77f, 0.77f, 0.77f}, 76.8f, 1.0f, 0.12f},
        {"Copper",    {0.19f, 0.07f, 0.02f}, {0.70f, 0.27f, 0.08f}, {0.26f, 0.14f, 0.09f}, 12.8f, 1.0f, 0.30f},
        {"Emerald",   {0.02f, 0.17f, 0.07f}, {0.08f, 0.62f, 0.32f}, {0.63f, 0.73f, 0.66f}, 76.8f, 0.0f, 0.20f},
        {"Gold",      {0.25f, 0.20f, 0.04f}, {0.75f, 0.61f, 0.23f}, {0.63f, 0.56f, 0.37f}, 51.2f, 1.0f, 0.18f},
        {"Jade",      {0.05f, 0.13f, 0.09f}, {0.32f, 0.61f, 0.45f}, {0.30f, 0.34f, 0.30f}, 12.8f, 0.0f, 0.35f},
        {"Obsidian",  {0.05f, 0.05f, 0.05f}, {0.19f, 0.19f, 0.19f}, {0.30f, 0.30f, 0.30f}, 38.4f, 0.0f, 0.15f},
        {"Pearl",     {0.25f, 0.23f, 0.21f}, {0.83f, 0.79f, 0.75f}, {0.30f, 0.30f, 0.28f}, 11.3f, 0.0f, 0.40f},
        {"Ruby",      {0.17f, 0.02f, 0.02f}, {0.61f, 0.07f, 0.07f}, {0.73f, 0.63f, 0.63f}, 76.8f, 0.0f, 0.15f},
        {"Silver",    {0.19f, 0.19f, 0.19f}, {0.51f, 0.51f, 0.51f}, {0.75f, 0.75f, 0.75f}, 51.2f, 1.0f, 0.20f},
        {"Turquoise", {0.10f, 0.19f, 0.20f}, {0.25f, 0.76f, 0.75f}, {0.30f, 0.31f, 0.31f}, 12.8f, 0.0f, 0.35f},
    };

    for (const Preset& preset : presets) {
        if (name.compare(QString::fromLatin1(preset.name), Qt::CaseInsensitive) == 0) {
            m_material.ambient = preset.ambient;
            m_material.diffuse = preset.diffuse;
            m_material.specular = preset.specular;
            m_material.shininess = preset.shininess;
            m_material.metallic = preset.metallic;
            m_material.roughness = preset.roughness;
            setMaterial(m_material);
            update();
            return;
        }
    }
}

void RhiWidget::setLight(const Light& light) {
    m_light = light;
    update();
}

void RhiWidget::setBackgroundColors(const QVector3D& topColor,
                                    const QVector3D& botColor) {
    m_bgTopColor = topColor;
    m_bgBotColor = botColor;
    if (m_renderer) {
        m_renderer->setBackgroundColors(topColor, botColor);
    }
    update();
}

void RhiWidget::fitToView() {
    if (!m_sceneBbox.isEmpty()) {
        m_camera.fitToView(m_sceneBbox);
        recordViewState();
        update();
    }
}

void RhiWidget::applyViewPreset(int preset) {
    if (m_sceneBbox.isEmpty()) {
        return;
    }
    m_camera.setViewPreset(preset, m_sceneBbox);
    recordViewState();
    update();
}

void RhiWidget::setProjectionType(ProjectionType type) {
    m_camera.setProjectionType(type);
    emit projectionChanged(isOrthographic());
    update();
}

void RhiWidget::toggleProjection() {
    m_camera.setProjectionType(m_camera.projectionType() == ProjectionType::Perspective
                                   ? ProjectionType::Orthographic
                                   : ProjectionType::Perspective);
    emit projectionChanged(isOrthographic());
    update();
}

bool RhiWidget::isOrthographic() const {
    return m_camera.projectionType() == ProjectionType::Orthographic;
}

void RhiWidget::setNormalLength(float length) {
    if (m_renderer) {
        m_renderer->setNormalLength(length);
    }
    update();
}

void RhiWidget::setClippingPlane(const QVector4D& plane) {
    m_clipPlane = plane;
    if (m_renderer) {
        m_renderer->setClipPlane(plane);
    }
    update();
}

void RhiWidget::selectMesh(int index) {
    m_selectedMesh = index;
    if (m_renderer) {
        m_renderer->setSelectedMesh(index);
    }
    update();
}

void RhiWidget::setAxisVisible(bool visible) {
    m_axisVisible = visible;
    if (m_renderer) {
        m_renderer->setAxisVisible(visible);
    }
    update();
}

void RhiWidget::setGridVisible(bool visible) {
    m_gridVisible = visible;
    if (m_renderer) {
        m_renderer->setGridVisible(visible);
    }
    update();
}

void RhiWidget::setToolMode(ToolMode mode) {
    m_toolMode = mode;
    updateCursorForTool();
}

void RhiWidget::updateCursorForTool() {
    QString resource;
    Qt::CursorShape fallback = Qt::ArrowCursor;

    switch (m_toolMode) {
        case ToolMode::Rotate:
            resource = ":/res/rotatecursor.png";
            fallback = Qt::OpenHandCursor;
            break;
        case ToolMode::Pan:
            resource = ":/res/pancursor.png";
            fallback = Qt::SizeAllCursor;
            break;
        case ToolMode::Zoom:
            resource = ":/res/zoomcursor.png";
            fallback = Qt::PointingHandCursor;
            break;
        case ToolMode::Select:
            fallback = Qt::ArrowCursor;
            break;
    }

    if (!resource.isEmpty()) {
        const QPixmap pixmap(resource);
        if (!pixmap.isNull()) {
            setCursor(QCursor(pixmap, pixmap.width() / 2, pixmap.height() / 2));
            return;
        }
    }
    setCursor(fallback);
}

QString RhiWidget::renderModeName() const {
    static const char* names[] = {
        "Phong", "Wireframe", "Vertex Normals", "Face Normals",
        "Vertex Normals Visualization", "Flat Shading", "PBR",
        "Shadow Mapping", "Skybox", "Selection", "Clipping Plane",
        "Split Screen",
    };
    if (m_renderMode >= 0 && m_renderMode < 12) {
        return QString::fromLatin1(names[m_renderMode]);
    }
    return "Unknown";
}

QString RhiWidget::displayStyleName() const {
    static const char* names[] = {"Shaded", "Wireframe", "Wire-Shaded", "Realistic"};
    if (m_displayStyle >= 0 && m_displayStyle < 4) {
        return QString::fromLatin1(names[m_displayStyle]);
    }
    return "Unknown";
}

QString RhiWidget::renderStatsText() const {
    if (!m_renderer) {
        return QString();
    }
    const RenderStats& stats = m_renderer->stats();
    return QString("Draw: %1  Cull: %2  LOD: %3/%4/%5  Pool: %6")
        .arg(stats.drawn)
        .arg(stats.culled)
        .arg(stats.lod0)
        .arg(stats.lod1)
        .arg(stats.lod2)
        .arg(stats.pooledBuffers);
}

void RhiWidget::setCullingEnabled(bool enabled) {
    m_cullEnabled = enabled;
    if (m_renderer) {
        m_renderer->setCullingEnabled(enabled);
    }
    update();
}

void RhiWidget::setLodEnabled(bool enabled) {
    m_lodEnabled = enabled;
    if (m_renderer) {
        m_renderer->setLodEnabled(enabled);
    }
    update();
}

void RhiWidget::setMsaaSampleCount(int samples) {
    samples = qBound(1, samples, 8);
    if (sampleCount() == samples) {
        return;
    }

    // MSAA 与后处理互斥：开启 MSAA 时自动关闭离屏后处理
    if (samples > 1 && (m_fxaaEnabled || m_aoEnabled)) {
        m_fxaaEnabled = false;
        m_aoEnabled = false;
        if (m_renderer) {
            m_renderer->setPostProcessing(false, false);
        }
    }

    QRhiWidget::setSampleCount(samples);
    update();
}

void RhiWidget::setPostProcessing(bool fxaa, bool ao) {
    m_fxaaEnabled = fxaa;
    m_aoEnabled = ao;

    if (fxaa || ao) {
        if (sampleCount() > 1) {
            QRhiWidget::setSampleCount(1);
        }
    }

    if (m_renderer) {
        m_renderer->setPostProcessing(fxaa, ao);
    }
    update();
}

void RhiWidget::setAnimationClips(const QVector<AnimationClip>& clips) {
    m_animationClips = clips;
    m_animClip = clips.isEmpty() ? -1 : 0;
    m_animTime = 0.0f;
    m_animPlaying = false;
    if (m_animTimer) {
        m_animTimer->stop();
    }
}

QString RhiWidget::animationName(int index) const {
    if (index < 0 || index >= m_animationClips.size()) {
        return QString();
    }
    return m_animationClips[index].name;
}

float RhiWidget::animationDuration(int index) const {
    if (index < 0 || index >= m_animationClips.size()) {
        return 0.0f;
    }
    return m_animationClips[index].duration;
}

void RhiWidget::playAnimation(int index) {
    if (index < 0 || index >= m_animationClips.size()) {
        return;
    }
    if (index != m_animClip) {
        m_animClip = index;
        m_animTime = 0.0f;
    }
    m_animPlaying = true;
    m_animClock.restart();
    if (m_animTimer) {
        m_animTimer->start();
    }
    applyAnimationTime(m_animTime);
}

void RhiWidget::pauseAnimation() {
    m_animPlaying = false;
    if (m_animTimer) {
        m_animTimer->stop();
    }
}

void RhiWidget::stopAnimation() {
    m_animPlaying = false;
    if (m_animTimer) {
        m_animTimer->stop();
    }
    m_animTime = 0.0f;
    resetAnimationDeltas();
}

void RhiWidget::setAnimationTime(float seconds) {
    if (m_animClip < 0 || m_animClip >= m_animationClips.size()) {
        return;
    }
    const float duration = m_animationClips[m_animClip].duration;
    m_animTime = qBound(0.0f, seconds, qMax(duration, 0.0f));
    applyAnimationTime(m_animTime);
}

void RhiWidget::advanceAnimation() {
    if (!m_animPlaying || m_animClip < 0 ||
        m_animClip >= m_animationClips.size()) {
        return;
    }

    const float deltaSeconds = float(m_animClock.restart()) / 1000.0f;
    const float duration = m_animationClips[m_animClip].duration;
    m_animTime += deltaSeconds * m_animSpeed;
    if (duration > 1e-4f) {
        m_animTime = std::fmod(m_animTime, duration);
        if (m_animTime < 0.0f) {
            m_animTime += duration;
        }
    } else {
        m_animTime = 0.0f;
    }

    applyAnimationTime(m_animTime);
}

void RhiWidget::applyAnimationTime(float time) {
    if (m_animClip < 0 || m_animClip >= m_animationClips.size()) {
        return;
    }

    const AnimationClip& clip = m_animationClips[m_animClip];
    QHash<QString, const AnimationChannel*> channels;
    channels.reserve(clip.channels.size());
    for (const AnimationChannel& channel : clip.channels) {
        channels.insert(channel.nodeName, &channel);
    }

    bool changed = false;
    for (RhiMesh* mesh : m_meshes) {
        if (!mesh) {
            continue;
        }

        if (mesh->isSkinned()) {
            // 骨骼蒙皮：把动画通道转换为骨骼局部姿态
            QHash<QString, QMatrix4x4> poses;
            poses.reserve(channels.size());
            for (auto it = channels.constBegin(); it != channels.constEnd();
                 ++it) {
                poses.insert(it.key(), it.value()->sample(time));
            }
            mesh->applyBonePoses(poses);
            changed = true;
            continue;
        }

        const CpuMesh& base = mesh->baseMesh();
        if (base.nodePath.isEmpty()) {
            continue;
        }

        QMatrix4x4 world;
        for (int i = 0; i < base.nodePath.size(); ++i) {
            const QMatrix4x4 rest =
                (i < base.nodeRestLocals.size()) ? base.nodeRestLocals[i]
                                                 : QMatrix4x4();
            auto it = channels.constFind(base.nodePath[i]);
            world *= (it != channels.constEnd()) ? it.value()->sample(time)
                                                 : rest;
        }

        mesh->setAnimationDelta(world * base.restWorld.inverted());
        changed = true;
    }

    if (changed && m_renderer) {
        m_renderer->resetMeshUploadFlag();
    }
    update();
}

void RhiWidget::resetAnimationDeltas() {
    bool changed = false;
    for (RhiMesh* mesh : m_meshes) {
        if (!mesh) {
            continue;
        }
        if (mesh->isSkinned()) {
            mesh->resetSkinPose();
            changed = true;
        } else if (!mesh->animationDelta().isIdentity()) {
            mesh->setAnimationDelta(QMatrix4x4());
            changed = true;
        }
    }
    if (changed && m_renderer) {
        m_renderer->resetMeshUploadFlag();
    }
    update();
}

RhiWidget::ViewState RhiWidget::currentViewState() const {
    ViewState state;
    state.eye = m_camera.position();
    state.target = m_camera.target();
    state.up = m_camera.upDirection();
    return state;
}

bool RhiWidget::viewStateNear(const ViewState& a, const ViewState& b) const {
    const float epsilon = 1e-4f;
    return (a.eye - b.eye).lengthSquared() < epsilon &&
           (a.target - b.target).lengthSquared() < epsilon &&
           (a.up - b.up).lengthSquared() < epsilon;
}

void RhiWidget::recordViewState() {
    if (m_viewAnimating) {
        return;
    }

    const ViewState state = currentViewState();
    if (m_viewHistoryIndex >= 0 &&
        m_viewHistoryIndex < m_viewHistory.size() &&
        viewStateNear(m_viewHistory[m_viewHistoryIndex], state)) {
        return;
    }

    // 丢弃当前位置之后的前进记录
    while (m_viewHistory.size() > m_viewHistoryIndex + 1) {
        m_viewHistory.removeLast();
    }

    m_viewHistory.append(state);
    if (m_viewHistory.size() > 32) {
        m_viewHistory.removeFirst();
    }
    m_viewHistoryIndex = m_viewHistory.size() - 1;
    emit viewHistoryChanged();
}

void RhiWidget::goBack() {
    if (!canGoBack()) {
        return;
    }
    --m_viewHistoryIndex;
    animateToViewState(m_viewHistory[m_viewHistoryIndex]);
    emit viewHistoryChanged();
}

void RhiWidget::goForward() {
    if (!canGoForward()) {
        return;
    }
    ++m_viewHistoryIndex;
    animateToViewState(m_viewHistory[m_viewHistoryIndex]);
    emit viewHistoryChanged();
}

void RhiWidget::animateToViewState(const ViewState& state) {
    m_viewAnimFrom = currentViewState();
    m_viewAnimTo = state;
    m_viewAnimating = true;
    m_viewAnimClock.restart();
    if (m_viewAnimTimer) {
        m_viewAnimTimer->start();
    }
}

QVector3D RhiWidget::unproject(const QPoint& pos, float ndcZ) const {
    const QSize viewport = size();
    if (viewport.width() <= 0 || viewport.height() <= 0) {
        return QVector3D();
    }

    const float ndcX = 2.0f * float(pos.x()) / float(viewport.width()) - 1.0f;
    const float ndcY = 1.0f - 2.0f * float(pos.y()) / float(viewport.height());

    const QMatrix4x4 proj = m_camera.projectionMatrix();
    const QMatrix4x4 view = m_camera.viewMatrix();
    const QMatrix4x4 inv = (proj * view).inverted();

    QVector4D p = inv * QVector4D(ndcX, ndcY, ndcZ, 1.0f);
    if (qAbs(p.w()) < 1e-6f) {
        return QVector3D();
    }
    return p.toVector3D() / p.w();
}

int RhiWidget::pickMesh(const QPoint& pos) const {
    if (m_meshes.isEmpty()) {
        return -1;
    }

    const QVector3D nearPoint = unproject(pos, -1.0f);
    const QVector3D farPoint = unproject(pos, 1.0f);
    const QVector3D direction = (farPoint - nearPoint).normalized();
    if (direction.lengthSquared() < 1e-8f) {
        return -1;
    }

    int bestIndex = -1;
    float bestT = std::numeric_limits<float>::max();

    for (int meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex) {
        const RhiMesh* mesh = m_meshes[meshIndex];
        if (!mesh || !mesh->isVisible()) {
            continue;
        }
        const CpuMesh& cpu = mesh->cpuMesh();
        if (cpu.bbox.isEmpty()) {
            continue;
        }

        const int triangleCount = cpu.indices.size() / 3;
        for (int tri = 0; tri < triangleCount; ++tri) {
            const quint32 i0 = cpu.indices[tri * 3 + 0];
            const quint32 i1 = cpu.indices[tri * 3 + 1];
            const quint32 i2 = cpu.indices[tri * 3 + 2];
            if (i0 >= static_cast<quint32>(cpu.vertices.size()) ||
                i1 >= static_cast<quint32>(cpu.vertices.size()) ||
                i2 >= static_cast<quint32>(cpu.vertices.size())) {
                continue;
            }

            const QVector3D& v0 = cpu.vertices[i0].position;
            const QVector3D& v1 = cpu.vertices[i1].position;
            const QVector3D& v2 = cpu.vertices[i2].position;

            const QVector3D edge1 = v1 - v0;
            const QVector3D edge2 = v2 - v0;
            const QVector3D h = QVector3D::crossProduct(direction, edge2);
            const float a = QVector3D::dotProduct(edge1, h);
            if (qAbs(a) < 1e-7f) {
                continue;
            }
            const float invA = 1.0f / a;
            const QVector3D s = nearPoint - v0;
            const float u = invA * QVector3D::dotProduct(s, h);
            if (u < 0.0f || u > 1.0f) {
                continue;
            }
            const QVector3D q = QVector3D::crossProduct(s, edge1);
            const float v = invA * QVector3D::dotProduct(direction, q);
            if (v < 0.0f || u + v > 1.0f) {
                continue;
            }
            const float t = invA * QVector3D::dotProduct(edge2, q);
            if (t > 1e-4f && t < bestT) {
                bestT = t;
                bestIndex = meshIndex;
            }
        }
    }

    return bestIndex;
}

void RhiWidget::mousePressEvent(QMouseEvent* e) {
    m_lastMousePos = e->pos();
    m_pressPos = e->pos();
    m_dragged = false;
    m_activeButton = e->button();
    if (e->button() == Qt::LeftButton) {
        m_camera.beginRotate(e->pos());
    }
}

void RhiWidget::mouseMoveEvent(QMouseEvent* e) {
    if ((e->pos() - m_pressPos).manhattanLength() > 3) {
        m_dragged = true;
    }

    const QPoint delta = e->pos() - m_lastMousePos;

    if (m_activeButton == Qt::LeftButton) {
        switch (m_toolMode) {
            case ToolMode::Pan:
                m_camera.pan(delta);
                break;
            case ToolMode::Zoom:
                m_camera.zoom(-float(delta.y()) * 0.01f);
                break;
            case ToolMode::Rotate:
            case ToolMode::Select:
            default:
                m_camera.rotate(m_lastMousePos, e->pos(), size());
                break;
        }
    } else if (m_activeButton == Qt::MiddleButton ||
               m_activeButton == Qt::RightButton) {
        m_camera.pan(delta);
    }

    m_lastMousePos = e->pos();
    update();
}

void RhiWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && !m_dragged && m_renderer &&
        (m_renderMode == static_cast<int>(RenderMode::Selection) ||
         m_toolMode == ToolMode::Select)) {
        m_selectedMesh = pickMesh(e->pos());
        m_renderer->setSelectedMesh(m_selectedMesh);
        emit meshesChanged();
        update();
    }
    if (m_dragged) {
        recordViewState();
    }
    m_activeButton = Qt::NoButton;
    m_dragged = false;
}

void RhiWidget::wheelEvent(QWheelEvent* e) {
    m_camera.zoom(e->angleDelta().y() / 120.0f);
    if (m_viewRecordTimer) {
        m_viewRecordTimer->start();
    }
    update();
}

void RhiWidget::saveScreenshot(const QString& filePath) {
    const QImage image = grabFramebuffer();
    if (image.isNull()) {
        qWarning() << "Failed to grab framebuffer";
        return;
    }
    if (!image.save(filePath)) {
        qWarning() << "Failed to save screenshot to" << filePath;
    }
}

bool RhiWidget::saveScreenshotEx(const QString& filePath, const QString& format,
                                 int quality) {
    const QImage image = grabFramebuffer();
    if (image.isNull()) {
        qWarning() << "Failed to grab framebuffer";
        return false;
    }

    bool success = false;
    if (format.compare("jpg", Qt::CaseInsensitive) == 0 ||
        format.compare("jpeg", Qt::CaseInsensitive) == 0) {
        success = image.save(filePath, "JPEG", quality);
    } else {
        const QByteArray fmt = format.toUpper().toLatin1();
        success = image.save(filePath, fmt.constData(), quality);
    }

    if (!success) {
        qWarning() << "Failed to save screenshot to" << filePath;
    }
    return success;
}

bool RhiWidget::saveScreenshotScaled(const QString& filePath, int width, int height) {
    const QImage image = grabFramebuffer();
    if (image.isNull()) {
        qWarning() << "Failed to grab framebuffer";
        return false;
    }

    const QImage scaled = image.scaled(qMax(1, width), qMax(1, height),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return scaled.save(filePath);
}

QString RhiWidget::getModelStatistics() const {
    if (m_meshes.isEmpty()) {
        return "No model loaded";
    }

    quint64 totalVertices = 0;
    quint64 totalIndices = 0;
    for (const RhiMesh* mesh : m_meshes) {
        if (mesh) {
            totalVertices += mesh->vertexCount();
            totalIndices += mesh->indexCount();
        }
    }

    const QVector3D minPt = m_sceneBbox.min();
    const QVector3D maxPt = m_sceneBbox.max();
    const QVector3D sizePt = m_sceneBbox.size();

    QString stats = QString(
        "Model Statistics:\n"
        "Source: %1\n"
        "Meshes: %2\n"
        "Vertices: %3\n"
        "Triangles: %4\n"
        "Bounding Box:\n"
        "  Min: (%5, %6, %7)\n"
        "  Max: (%8, %9, %10)\n"
        "  Size: (%11, %12, %13)\n"
        "\n"
        "Material:\n"
        "  Ambient: (%14, %15, %16)\n"
        "  Diffuse: (%17, %18, %19)\n"
        "  Specular: (%20, %21, %22)\n"
        "  Shininess: %23\n"
        "  Metallic: %24\n"
        "  Roughness: %25\n"
        "\n"
        "Light:\n"
        "  Position: (%26, %27, %28)\n"
        "  Color: (%29, %30, %31)")
        .arg(m_sourceLabel)
        .arg(m_meshes.size())
        .arg(totalVertices)
        .arg(totalIndices / 3)
        .arg(minPt.x(), 0, 'f', 2).arg(minPt.y(), 0, 'f', 2).arg(minPt.z(), 0, 'f', 2)
        .arg(maxPt.x(), 0, 'f', 2).arg(maxPt.y(), 0, 'f', 2).arg(maxPt.z(), 0, 'f', 2)
        .arg(sizePt.x(), 0, 'f', 2).arg(sizePt.y(), 0, 'f', 2).arg(sizePt.z(), 0, 'f', 2)
        .arg(m_material.ambient.x(), 0, 'f', 2)
        .arg(m_material.ambient.y(), 0, 'f', 2)
        .arg(m_material.ambient.z(), 0, 'f', 2)
        .arg(m_material.diffuse.x(), 0, 'f', 2)
        .arg(m_material.diffuse.y(), 0, 'f', 2)
        .arg(m_material.diffuse.z(), 0, 'f', 2)
        .arg(m_material.specular.x(), 0, 'f', 2)
        .arg(m_material.specular.y(), 0, 'f', 2)
        .arg(m_material.specular.z(), 0, 'f', 2)
        .arg(m_material.shininess, 0, 'f', 1)
        .arg(m_material.metallic, 0, 'f', 2)
        .arg(m_material.roughness, 0, 'f', 2)
        .arg(m_light.position.x(), 0, 'f', 2)
        .arg(m_light.position.y(), 0, 'f', 2)
        .arg(m_light.position.z(), 0, 'f', 2)
        .arg(m_light.color.x(), 0, 'f', 2)
        .arg(m_light.color.y(), 0, 'f', 2)
        .arg(m_light.color.z(), 0, 'f', 2);

    return stats;
}
