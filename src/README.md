# ModelViewer-Qt6 源代码目录

本目录包含了 ModelViewer-Qt6 项目的所有源代码文件。

## 目录结构

```
src/
├── main.cpp              # 程序入口点
├── CMakeLists.txt        # CMake 构建配置文件
├── core/                 # 核心数据结构
├── loader/               # 模型加载器
├── math/                 # 数学库
├── renderer/             # 渲染器
├── ui/                   # 用户界面
└── shaders/              # 着色器文件
```

## 模块说明

### core/ - 核心数据结构
包含应用程序的基础数据结构：
- `CpuMesh.h` - CPU 端网格数据结构，定义顶点和索引格式
- `Light.h` - 光照数据结构
- `Material.h` - 材质数据结构

### loader/ - 模型加载器
负责加载各种 3D 模型格式：
- `ModelLoader.cpp/h` - 基于 Assimp 的模型加载器
- 支持多种 3D 模型格式的导入

### math/ - 数学库
提供 3D 图形学相关的数学工具：
- `BoundingBox.cpp/h` - 包围盒计算和管理
- `Camera.cpp/h` - 摄像机控制和视图矩阵
- `TrackBall.cpp/h` - 轨迹球控制器，用于模型旋转

### renderer/ - 渲染器
基于 Qt RHI 的跨平台渲染引擎：
- `RhiRenderer.cpp/h` - 主渲染器类，管理所有渲染管线
- `RhiMesh.cpp/h` - GPU 端网格数据管理

### ui/ - 用户界面
Qt Widgets 用户界面组件：
- `MainWindow.cpp/h` - 主窗口
- `RhiWidget.cpp/h` - RHI 渲染窗口组件
- `LightDialog.cpp/h` - 光照设置对话框
- `MaterialDialog.cpp/h` - 材质编辑对话框

### shaders/ - 着色器
GLSL/HLSL/MSL 着色器文件，支持多种渲染效果：
- Phong 着色、线框模式、法线可视化
- 阴影映射、环境光遮蔽、天空盒
- 裁剪平面、选择高亮等特效

## 技术特点

- **跨平台渲染**：基于 Qt RHI，支持 OpenGL、DirectX、Vulkan、Metal
- **现代 C++**：使用 C++17 标准
- **模块化设计**：清晰的模块分离，便于维护和扩展
- **丰富的渲染特性**：支持多种着色器和渲染效果

## 构建要求

- Qt 6.9.3 或更高版本
- CMake 3.20 或更高版本
- vcpkg 包管理器
- Assimp 库（用于模型加载）

## 入口点

程序从 `main.cpp` 开始，创建 QApplication 实例并显示主窗口。
