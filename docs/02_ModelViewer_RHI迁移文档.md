# ModelViewer Qt6.9 RHI 架构迁移文档

> 原始技术栈：Qt5 + `QOpenGLWidget` + 裸 OpenGL 3.3 API  
> 目标技术栈：Qt6.9 + `QRhiWidget` + Qt RHI（后端可选 OpenGL / Vulkan / Metal / D3D12）  
> 迁移原则：**KISS**——最小改动范围，保留原有业务逻辑，仅替换渲染层

---

## 一、RHI 核心概念速查

理解以下对应关系是迁移的基础：

| OpenGL 概念 | Qt RHI 对应类型 | 说明 |
|---|---|---|
| GL Context | `QRhi` | RHI 实例，对应一个图形设备 |
| Framebuffer / Swapchain | `QRhiRenderTarget` / `QRhiSwapChain` | QRhiWidget 内部管理，不需手动创建 |
| VAO + VBO | `QRhiBuffer` (VertexBuffer) | 上传顶点数据 |
| EBO (索引缓冲) | `QRhiBuffer` (IndexBuffer) | 上传索引数据 |
| Uniform Buffer Object | `QRhiBuffer` (UniformBuffer) | 替代所有 glUniform* 调用 |
| Texture | `QRhiTexture` | （本项目暂无贴图，可跳过） |
| Sampler | `QRhiSampler` | — |
| Vertex Attribute 布局 | `QRhiVertexInputLayout` | 描述 location/offset/stride |
| 着色器程序 | `QRhiShaderStage` + `.qsb` | GLSL → `qsb` 工具编译为跨后端字节码 |
| Render State (depth/blend) | `QRhiGraphicsPipeline` | PSO，不可更改，需预先创建 |
| Draw Call | `QRhiCommandBuffer` | 录制命令，统一提交 |
| glUniform / glBindTexture | `QRhiShaderResourceBindings` | 绑定 UBO、纹理到着色器 |

---

## 二、迁移整体路线图

```
阶段 1：项目工程迁移
    Qt5 → Qt6.9 模块替换，构建系统从 qmake 迁至 CMake

阶段 2：渲染宿主替换
    QOpenGLWidget → QRhiWidget
    覆写 initialize() / render() 替代 initializeGL() / paintGL()

阶段 3：GPU 资源重建
    GLMesh (VAO/VBO/EBO) → RhiMesh (QRhiBuffer × 3)
    GLShaderProgram       → QSB 着色器文件 + QRhiShaderStage

阶段 4：渲染状态迁移
    裸 glEnable/glDepthFunc → QRhiGraphicsPipeline（PSO）

阶段 5：Uniform 数据迁移
    glUniform*            → QRhiBuffer (UniformBuffer) + QRhiShaderResourceBindings

阶段 6：着色器迁移
    .vert / .frag (GLSL)  → .vert.qsb / .frag.qsb（通过 qsb 工具编译）
```

---

## 三、阶段 1：工程迁移

### 3.1 CMakeLists.txt 替代 .pro 文件

```cmake
cmake_minimum_required(VERSION 3.20)
project(ModelViewer)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Widgets ShaderTools)
find_package(assimp REQUIRED)

qt_standard_project_setup()

qt_add_executable(ModelViewer
    main.cpp
    MainWindow.h MainWindow.cpp
    RhiWidget.h  RhiWidget.cpp       # 原 GLWidget 的替代品
    RhiRenderer.h RhiRenderer.cpp    # 原 GLRenderer 的替代品
    RhiMesh.h    RhiMesh.cpp         # 原 GLMesh 的替代品
    Camera.h     Camera.cpp          # 无需修改
    TrackBall.h  TrackBall.cpp       # 无需修改
    ModelLoader.h ModelLoader.cpp    # 无需修改
    BoundingBox.h BoundingBox.cpp    # 无需修改
    Material.h   Light.h             # 无需修改
)

# 编译 QSB 着色器
qt_add_shaders(ModelViewer "shaders"
    GLSL "460"
    HLSL "50"
    MSL  "12"
    FILES
        shaders/phong.vert
        shaders/phong.frag
        shaders/wireframe.vert
        shaders/wireframe.frag
        shaders/background.vert
        shaders/background.frag
    OUTPUT_TARGETS shader_targets
)

target_link_libraries(ModelViewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    assimp::assimp
)

# 内嵌 QSB 到 Qt 资源系统
qt_add_resources(ModelViewer "resources"
    PREFIX "/"
    FILES
        shaders/phong.vert.qsb
        shaders/phong.frag.qsb
        shaders/wireframe.vert.qsb
        shaders/wireframe.frag.qsb
        shaders/background.vert.qsb
        shaders/background.frag.qsb
)
```

### 3.2 模块依赖变更

```cpp
// Qt5 头文件                         → Qt6 替代
#include <QOpenGLWidget>               → #include <QRhiWidget>
#include <QOpenGLFunctions_3_3_Core>   → 删除，不再需要
#include <QOpenGLShaderProgram>        → 删除，改用 QShader / QRhiShaderStage
#include <QOpenGLBuffer>               → 删除，改用 QRhiBuffer
#include <QOpenGLVertexArrayObject>    → 删除，改用 QRhiVertexInputLayout
```

Qt6 中 `QMatrix4x4`、`QVector3D`、`QVector4D`、`QQuaternion` 均保持不变，Camera / TrackBall / BoundingBox **无需修改**。

---

## 四、阶段 2：渲染宿主替换（`QRhiWidget`）

### 4.1 新建 `RhiWidget`

`QRhiWidget`（Qt 6.7+ 正式稳定）是 `QOpenGLWidget` 在 RHI 层的直接对应物。

```cpp
// RhiWidget.h
#pragma once
#include <QRhiWidget>
#include <QPoint>
#include "Camera.h"
#include "Material.h"
#include "Light.h"
#include "RhiMesh.h"

class RhiRenderer;

class RhiWidget : public QRhiWidget {
    Q_OBJECT
public:
    explicit RhiWidget(QWidget* parent = nullptr);
    ~RhiWidget() override;

    void loadModel(const QString& filePath);
    void setRenderMode(int mode);
    void setMaterial(const Material& mat);
    void setLight(const Light& light);
    void fitToView();

protected:
    // ── 替代 initializeGL() ──────────────────────────────
    void initialize(QRhiCommandBuffer* cb) override;

    // ── 替代 paintGL() ───────────────────────────────────
    void render(QRhiCommandBuffer* cb) override;

    // ── 替代 resizeGL() ──────────────────────────────────
    // QRhiWidget 内部自动处理 resize，projection 在 render() 中按 renderTarget()->pixelSize() 更新

    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    RhiRenderer*         m_renderer  = nullptr;
    Camera               m_camera;
    Material             m_material;
    Light                m_light;
    QVector<RhiMesh*>    m_meshes;
    QPoint               m_lastMousePos;
    Qt::MouseButton      m_activeButton = Qt::NoButton;
};
```

```cpp
// RhiWidget.cpp
#include "RhiWidget.h"
#include "RhiRenderer.h"
#include "ModelLoader.h"
#include "BoundingBox.h"
#include <QMouseEvent>
#include <QWheelEvent>

RhiWidget::RhiWidget(QWidget* parent)
    : QRhiWidget(parent)
{
    // 指定 RHI 后端（可由用户通过命令行参数覆盖）
    // 不指定则由 Qt 自动选择平台最优后端
    // setApi(QRhiWidget::Api::Vulkan);
}

RhiWidget::~RhiWidget() {
    // RhiRenderer 在 initialize() 中创建，析构时销毁
    // 注意：QRhiWidget 保证在销毁前 QRhi 上下文仍然有效
    delete m_renderer;
    qDeleteAll(m_meshes);
}

void RhiWidget::initialize(QRhiCommandBuffer* cb) {
    // rhi() 返回当前有效的 QRhi 实例（由 QRhiWidget 管理）
    if (!m_renderer) {
        m_renderer = new RhiRenderer(rhi(), renderTarget());
        m_renderer->initialize(cb);
    } else {
        // resize 或上下文重建时重新同步 RenderTarget
        m_renderer->setRenderTarget(renderTarget());
        m_renderer->releaseAndRebuildPipelines();
    }
}

void RhiWidget::render(QRhiCommandBuffer* cb) {
    const QSize pixelSize = renderTarget()->pixelSize();
    m_camera.setAspectRatio(float(pixelSize.width()) / float(pixelSize.height()));

    m_renderer->render(cb, m_meshes, m_camera, m_light, m_material);
}

void RhiWidget::loadModel(const QString& filePath) {
    qDeleteAll(m_meshes);
    m_meshes.clear();

    ModelLoader loader;
    // ModelLoader 返回 CPU 端数据（顶点/索引），RhiMesh 负责上传 GPU
    const auto cpuMeshes = loader.loadCPU(filePath);

    BoundingBox sceneBbox;
    for (auto& cpuMesh : cpuMeshes) {
        // 上传到 GPU 需要 QRhi，因此需要在 RhiWidget 拥有 rhi() 后执行
        // 简单方案：在主线程 makeCurrent 后上传（defer to next initialize/render）
        auto* mesh = new RhiMesh(rhi(), cpuMesh);
        mesh->upload();
        m_meshes.append(mesh);
        sceneBbox.expand(cpuMesh.bbox);
    }
    m_camera.fitToView(sceneBbox);
    update();   // 触发下一帧渲染
}

// ── 鼠标事件（逻辑与原 GLWidget 完全相同）───────────────
void RhiWidget::mousePressEvent(QMouseEvent* e) {
    m_lastMousePos  = e->pos();
    m_activeButton  = e->button();
    if (e->button() == Qt::LeftButton)
        m_camera.beginRotate(e->pos());
}

void RhiWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_activeButton == Qt::LeftButton) {
        m_camera.rotate(m_lastMousePos, e->pos(), size());
    } else if (m_activeButton == Qt::MiddleButton) {
        const QPoint delta = e->pos() - m_lastMousePos;
        m_camera.pan(delta);
    }
    m_lastMousePos = e->pos();
    update();
}

void RhiWidget::mouseReleaseEvent(QMouseEvent*) { m_activeButton = Qt::NoButton; }

void RhiWidget::wheelEvent(QWheelEvent* e) {
    m_camera.zoom(e->angleDelta().y() / 120.0f);
    update();
}
```

---

## 五、阶段 3：GPU 资源重建（`RhiMesh`）

`RhiMesh` 替代原来的 `GLMesh`，使用 `QRhiBuffer` 替代 VAO/VBO/EBO。

```cpp
// RhiMesh.h
#pragma once
#include <rhi/qrhi.h>
#include <QVector3D>
#include <QVector2D>

struct CpuMesh {
    struct Vertex {
        QVector3D position;
        QVector3D normal;
        QVector2D texCoord;
    };
    QVector<Vertex>       vertices;
    QVector<quint32>      indices;
    BoundingBox           bbox;
};

class RhiMesh {
public:
    RhiMesh(QRhi* rhi, const CpuMesh& cpu);
    ~RhiMesh();

    void upload();  // 将 CPU 数据上传到 GPU buffer

    QRhiBuffer* vertexBuffer() const { return m_vbuf; }
    QRhiBuffer* indexBuffer()  const { return m_ibuf; }
    int         indexCount()   const { return m_indexCount; }

private:
    QRhi*       m_rhi       = nullptr;
    QRhiBuffer* m_vbuf      = nullptr;   // 顶点缓冲
    QRhiBuffer* m_ibuf      = nullptr;   // 索引缓冲
    CpuMesh     m_cpuData;
    int         m_indexCount = 0;
};
```

```cpp
// RhiMesh.cpp
#include "RhiMesh.h"

RhiMesh::RhiMesh(QRhi* rhi, const CpuMesh& cpu)
    : m_rhi(rhi), m_cpuData(cpu), m_indexCount(cpu.indices.size())
{}

RhiMesh::~RhiMesh() {
    delete m_vbuf;
    delete m_ibuf;
}

void RhiMesh::upload() {
    const quint32 vbufSize = m_cpuData.vertices.size() * sizeof(CpuMesh::Vertex);
    const quint32 ibufSize = m_cpuData.indices.size()  * sizeof(quint32);

    // 创建不可变静态缓冲（Immutable = 上传一次不再修改，驱动可放入显存）
    m_vbuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                               QRhiBuffer::VertexBuffer, vbufSize);
    m_vbuf->create();

    m_ibuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                               QRhiBuffer::IndexBuffer,  ibufSize);
    m_ibuf->create();

    // 通过 QRhiResourceUpdateBatch 异步上传数据
    QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(m_vbuf,
        m_cpuData.vertices.constData());
    batch->uploadStaticBuffer(m_ibuf,
        m_cpuData.indices.constData());

    // 注意：batch 需要在 QRhiCommandBuffer::beginPass 时提交
    // 实际项目中将 batch 返回给调用者，或在首次 render 时提交
    // 这里简化：保存到成员，由 RhiRenderer 在首帧 beginPass 前提交
    m_pendingBatch = batch;  // 需要在头文件中添加 QRhiResourceUpdateBatch* m_pendingBatch
}
```

---

## 六、阶段 4 & 5：渲染状态与 Uniform 迁移（`RhiRenderer`）

这是迁移工作量最集中的部分。RHI 要求把所有渲染状态（深度测试、混合、顶点布局）**提前**固化在 `QRhiGraphicsPipeline` 中。

### 6.1 Uniform Buffer 布局设计

将原来零散的 `glUniform*` 调用整合为两个 UBO：

```glsl
// 每帧更新（per-frame UBO，binding=0）
layout(std140, binding=0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;   // transpose(inverse(model*view)) 的 3x3 打包为 mat4
    vec3 lightPos;       // 世界空间
    float _pad0;
    vec3 lightColor;
    float _pad1;
    vec3 viewPos;        // 相机位置
    float _pad2;
};

// 材质（可按需更新，binding=1）
layout(std140, binding=1) uniform MaterialUBO {
    vec3  ambient;
    float shininess;
    vec3  diffuse;
    float _pad0;
    vec3  specular;
    float _pad1;
};
```

对应的 C++ 结构体（注意 std140 对齐）：

```cpp
// RhiRenderer.h 中定义
struct alignas(16) FrameUBOData {
    float model[16];
    float view[16];
    float projection[16];
    float normalMatrix[16];
    float lightPos[3];   float _pad0;
    float lightColor[3]; float _pad1;
    float viewPos[3];    float _pad2;
};

struct alignas(16) MaterialUBOData {
    float ambient[3];    float shininess;
    float diffuse[3];    float _pad0;
    float specular[3];   float _pad1;
};
```

### 6.2 `RhiRenderer` 完整实现

```cpp
// RhiRenderer.h
#pragma once
#include <rhi/qrhi.h>
#include "Camera.h"
#include "Material.h"
#include "Light.h"
#include "RhiMesh.h"

class RhiRenderer {
public:
    RhiRenderer(QRhi* rhi, QRhiRenderTarget* rt);
    ~RhiRenderer();

    void initialize(QRhiCommandBuffer* cb);
    void setRenderTarget(QRhiRenderTarget* rt);
    void releaseAndRebuildPipelines();
    void setRenderMode(int mode);   // 0=Phong, 1=Wireframe

    void render(QRhiCommandBuffer* cb,
                const QVector<RhiMesh*>& meshes,
                const Camera& cam,
                const Light& light,
                const Material& mat);

private:
    void buildPipeline();
    void buildBackgroundPipeline();
    QShader loadShader(const QString& qsbPath);

    QRhi*               m_rhi;
    QRhiRenderTarget*   m_rt;

    // ── Phong 管线 ──────────────────────────────────────
    QRhiGraphicsPipeline*       m_pipeline        = nullptr;
    QRhiShaderResourceBindings* m_srb             = nullptr;
    QRhiBuffer*                 m_frameUBO        = nullptr;
    QRhiBuffer*                 m_materialUBO     = nullptr;

    // ── Background 管线 ─────────────────────────────────
    QRhiGraphicsPipeline*       m_bgPipeline      = nullptr;
    QRhiShaderResourceBindings* m_bgSrb           = nullptr;
    QRhiBuffer*                 m_bgUBO           = nullptr;
    QRhiBuffer*                 m_bgVBuf          = nullptr;  // 全屏四边形

    int m_renderMode = 0;
};
```

```cpp
// RhiRenderer.cpp
#include "RhiRenderer.h"
#include <QFile>

RhiRenderer::RhiRenderer(QRhi* rhi, QRhiRenderTarget* rt)
    : m_rhi(rhi), m_rt(rt) {}

RhiRenderer::~RhiRenderer() {
    delete m_pipeline;      delete m_srb;
    delete m_frameUBO;      delete m_materialUBO;
    delete m_bgPipeline;    delete m_bgSrb;
    delete m_bgUBO;         delete m_bgVBuf;
}

QShader RhiRenderer::loadShader(const QString& qsbPath) {
    QFile f(qsbPath);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Cannot open shader: %s", qPrintable(qsbPath));
    return QShader::fromSerialized(f.readAll());
}

void RhiRenderer::initialize(QRhiCommandBuffer*) {
    // ── 创建 UBO ────────────────────────────────────────
    m_frameUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                   QRhiBuffer::UniformBuffer,
                                   sizeof(FrameUBOData));
    m_frameUBO->create();

    m_materialUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                      QRhiBuffer::UniformBuffer,
                                      sizeof(MaterialUBOData));
    m_materialUBO->create();

    buildPipeline();
    buildBackgroundPipeline();
}

void RhiRenderer::buildPipeline() {
    // ── 顶点输入布局（对应 CpuMesh::Vertex 的内存布局）────
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(CpuMesh::Vertex) }                    // stride = 32 bytes
    });
    inputLayout.setAttributes({
        // location=0: position (vec3, offset=0)
        { 0, 0, QRhiVertexInputAttribute::Float3, 0  },
        // location=1: normal   (vec3, offset=12)
        { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
        // location=2: texCoord (vec2, offset=24)
        { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
    });

    // ── Shader Resource Bindings（UBO 绑定表）────────────
    m_srb = m_rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0,  // binding=0
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO),
        QRhiShaderResourceBinding::uniformBuffer(
            1,  // binding=1
            QRhiShaderResourceBinding::FragmentStage,
            m_materialUBO),
    });
    m_srb->create();

    // ── 图形管线（PSO）──────────────────────────────────
    m_pipeline = m_rhi->newGraphicsPipeline();
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex,
          loadShader(":/shaders/phong.vert.qsb") },
        { QRhiShaderStage::Fragment,
          loadShader(":/shaders/phong.frag.qsb") },
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());

    // ── 渲染状态（原来 glEnable/glDepthFunc 的等价物）─────
    m_pipeline->setDepthTest(true);
    m_pipeline->setDepthWrite(true);
    m_pipeline->setDepthOp(QRhiGraphicsPipeline::Less);
    m_pipeline->setCullMode(QRhiGraphicsPipeline::Back);
    m_pipeline->setFrontFace(QRhiGraphicsPipeline::CCW);
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_pipeline->create();
}

void RhiRenderer::buildBackgroundPipeline() {
    // 全屏四边形顶点（NDC，两个三角形组成矩形）
    static const float bgVerts[] = {
        -1, -1,   1, -1,   -1,  1,
        -1,  1,   1, -1,    1,  1,
    };
    m_bgVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                 QRhiBuffer::VertexBuffer,
                                 sizeof(bgVerts));
    m_bgVBuf->create();

    // 通过 resourceUpdateBatch 上传（在首帧 render 时提交）
    // ... 省略上传代码，与 RhiMesh::upload() 模式相同

    // Background UBO：存放两个颜色（顶部色、底部色）
    m_bgUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                QRhiBuffer::UniformBuffer, 32);
    m_bgUBO->create();

    m_bgSrb = m_rhi->newShaderResourceBindings();
    m_bgSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::FragmentStage, m_bgUBO),
    });
    m_bgSrb->create();

    m_bgPipeline = m_rhi->newGraphicsPipeline();
    m_bgPipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   loadShader(":/shaders/background.vert.qsb") },
        { QRhiShaderStage::Fragment, loadShader(":/shaders/background.frag.qsb") },
    });

    QRhiVertexInputLayout bgLayout;
    bgLayout.setBindings({ { 2 * sizeof(float) } });
    bgLayout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float2, 0 } });
    m_bgPipeline->setVertexInputLayout(bgLayout);
    m_bgPipeline->setShaderResourceBindings(m_bgSrb);
    m_bgPipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    m_bgPipeline->setDepthTest(false);   // 背景不参与深度测试
    m_bgPipeline->setDepthWrite(false);
    m_bgPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_bgPipeline->create();
}

void RhiRenderer::render(QRhiCommandBuffer* cb,
                          const QVector<RhiMesh*>& meshes,
                          const Camera& cam,
                          const Light& light,
                          const Material& mat)
{
    // ── 1. 构造本帧 UBO 数据 ────────────────────────────
    FrameUBOData frameData;
    const QMatrix4x4 view  = cam.viewMatrix();
    const QMatrix4x4 proj  = m_rhi->clipSpaceCorrMatrix()  // ← 关键：处理 NDC 差异
                             * cam.projectionMatrix();
    const QMatrix4x4 model;                                 // 单位矩阵（旋转已在 view 中）
    const QMatrix4x4 normalMat = (view * model).inverted().transposed();

    memcpy(frameData.model,        model.constData(),     64);
    memcpy(frameData.view,         view.constData(),      64);
    memcpy(frameData.projection,   proj.constData(),      64);
    memcpy(frameData.normalMatrix, normalMat.constData(), 64);
    const QVector3D lp = light.position;
    frameData.lightPos[0]   = lp.x(); frameData.lightPos[1]   = lp.y(); frameData.lightPos[2]   = lp.z();
    const QVector3D lc = light.color;
    frameData.lightColor[0] = lc.x(); frameData.lightColor[1] = lc.y(); frameData.lightColor[2] = lc.z();
    const QVector3D vp = cam.position();
    frameData.viewPos[0]    = vp.x(); frameData.viewPos[1]    = vp.y(); frameData.viewPos[2]    = vp.z();

    MaterialUBOData matData;
    matData.shininess    = mat.shininess;
    memcpy(matData.ambient,  &mat.ambient,  12);
    memcpy(matData.diffuse,  &mat.diffuse,  12);
    memcpy(matData.specular, &mat.specular, 12);

    // ── 2. 提交 UBO 数据更新 ────────────────────────────
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    updates->updateDynamicBuffer(m_frameUBO,    0, sizeof(FrameUBOData),    &frameData);
    updates->updateDynamicBuffer(m_materialUBO, 0, sizeof(MaterialUBOData), &matData);

    // ── 3. 开始 RenderPass ──────────────────────────────
    const QColor clearColor(30, 30, 30);     // 背景基色（被背景 pass 覆写）
    const QRhiDepthStencilClearValue dsv(1.0f, 0);

    cb->beginPass(m_rt, clearColor, dsv, updates);

    const QSize sz = m_rt->pixelSize();
    cb->setViewport({ 0, 0, float(sz.width()), float(sz.height()) });

    // ── 4. 绘制背景四边形 ───────────────────────────────
    cb->setGraphicsPipeline(m_bgPipeline);
    cb->setShaderResources(m_bgSrb);
    const QRhiCommandBuffer::VertexInput bgVBufBind(m_bgVBuf, 0);
    cb->setVertexInput(0, 1, &bgVBufBind);
    cb->draw(6);    // 6 个顶点 = 2 个三角形

    // ── 5. 绘制模型网格 ─────────────────────────────────
    cb->setGraphicsPipeline(m_pipeline);
    cb->setShaderResources(m_srb);

    for (RhiMesh* mesh : meshes) {
        const QRhiCommandBuffer::VertexInput vbufBind(mesh->vertexBuffer(), 0);
        cb->setVertexInput(0, 1, &vbufBind,
                           mesh->indexBuffer(),
                           0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(mesh->indexCount());
    }

    cb->endPass();
}

void RhiRenderer::setRenderTarget(QRhiRenderTarget* rt) { m_rt = rt; }

void RhiRenderer::releaseAndRebuildPipelines() {
    delete m_pipeline;  m_pipeline  = nullptr;
    delete m_srb;       m_srb       = nullptr;
    delete m_bgPipeline; m_bgPipeline = nullptr;
    delete m_bgSrb;     m_bgSrb     = nullptr;
    buildPipeline();
    buildBackgroundPipeline();
}
```

---

## 七、阶段 6：着色器迁移

### 7.1 Phong 着色器

着色器 GLSL 语法与原版几乎相同，主要改动是：**去掉 `glUniform` 对应的 `uniform` 变量，改用 UBO 块**。

```glsl
// shaders/phong.vert  （版本升至 460，使用 UBO）
#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

out vec3 fragPos;
out vec3 fragNormal;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    fragPos       = worldPos.xyz;
    fragNormal    = mat3(normalMatrix) * inNormal;
    gl_Position   = projection * view * worldPos;
}
```

```glsl
// shaders/phong.frag
#version 460

in  vec3 fragPos;
in  vec3 fragNormal;
out vec4 outColor;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model; mat4 view; mat4 projection; mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

layout(std140, binding = 1) uniform MaterialUBO {
    vec3  ambient;  float shininess;
    vec3  diffuse;  float _p0;
    vec3  specular; float _p1;
};

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(viewPos  - fragPos);
    vec3 R = reflect(-L, N);

    vec3 a = ambient  * lightColor;
    vec3 d = diffuse  * lightColor * max(dot(N, L), 0.0);
    vec3 s = specular * lightColor * pow(max(dot(R, V), 0.0), shininess);

    outColor = vec4(a + d + s, 1.0);
}
```

```glsl
// shaders/background.vert
#version 460
layout(location = 0) in vec2 inPos;
out float vY;   // 用于片元插值计算渐变
void main() {
    vY = inPos.y;
    gl_Position = vec4(inPos, 0.0, 1.0);
}
```

```glsl
// shaders/background.frag
#version 460
in  float vY;
out vec4  outColor;
layout(std140, binding = 0) uniform BgUBO {
    vec4 topColor;
    vec4 bottomColor;
};
void main() {
    float t   = (vY + 1.0) * 0.5;        // [-1,1] → [0,1]
    outColor  = mix(bottomColor, topColor, t);
}
```

### 7.2 QSB 编译命令

CMake 中 `qt_add_shaders()` 会自动完成下列等价操作：

```bash
# 手动编译示例（仅供理解，CMake 自动处理）
qsb --glsl "460" --hlsl 50 --msl 12 \
    -o shaders/phong.vert.qsb shaders/phong.vert

qsb --glsl "460" --hlsl 50 --msl 12 \
    -o shaders/phong.frag.qsb shaders/phong.frag
```

`qsb` 工具位于 Qt 安装目录的 `bin/` 下，会将一份 GLSL 源码编译为包含 GLSL/HLSL/MSL 多后端字节码的 `.qsb` 文件，运行时由 RHI 自动选择匹配当前后端的版本。

---

## 八、`clipSpaceCorrMatrix()` — 最容易忽略的关键点

不同图形 API 的 NDC 裁剪空间存在差异：

| API | Y 轴方向 | 深度范围 |
|---|---|---|
| OpenGL | Y 向上 | [-1, 1] |
| Vulkan | Y 向下 | [0, 1] |
| Metal | Y 向上 | [0, 1] |
| D3D12 | Y 向上 | [0, 1] |

`m_rhi->clipSpaceCorrMatrix()` 返回一个修正矩阵，**左乘到投影矩阵上**即可消除上述差异，无需在着色器中做任何平台判断：

```cpp
// render() 中正确写法：
const QMatrix4x4 proj = m_rhi->clipSpaceCorrMatrix() * cam.projectionMatrix();
```

---

## 九、`ModelLoader` 的最小改动

原 `ModelLoader` 返回 `QVector<GLMesh*>`（含 GPU 资源），迁移后需要把加载与 GPU 上传分离：

```cpp
// 新接口：只返回 CPU 数据，不触碰 OpenGL
class ModelLoader {
public:
    QVector<CpuMesh> loadCPU(const QString& filePath);
private:
    QVector<CpuMesh> loadWithAssimp(const QString& path);
    QVector<CpuMesh> loadWithOCC(const QString& path);
};
```

`AssimpLoader` 内部从 `aiMesh` 填充 `CpuMesh::Vertex` 和索引数组，不再调用任何 OpenGL 函数。GPU 上传由 `RhiMesh::upload()` 负责。这是 **唯一需要修改** `ModelLoader` 的地方。

---

## 十、`MainWindow` 改动

只需将成员变量类型从 `GLWidget*` 改为 `RhiWidget*`，其余信号槽调用接口保持不变：

```cpp
// MainWindow.h — 修改前
GLWidget* m_glWidget;

// MainWindow.h — 修改后
RhiWidget* m_rhiWidget;
```

槽函数示例：
```cpp
// 修改前
connect(ui->actionOpen, &QAction::triggered, this, [this]{
    const QString path = QFileDialog::getOpenFileName(...);
    m_glWidget->loadModel(path);     // GLWidget 接口
});

// 修改后（接口名称完全相同，仅类型变化）
connect(ui->actionOpen, &QAction::triggered, this, [this]{
    const QString path = QFileDialog::getOpenFileName(...);
    m_rhiWidget->loadModel(path);    // RhiWidget 接口
});
```

---

## 十一、文件变更总表

| 文件 | 状态 | 说明 |
|---|---|---|
| `GLWidget.h / .cpp` | **删除** | 由 `RhiWidget` 替代 |
| `GLRenderer.h / .cpp` | **删除** | 由 `RhiRenderer` 替代 |
| `GLMesh.h / .cpp` | **删除** | 由 `RhiMesh` 替代 |
| `GLShaderProgram.h / .cpp` | **删除** | 由 QSB + QRhiShaderStage 替代 |
| `RhiWidget.h / .cpp` | **新增** | — |
| `RhiRenderer.h / .cpp` | **新增** | — |
| `RhiMesh.h / .cpp` | **新增** | — |
| `shaders/*.vert/.frag` | **修改** | 升级 GLSL 版本，改用 UBO |
| `shaders/*.qsb` | **新增** | 由 CMake `qt_add_shaders` 自动生成 |
| `ModelLoader.h / .cpp` | **小改** | 返回 `CpuMesh` 代替 `GLMesh*` |
| `Camera.h / .cpp` | **不变** | — |
| `TrackBall.h / .cpp` | **不变** | — |
| `BoundingBox.h / .cpp` | **不变** | — |
| `Material.h / Light.h` | **不变** | — |
| `MainWindow.h / .cpp` | **极小改** | 成员类型 `GLWidget*` → `RhiWidget*` |
| `ModelViewer.pro` | **删除** | 由 `CMakeLists.txt` 替代 |
| `CMakeLists.txt` | **新增** | — |
