# ModelViewer-Qt6

基于 Qt6 RHI 的跨平台 3D 模型查看器

## 项目简介

ModelViewer-Qt6 是一个现代化的 3D 模型查看器，使用 Qt6 的 RHI（Rendering Hardware Interface）框架实现跨平台硬件加速渲染。支持多种 3D 模型格式，提供丰富的渲染模式和交互功能。

![渲染效果截图](screenshots/Screenshot%2021-07-24%20172616.jpg)

## 主要特性

### 核心功能
- **多格式支持**：基于 Assimp 库支持 OBJ、FBX、GLTF、3DS、DAE、STL 等主流 3D 格式
- **跨平台渲染**：通过 Qt RHI 支持 OpenGL、DirectX、Vulkan、Metal 等图形 API
- **硬件加速**：充分利用 GPU 进行高性能渲染
- **现代 C++**：使用 C++17 标准和 Google C++ 编码规范

### 渲染模式
- **Phong 着色**：经典的光照模型
- **线框模式**：显示模型网格拓扑结构
- **法线可视化**：顶点法线和面法线显示
- **平面着色**：无光照的平面颜色渲染
- **PBR 渲染**：基于物理的渲染
- **阴影映射**：实时阴影效果
- **天空盒**：环境背景渲染
- **选择高亮**：物体选中效果
- **裁剪平面**：几何体裁剪显示
- **分屏对比**：多种渲染模式对比

### 渲染效果展示

#### 基础渲染
![基础渲染](screenshots/Slide1.PNG)

#### 高级渲染模式
![高级渲染](screenshots/Slide2.PNG)

#### 纹理和材质
![纹理渲染](screenshots/Slide3.PNG)

#### 透明度和反射
![透明反射](screenshots/Screenshot%2021-07-24%20150032.jpg)

#### PBR 材质渲染
![PBR 渲染](screenshots/Slide4.PNG)

#### 天空盒环境
![天空盒](screenshots/Slide5.PNG)

#### 剖面视图
![剖面视图](screenshots/Slide6.PNG)

#### 多剖面视图
![多剖面](screenshots/Slide7.PNG)

#### 多投影模式
![多投影](screenshots/Screenshot%2021-07-24%20223138.jpg)

## 项目结构

```
ModelViewer-Qt6/
├── src/                    # 源代码目录
│   ├── core/               # 核心数据结构
│   │   ├── CpuMesh.h      # CPU 端网格数据
│   │   ├── Light.h         # 光源数据结构
│   │   └── Material.h      # 材质数据结构
│   ├── loader/             # 模型加载器
│   │   ├── ModelLoader.h    # 加载器接口
│   │   └── ModelLoader.cpp # Assimp 加载实现
│   ├── math/               # 数学库
│   │   ├── BoundingBox.h   # 包围盒计算
│   │   ├── Camera.h        # 摄像机控制
│   │   ├── Camera.cpp
│   │   ├── TrackBall.h     # 轨迹球控制器
│   │   └── TrackBall.cpp
│   ├── renderer/           # 渲染引擎
│   │   ├── RhiMesh.h      # GPU 网格管理
│   │   ├── RhiMesh.cpp
│   │   ├── RhiRenderer.h  # 主渲染器
│   │   └── RhiRenderer.cpp
│   ├── ui/                 # 用户界面
│   │   ├── MainWindow.h    # 主窗口
│   │   ├── MainWindow.cpp
│   │   ├── RhiWidget.h     # RHI 渲染控件
│   │   └── RhiWidget.cpp
│   └── main.cpp           # 程序入口
├── shaders/               # 着色器文件
├── res/                  # 资源文件
├── textures/              # 纹理资源
├── fonts/                # 字体文件
└── docs/                 # 文档目录
```

## 技术架构

### 渲染管线
- **Qt RHI 抽象层**：统一的图形 API 接口
- **多管线支持**：每种渲染模式独立的渲染管线
- **统一缓冲区**：高效的 GPU 数据管理
- **着色器系统**：自动编译和多平台支持

### 数据流
```
模型文件 → Assimp 加载 → CpuMesh → RhiMesh → GPU 渲染
```

### 交互系统
- **轨迹球控制**：直观的 3D 旋转
- **摄像机系统**：透视/正交投影切换
- **视图适配**：自动调整视角以包含整个模型
- **鼠标交互**：旋转、平移、缩放操作

## 构建要求

### 系统要求
- **操作系统**：Windows 10+、macOS 10.15+、Linux (Ubuntu 18.04+)
- **编译器**：MSVC 2019+、GCC 8+、Clang 10+
- **图形驱动**：支持 OpenGL 4.3+ 或 DirectX 11+

### 依赖库
- **Qt 6.9.3+**：GUI 框架和 RHI 支持
- **vcpkg**：C++ 包管理器
- **Assimp 5.0+**：3D 模型加载库

### 构建步骤

#### Windows (Visual Studio)
```bash
# 1. 安装 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# 2. 安装依赖
vcpkg install qt6:x64-windows
vcpkg install assimp:x64-windows

# 3. 配置 CMake
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 4. 编译
cmake --build build
```

#### Linux/macOS
```bash
# 1. 安装依赖
sudo apt-get install qt6-base-dev qt6-tools-dev assimp-dev

# 2. 配置和编译
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 使用指南

### 基本操作
1. **打开模型**：文件 → 打开，选择支持的 3D 模型文件
2. **视图控制**：
   - 鼠标左键拖拽：旋转模型
   - 鼠标右键拖拽：平移视图
   - 鼠标滚轮：缩放视图
3. **渲染模式**：视图菜单切换不同的渲染效果
4. **投影切换**：视图 → 投影模式，在透视/正交间切换

### 高级功能
- **材质设置**：工具 → 材质设置，调整光照参数
- **光照设置**：工具 → 光照设置，配置光源属性
- **背景设置**：工具 → 背景设置，自定义背景颜色
- **截图功能**：工具 → 截图，保存当前渲染画面

### 快捷键
- `Space`：重置视图
- `F`：聚焦选中物体
- `W`：切换线框模式
- `N`：显示法线
- `L`：切换光照
- `S`：切换阴影
- `F11`：全屏模式

## 开发指南

### 代码规范
- 遵循 Google C++ 编码规范
- 使用中文注释，提供详细的 API 文档
- 采用现代 C++ 特性（C++17）
- RAII 资源管理

### 扩展开发
1. **添加新渲染模式**：
   - 在 `RhiRenderer.h` 中添加枚举值
   - 实现对应的 `buildXXXPipeline()` 方法
   - 创建相应的着色器文件

2. **支持新模型格式**：
   - 扩展 `ModelLoader.cpp` 中的格式列表
   - 添加特定的数据转换逻辑

3. **自定义 UI 组件**：
   - 继承现有控件类
   - 使用 Qt 信号槽机制

## 性能优化

### 渲染优化
- **批处理渲染**：减少绘制调用
- **视锥剔除**：避免渲染不可见物体
- **LOD 系统**：距离相关的细节层次
- **实例化渲染**：大量相同物体优化

### 内存管理
- **对象池**：复用临时对象
- **延迟释放**：GPU 资源安全释放
- **智能指针**：自动内存管理

## 故障排除

### 常见问题
1. **模型加载失败**
   - 检查文件格式是否支持
   - 确认文件路径正确
   - 查看控制台错误信息

2. **渲染异常**
   - 更新显卡驱动
   - 检查 OpenGL/DirectX 支持
   - 尝试不同的渲染模式

3. **编译错误**
   - 确认依赖库版本
   - 检查 CMake 配置
   - 验证编译器版本

### 调试技巧
- 使用 Qt Creator 调试器
- 启用 RHI 调试层
- 检查着色器编译日志
- 监控 GPU 内存使用

## 贡献指南

### 提交规范
- 使用清晰的提交信息
- 遵循现有代码风格
- 添加适当的测试
- 更新相关文档

### 开发流程
1. Fork 项目仓库
2. 创建功能分支
3. 开发和测试
4. 提交 Pull Request

## 许可证

本项目采用 Apache License 2.0 许可证，详见 [LICENSE](LICENSE) 文件。

## 致谢

- **Qt 团队**：提供优秀的跨平台框架
- **Assimp 团队**：强大的 3D 模型加载库
- **开源社区**：各种工具和库的支持

## 联系方式

- **项目主页**：https://github.com/yuenar/ModelViewer-Qt6
- **问题反馈**：GitHub Issues
- **文档**：docs/ 目录下的详细文档

---

**感谢使用 ModelViewer-Qt6！** 🎉

如果这个项目对您有帮助，请考虑给我们一个 ⭐ Star !
