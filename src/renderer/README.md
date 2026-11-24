# Renderer 模块

本模块是 ModelViewer 的核心渲染引擎，基于 Qt RHI（Rendering Hardware Interface）实现跨平台 GPU 渲染。

## 桌面 / 移动端共用

桌面 `RhiWidget` 与 Android `MobileRhiItem` 都直接复用本模块的 `RhiRenderer`，渲染核心零改动：
- 移动端 `MobileRhiItemRenderer` 持有 `RhiRenderer*`，在 `synchronize()` 中把 Bridge 快照
  逐项设进渲染器，在 `render()` 中调用 `m_renderer->render(cb, meshes, camera, light, material)`；
- 网格 `RhiMesh` 的 GPU 缓冲在渲染线程创建/释放；`BufferPool` 与统计 `RenderStats` 同样共用；
- Android 可走 Vulkan（API > 31）或 OpenGL ES 3.1，着色器由 `qt_add_shaders` 一次编译出
  GLSL 460 / 310 es、HLSL 50、MSL 12（含 SPIR-V 变体）。

## 文件说明

### RhiRenderer.h/cpp
主渲染器类，管理所有渲染管线和 GPU 资源。

**核心职责：**
- RHI 设备初始化和管理
- 渲染管线创建和配置
- GPU 资源（缓冲区、纹理、采样器）管理
- 渲染命令录制和提交
- 多种渲染效果的实现

**主要渲染管线（`RhiRenderer::RenderMode`，0–11）：**
Phong、Wireframe、Normals、FaceNormals、VertexNormals、FlatShading、PBR、
ShadowMapping、Skybox、Selection、ClippingPlane、SplitScreen；另叠加
`DisplayStyle`（0–3：Shaded / Wireframe / WireShaded / Realistic）。
编译到 qsb 的辅助着色器还包括 cap、text3d、axis、post、wire_overlay 等。

### RhiMesh.h/cpp
GPU 端网格数据管理类。

**主要功能：**
- 顶点缓冲区和索引缓冲区创建
- 网格数据的 GPU 上传
- 绘制命令的封装
- 资源生命周期管理

## 渲染特性

### 基础渲染
- **Phong 着色** - 经典的光照模型
- **线框模式** - 网格边框显示
- **顶点着色** - 按顶点属性着色
- **纹理映射** - 支持 2D 纹理和立方体贴图

### 高级效果
- **法线可视化** - 顶点法线和面法线显示
- **阴影映射** - 实时阴影生成
- **环境映射** - 天空盒和环境光
- **裁剪平面** - 几何体裁剪显示
- **选择高亮** - 物体选中效果

### 后处理
- **色调映射** - HDR 到 LDR 转换
- **伽马校正** - 颜色空间转换
- **FXAA** - 快速近似抗锯齿
- **基于深度的 AO** - 深度驱动的暗角/环境光遮蔽后处理（`post.vert/frag`）

## 技术架构

### RHI 抽象层
Qt RHI 提供了统一的图形 API 抽象：
- **OpenGL** - 跨平台支持
- **DirectX 11/12** - Windows 平台
- **Vulkan** - 高性能现代 API
- **Metal** - macOS/iOS 平台
- **Android 后端选择** - `main_mobile.cpp` 读取 `SDK_INT`：`> 31` 用 Vulkan，否则 OpenGL ES 3.1
  （通过 `QSG_RHI_BACKEND` 设置，`QSurfaceFormat` 指定 ES 3.1 下限）

### 渲染管线设计
```cpp
class RhiRenderer {
private:
    // RHI 资源
    QRhi* rhi;                    // RHI 设备
    QRhiSwapChain* swapChain;     // 交换链
    QRhiRenderPass* renderPass;   // 渲染通道
    
    // 渲染管线
    QMap<QString, QRhiGraphicsPipeline*> pipelines;
    
    // GPU 资源
    QVector<QRhiBuffer*> uniformBuffers;
    QVector<QRhiTexture*> textures;
    QVector<QRhiSampler*> samplers;
};
```

### 着色器管理
- **Qt Shader Tools** - `qt_add_shaders` 在构建时预编译为 qsb（`PRECOMPILE` + `OPTIMIZE`）
- **变体** - `GLSL "460,310 es"` / `HLSL 50` / `MSL 12`；桌面与 Android 共用同一份
  `MODELVIEWER_SHADER_FILES` 清单
- **几何着色器** - `*.geom` 源文件保留但不参与编译；法线/线框线段由 CPU 生成
- **热重载** - 桌面 Shader Editor 运行时用 `qsb` 编译并重建管线（移动端无此功能）

## 渲染流程

### 初始化阶段
1. RHI 设备创建和配置
2. 交换链和渲染通道设置
3. 着色器编译和管线创建
4. 基础几何体和纹理生成

### 每帧渲染
1. 更新 uniform 缓冲区
2. 设置渲染目标和视口
3. 绑定渲染管线和资源
4. 录制绘制命令
5. 提交命令缓冲区
6. 呈现到屏幕

### 资源管理
- **延迟初始化** - 按需创建 GPU 资源
- **资源池** - 复用缓冲区和纹理
- **内存监控** - GPU 内存使用统计
- **自动清理** - 资源生命周期管理

## 性能优化

### GPU 端优化
- **批处理渲染** - 减少绘制调用
- **实例化渲染** - 大量相同物体
- **遮挡剔除** - 不可见物体跳过
- **LOD 系统** - 距离相关的细节层次

### CPU 端优化
- **命令缓冲区** - 批量提交渲染命令
- **多线程渲染** - 并行处理渲染任务
- **缓存机制** - 避免重复计算
- **内存池** - 减少动态分配

## 调试工具

### 渲染调试
- **线框模式** - 查看网格拓扑
- **法线显示** - 检查光照计算
- **深度缓冲** - 可视化深度信息
- **性能分析** - GPU 时间统计

### 错误处理
- **设备丢失** - GPU 设备恢复
- **内存不足** - 优雅降级
- **着色器编译** - 错误信息收集
- **资源验证** - 参数有效性检查

## 使用示例

```cpp
// 移动端（MobileRhiItemRenderer::synchronize + render）的实际调用方式：
renderer->setRenderMode(bridge.renderMode());
renderer->setDisplayStyle(bridge.displayStyle());
renderer->setAxisVisible(bridge.axisVisible());
// ... 其余参数逐项设置 ...
renderer->setRenderTarget(renderTarget());
renderer->render(cb, meshes, camera, light, material);
```

## 依赖项

- **Qt RHI** - 渲染硬件接口
- **Qt Gui** - 图形相关基础类
- **Core 模块** - 数据结构定义
- **Math 模块** - 数学工具函数

## 扩展性

模块设计支持：
- 添加新的渲染管线
- 自定义着色器效果
- 后处理插件系统
- 渲染配置文件
