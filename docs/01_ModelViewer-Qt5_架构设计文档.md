# ModelViewer-Qt5 架构设计文档

> 仓库地址：https://github.com/sharjith/ModelViewer-Qt5  
> 技术栈：Qt5 + OpenGL 3.3 Core Profile + GLSL + Assimp / OpenCASCADE

---

## 一、项目概览

ModelViewer-Qt5 是一个基于 Qt5 的跨平台三维模型查看器，支持多种工业/图形格式的导入与实时交互渲染。核心渲染引擎直接调用 OpenGL 3.3 Core Profile，通过 `QOpenGLWidget` 将 OpenGL 上下文嵌入 Qt 窗口体系。

### 1.1 主要功能

| 功能模块 | 描述 |
|---|---|
| 模型导入 | OBJ、STL、STEP、IGES、BREP（通过 Assimp 或 OpenCASCADE） |
| 实时渲染 | Phong/Blinn-Phong 光照、线框模式、法线可视化 |
| 视角控制 | 轨迹球旋转、滚轮缩放、中键平移 |
| 材质管理 | 漫反射/高光/环境光颜色调节 |
| 投影模式 | 透视投影 / 正交投影切换 |
| 背景控制 | 渐变背景色设置 |
| 截图导出 | 导出当前视口为 PNG/JPEG |

---

## 二、目录结构

```
ModelViewer-Qt5/
├── main.cpp                    # 程序入口，创建 QApplication 与 MainWindow
├── MainWindow.h / .cpp         # 主窗口：菜单、工具栏、状态栏、槽函数
├── MainWindow.ui               # Qt Designer UI 描述文件
│
├── GLWidget.h / .cpp           # 核心：继承 QOpenGLWidget，管理 GL 上下文与渲染循环
├── GLRenderer.h / .cpp         # 渲染器：封装绘制调用、着色器切换、渲染状态
├── GLMesh.h / .cpp             # 网格数据：VAO/VBO/EBO 的创建与绑定
├── GLShaderProgram.h / .cpp    # 着色器程序封装：编译、链接、uniform 上传
│
├── Camera.h / .cpp             # 相机：视图矩阵、投影矩阵、轨迹球逻辑
├── TrackBall.h / .cpp          # 轨迹球算法：鼠标 delta → 四元数旋转
│
├── ModelLoader.h / .cpp        # 模型加载调度：根据扩展名分发到对应解析器
├── AssimpLoader.h / .cpp       # Assimp 桥接：Scene → GLMesh 列表
├── OccLoader.h / .cpp          # OpenCASCADE 桥接：TopoDS_Shape → 三角网格 → GLMesh
│
├── Material.h                  # 材质数据结构：ambient/diffuse/specular/shininess
├── Light.h                     # 光源数据结构：位置、颜色、衰减
├── BoundingBox.h / .cpp        # AABB 包围盒：用于 fit-to-view 自动适配
│
├── shaders/
│   ├── phong.vert / .frag      # Phong 光照着色器
│   ├── wireframe.vert / .frag  # 线框模式着色器
│   ├── normals.geom / .frag    # 法线可视化几何着色器
│   └── background.vert / .frag # 渐变背景全屏四边形着色器
│
├── resources.qrc               # Qt 资源文件：内嵌着色器、图标
└── ModelViewer.pro             # qmake 项目文件
```

---

## 三、核心类职责与关系

### 3.1 类关系图

```
QMainWindow
    └── MainWindow
            ├── 持有 → GLWidget          (中心视口)
            ├── 持有 → ModelLoader       (文件加载器)
            └── 连接信号槽 ↔ GLRenderer

QOpenGLWidget
    └── GLWidget
            ├── 持有 → GLRenderer        (绘制调度)
            ├── 持有 → Camera            (视图/投影矩阵)
            └── 持有 → QVector<GLMesh*>  (场景网格列表)

GLRenderer
    ├── 持有 → GLShaderProgram (phong / wireframe / normal / bg)
    └── 引用 → Camera, Light, Material

GLMesh
    └── 封装 → VAO + VBO(位置/法线/UV) + EBO(索引)

Camera
    └── 持有 → TrackBall (旋转状态四元数)

ModelLoader
    ├── 使用 → AssimpLoader  (OBJ/STL/FBX 等)
    └── 使用 → OccLoader     (STEP/IGES/BREP)
```

### 3.2 各核心类详解

#### `GLWidget`（`QOpenGLWidget` 子类）

GLWidget 是整个渲染管线的宿主，覆写三个关键虚函数：

```cpp
// 初始化 OpenGL 状态、创建着色器、初始化渲染器
void GLWidget::initializeGL();

// 响应窗口尺寸变化，更新投影矩阵与视口
void GLWidget::resizeGL(int w, int h);

// 每帧绘制：清屏 → 绘制背景 → 绘制网格
void GLWidget::paintGL();
```

鼠标/滚轮事件由 Qt 事件系统触发，转发给 Camera/TrackBall：

```cpp
void GLWidget::mousePressEvent(QMouseEvent*);
void GLWidget::mouseMoveEvent(QMouseEvent*);
void GLWidget::wheelEvent(QWheelEvent*);
```

#### `GLRenderer`

封装所有 OpenGL 绘制调用，与具体几何数据（GLMesh）解耦：

```cpp
class GLRenderer {
public:
    void initialize();                          // 初始化各着色器程序
    void render(const QVector<GLMesh*>& meshes,
                const Camera& cam,
                const Light& light);            // 主渲染入口
    void setRenderMode(RenderMode mode);        // 切换 Phong/线框/法线
    void setMaterial(const Material& mat);
private:
    GLShaderProgram* m_phongShader;
    GLShaderProgram* m_wireframeShader;
    GLShaderProgram* m_normalShader;
    GLShaderProgram* m_backgroundShader;
    void renderBackground();
    void uploadLightUniforms(GLShaderProgram*, const Light&);
    void uploadCameraUniforms(GLShaderProgram*, const Camera&);
};
```

#### `GLMesh`

持有单个网格的 GPU 资源：

```cpp
class GLMesh {
public:
    struct Vertex {
        QVector3D position;
        QVector3D normal;
        QVector2D texCoord;
    };
    GLMesh(const QVector<Vertex>& verts,
           const QVector<unsigned int>& indices);
    ~GLMesh();              // 析构时 glDeleteVertexArrays / glDeleteBuffers
    void draw() const;      // glBindVertexArray + glDrawElements
    BoundingBox boundingBox() const;
private:
    GLuint m_vao, m_vbo, m_ebo;
    int    m_indexCount;
    BoundingBox m_bbox;
};
```

#### `GLShaderProgram`

对 `QOpenGLShaderProgram` 的轻量包装，添加统一的 uniform 上传接口：

```cpp
class GLShaderProgram {
public:
    bool load(const QString& vertPath,
              const QString& fragPath,
              const QString& geomPath = "");
    void bind();
    void release();
    void setMat4(const char* name, const QMatrix4x4&);
    void setVec3(const char* name, const QVector3D&);
    void setFloat(const char* name, float);
    void setInt(const char* name, int);
private:
    QOpenGLShaderProgram* m_program;
};
```

#### `Camera`

维护视图变换状态，对外提供矩阵：

```cpp
class Camera {
public:
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;

    void setAspectRatio(float aspect);
    void setProjectionType(ProjectionType type);  // Perspective / Ortho

    // 交互接口（由 GLWidget 调用）
    void rotate(const QPoint& from, const QPoint& to, const QSize& viewport);
    void pan(const QPoint& delta);
    void zoom(float delta);
    void fitToView(const BoundingBox& bbox);

private:
    TrackBall    m_trackball;      // 旋转四元数
    QVector3D    m_pivot;          // 旋转中心
    QVector3D    m_eye;            // 相机位置
    float        m_fov;
    float        m_near, m_far;
    float        m_aspectRatio;
    ProjectionType m_projType;
};
```

#### `TrackBall`

将二维屏幕坐标映射到单位球面，差分计算旋转四元数：

```cpp
class TrackBall {
public:
    void press(const QPointF& point);
    void move(const QPointF& point);
    QQuaternion rotation() const;
private:
    QVector3D mapToSphere(const QPointF& p) const;
    QQuaternion m_rotation;
    QPointF     m_lastPos;
    bool        m_pressed;
};
```

#### `ModelLoader`

根据文件扩展名分发加载任务：

```cpp
class ModelLoader {
public:
    QVector<GLMesh*> load(const QString& filePath);
private:
    QVector<GLMesh*> loadWithAssimp(const QString& path);
    QVector<GLMesh*> loadWithOCC(const QString& path);
    static bool isOccFormat(const QString& ext);  // step/iges/brep
};
```

---

## 四、渲染管线流程

### 4.1 每帧绘制序列

```
GLWidget::paintGL()
    │
    ├─① glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    │
    ├─② GLRenderer::renderBackground()
    │       关闭深度写入，绑定 backgroundShader
    │       绘制全屏四边形（渐变色通过 uniform 传入）
    │       恢复深度写入
    │
    └─③ GLRenderer::render(meshes, camera, light)
            ├─ 根据 RenderMode 绑定对应 ShaderProgram
            ├─ 上传 MVP 矩阵（model / view / projection）
            ├─ 上传光照 uniform（lightPos、lightColor、...）
            ├─ 上传材质 uniform（ambient、diffuse、specular、shininess）
            └─ 遍历 meshes → GLMesh::draw()
                    glBindVertexArray(m_vao)
                    glDrawElements(GL_TRIANGLES, m_indexCount, ...)
```

### 4.2 着色器数据流

```
顶点数据（VBO）
    position  (location=0, vec3)
    normal    (location=1, vec3)
    texCoord  (location=2, vec2)
        │
        ▼
[phong.vert]
    ─ 计算 fragPos（世界空间）
    ─ 计算 transformedNormal（法线矩阵变换）
    ─ 传出 fragPos, normal 到片元着色器
        │
        ▼
[phong.frag]
    ─ 计算 ambient = Ka * lightColor
    ─ 计算 diffuse = Kd * max(dot(N,L), 0)
    ─ 计算 specular = Ks * pow(max(dot(R,V),0), shininess)
    ─ fragColor = (ambient + diffuse + specular) * objectColor
```

### 4.3 模型加载流程

```
用户选择文件
    │
    ▼
ModelLoader::load(filePath)
    │
    ├── 扩展名 ∈ {obj, stl, fbx, dae, ...}
    │       └── AssimpLoader::load()
    │               Assimp::Importer::ReadFile()
    │               aiScene → 遍历 aiMesh
    │               每个 aiMesh → GLMesh(vertices, indices)
    │
    └── 扩展名 ∈ {step, iges, brep}
            └── OccLoader::load()
                    BRep_Builder / IGESControl_Reader / STEPControl_Reader
                    TopoDS_Shape → BRepMesh_IncrementalMesh（三角化）
                    提取三角面片顶点+法线 → GLMesh(vertices, indices)
    │
    ▼
QVector<GLMesh*> 存入 GLWidget::m_meshes
    │
    ▼
Camera::fitToView(合并 BoundingBox)
    │
    ▼
GLWidget::update() → 触发重绘
```

---

## 五、坐标系与变换约定

| 矩阵 | 含义 | 更新时机 |
|---|---|---|
| Model Matrix | 初始为单位矩阵；轨迹球旋转更新旋转分量 | 鼠标拖拽 |
| View Matrix | 由 Camera::viewMatrix() 提供（lookAt） | 平移/缩放 |
| Projection Matrix | 由 Camera::projectionMatrix() 提供 | resize / 切换投影类型 |
| Normal Matrix | `transpose(inverse(MV))` 的 3×3 部分 | 每帧重算 |

- 世界坐标系：右手坐标系，Y 轴向上
- NDC 裁剪范围：[-1, 1]³（OpenGL 标准）
- 深度测试：`GL_LESS`，深度缓冲 24bit

---

## 六、信号槽连接关系

```
MainWindow
    ├── actionOpen    triggered → onOpenFile()
    │                               ModelLoader::load() → GLWidget::setMeshes()
    ├── actionWireframe toggled  → GLWidget::setRenderMode(Wireframe)
    ├── actionNormals   toggled  → GLWidget::setRenderMode(Normals)
    ├── actionFitView   triggered → GLWidget::fitToView()
    ├── actionOrtho     toggled  → GLWidget::setProjectionType(Ortho)
    ├── materialDlg     accepted → GLWidget::setMaterial(material)
    ├── lightDlg        accepted → GLWidget::setLight(light)
    └── actionSnapshot  triggered → GLWidget::grabFramebuffer().save(path)
```

---

## 七、内存与资源管理

- `GLMesh` 对象在 `GLWidget` 的成员 `QVector<GLMesh*> m_meshes` 中集中管理；加载新文件前调用 `qDeleteAll(m_meshes)` 释放旧数据
- GPU 资源（VAO/VBO/EBO）在 `GLMesh` 析构函数中通过 `QOpenGLFunctions::glDeleteBuffers` 释放；必须在 GL 上下文有效时调用（在 `GLWidget::makeCurrent()` 保护下）
- 着色器程序由 `GLRenderer` 独占，在 `GLRenderer` 析构时随 `QOpenGLShaderProgram` 自动释放
- 模型加载在主线程同步执行（大文件时 UI 会短暂阻塞，未采用异步方案）

---

## 八、编译依赖

| 依赖库 | 版本要求 | 用途 |
|---|---|---|
| Qt5 | ≥ 5.12 | 窗口系统、OpenGL 封装、事件循环 |
| OpenGL | 3.3 Core | 底层图形 API |
| Assimp | ≥ 4.0 | OBJ/STL/FBX/DAE 等格式解析 |
| OpenCASCADE | ≥ 7.0 (可选) | STEP/IGES/BREP 解析与三角化 |
| GLM (可选) | — | 数学辅助（部分版本使用 Qt 自带数学类替代） |

构建系统使用 `qmake`，项目文件 `ModelViewer.pro` 通过 `LIBS` 和 `INCLUDEPATH` 指定第三方依赖路径。

---

## 九、已知局限性

1. **单线程加载**：大型 STEP 模型三角化耗时长，主线程阻塞
2. **无 LOD 机制**：高面数模型无降精度策略
3. **固定管线模式受限**：部分效果（PBR、阴影）未实现
4. **无场景图**：所有网格平铺在同一个列表，不支持层级结构
5. **OpenGL 裸调用**：未使用 Qt RHI 或其他抽象层，跨平台行为依赖驱动实现
6. **macOS Metal 兼容问题**：Qt5 在 macOS 12+ 上 OpenGL 已标记为 Deprecated
