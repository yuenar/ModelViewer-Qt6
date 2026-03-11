# Loader 模块

本模块负责加载各种 3D 模型文件格式，将其转换为应用程序内部的 `CpuMesh` 数据结构。

## 文件说明

### ModelLoader.h
模型加载器的头文件，定义了加载器接口。

**主要接口：**
```cpp
class ModelLoader {
public:
    QVector<CpuMesh> loadCPU(const QString& filePath);  // 加载模型文件
    
private:
    QVector<CpuMesh> loadWithAssimp(const QString& path);  // 使用 Assimp 加载
    bool isAssimpFormat(const QString& ext) const;        // 检查文件格式
};
```

### ModelLoader.cpp
模型加载器的实现文件。

**核心功能：**
- 基于 Assimp 库的多格式模型加载
- 自动格式检测和验证
- 数据格式转换和优化
- 错误处理和日志记录

## 支持的文件格式

通过 Assimp 库，支持以下主流 3D 模型格式：

**常用格式：**
- OBJ (.obj) - Wavefront OBJ 文件
- FBX (.fbx) - Autodesk FBX 文件
- GLTF (.gltf, .glb) - GL Transmission Format
- 3DS (.3ds) - 3D Studio Max 文件
- DAE (.dae) - COLLADA 文件
- STL (.stl) - 立体光刻文件

**其他格式：**
- BLEND (.blend) - Blender 文件
- X (.x) - DirectX X 文件
- PLY (.ply) - Stanford PLY 文件
- 等等...

## 加载流程

1. **文件验证** - 检查文件是否存在和格式是否支持
2. **Assimp 导入** - 使用 Assimp 导入场景数据
3. **数据转换** - 将 Assimp 数据结构转换为 `CpuMesh`
4. **坐标系统转换** - 统一为右手坐标系
5. **法线计算** - 为缺失法线的网格计算顶点法线
6. **包围盒计算** - 计算每个网格的包围盒
7. **内存优化** - 优化顶点数据和索引数组

## 数据处理

**顶点数据处理：**
- 位置坐标直接映射
- 法线向量标准化
- 纹理坐标翻转（Y 轴）
- 切线空间计算（如需要）

**网格优化：**
- 移除重复顶点
- 优化索引顺序
- 计算包围盒
- 验证数据完整性

## 错误处理

- 文件不存在或无法读取
- 不支持的文件格式
- Assimp 导入失败
- 数据转换错误
- 内存不足等异常情况

## 性能考虑

- 大文件采用流式加载
- 多线程加载支持
- 内存池管理
- 缓存机制（可扩展）

## 使用示例

```cpp
ModelLoader loader;
QVector<CpuMesh> meshes = loader.loadCPU("model.obj");

if (!meshes.isEmpty()) {
    // 成功加载，处理网格数据
    for (const auto& mesh : meshes) {
        qDebug() << "Loaded mesh with" << mesh.vertices.size() << "vertices";
    }
}
```

## 依赖项

- **Assimp** - Open Asset Import Library，用于多格式 3D 模型加载
- **Qt Core** - 用于文件路径处理和容器类

## 扩展性

模块设计支持：
- 添加新的文件格式支持
- 自定义数据转换规则
- 加载进度回调
- 异步加载机制
