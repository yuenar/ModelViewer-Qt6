# Core 模块

本模块包含 ModelViewer 的核心数据结构，定义了应用程序中使用的基础数据类型。

## 文件说明

### CpuMesh.h
定义了 CPU 端的网格数据结构。

**主要结构：**
```cpp
struct CpuMesh {
    struct Vertex {
        QVector3D position;    // 顶点位置
        QVector3D normal;      // 顶点法线
        QVector2D texCoord;    // 纹理坐标
    };
    
    QVector<Vertex> vertices;  // 顶点数组
    QVector<quint32> indices;  // 索引数组（每 3 个索引一个三角形）
    BoundingBox bbox;         // 包围盒（API 为 expand() / radius() / isEmpty()）
    QString name;             // 网格名称
    QVector3D baseColor;      // 材质基础色
    QImage diffuseTexture;    // 漫反射贴图（移动端由渲染线程上传）
    QString nodeName;         // 所属节点
    QVector<QString> nodePath;            // 根 → 节点的名称链（节点动画用）
    QVector<QMatrix4x4> nodeRestLocals;   // 与 nodePath 平行的静止局部变换
    QMatrix4x4 restWorld;                 // 加载时烘焙到顶点的静止世界变换
    QVector<SkinVertex> skinWeights;      // 每顶点最多 4 根骨骼的蒙皮权重
    QVector<QString> boneNames;
    QVector<QMatrix4x4> boneOffsets;
    QVector<QMatrix4x4> boneBindMatrices;
    QVector<QVector<QString>> bonePaths;
    QVector<QVector<QMatrix4x4>> boneRestLocals;
};
```

**功能特点：**
- 支持位置、法线、纹理坐标的顶点格式
- 使用索引缓冲区优化渲染性能
- 内置包围盒用于快速碰撞检测和视锥剔除
- 携带节点动画链与骨骼蒙皮数据，桌面/移动端共用同一套动画逻辑

### Light.h
定义场景中的光照参数。

**主要功能：**
- 光源位置、颜色、强度等属性
- 支持点光源、方向光、聚光灯等类型
- 为 PBR 渲染提供光照参数

### Material.h
定义材质属性和参数。

**主要功能：**
- 漫反射、镜面反射、自发光等基础材质属性
- PBR 材质参数（金属度、粗糙度等）
- 纹理绑定和采样参数

### Animation.h/.cpp
定义动画数据与加载结果：
- `AnimationChannel` / `AnimationClip` - 节点关键帧与动画片段（名称、时长、通道）
- `ModelAsset` - `ModelLoader::loadCPU()` 的返回类型，含 `QVector<CpuMesh> meshes` 与
  `QVector<AnimationClip> animations`；移动端异步加载直接消费该结构

### ParametricSurface.h/.cpp
程序化生成 5 种演示曲面：平面、球体、圆环、圆柱、圆锥。移动端模型列表页通过该接口创建演示模型。

## 设计原则

- **数据导向**：专注于数据结构定义，不包含业务逻辑
- **内存效率**：使用 Qt 容器类，优化内存布局
- **类型安全**：使用强类型定义，减少类型转换错误
- **可扩展性**：结构设计便于添加新的顶点属性和材质参数

## 使用说明

这些核心数据结构被其他模块广泛使用：
- `loader` 模块将加载的模型数据填充到 `CpuMesh` 结构中，并返回 `ModelAsset`
- `renderer` 模块将这些数据上传到 GPU 进行渲染
- `ui` 模块使用这些结构进行参数编辑和显示
- `mobile` 模块的 `MobileViewerBridge` 持有 `ModelAsset.meshes` / `.animations`，
  并用 `CpuMesh::nodePath` / `skinWeights` 驱动节点动画与骨骼蒙皮

## 注意事项

- 所有结构都使用 Qt 的数据类型（QVector3D、QVector2D 等）
- 顶点数据采用右手坐标系
- 法线向量假设为单位向量
