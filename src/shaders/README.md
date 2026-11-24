# Shaders 目录

本目录包含 ModelViewer 所有的着色器文件，支持多种渲染效果和可视化功能。

> **重要**：只有 `src/CMakeLists.txt` 中 `MODELVIEWER_SHADER_FILES` 列出的文件会参与
> `qt_add_shaders` 编译。所有 `*.geom` 源文件（normals.geom、face_normal.geom、axis.geom、
> twoside_per_fragment.geom、subdiv.geom 等）**保留但不参与构建**——几何着色器在 HLSL/MSL
> 没有等价物，法线与线框线段改为 CPU 生成。下文提到 `.geom` 的条目请按此理解。
>
> 实际参与编译的清单：phong、flat、wireframe、wire_overlay.frag、normals、normal_lines、
> background、pbr、shadow_depth、shadowed_phong、skybox、selection、clipped_mesh、cap、
> text3d、splitScreen、post、axis（均为 .vert/.frag，除 wire_overlay 只有 .frag）。

## 着色器分类

### 基础渲染着色器

#### phong.vert / phong.frag
**Phong 光照模型着色器**
- 实现经典的 Phong 光照计算
- 支持环境光、漫反射、镜面反射
- 可配置光照参数和材质属性

#### wireframe.vert / wireframe.frag
**线框模式着色器**
- 渲染网格的边框线条
- 支持线条颜色和宽度设置
- 用于查看模型拓扑结构

### 可视化着色器

#### normals.vert / normals.geom / normals.frag
**顶点法线可视化**
- 在每个顶点处绘制法线向量
- 使用几何着色器生成线条
- 便于检查法线计算正确性

#### vertex_normal.vert / vertex_normal.geom / vertex_normal.frag
**顶点法线可视化（增强版）**
- 更详细的顶点法线显示
- 支持法线长度和颜色配置
- 包含长度归一化选项

#### face_normal.vert / face_normal.geom / face_normal.frag
**面法线可视化**
- 显示每个三角面的法线
- 几何着色器计算面中心点
- 用于检查面法线方向

### 特效着色器

#### background.vert / background.frag
**背景渲染着色器**
- 渲染纯色或渐变背景
- 支持颜色插值和透明度
- 为场景提供背景色

#### selection.vert / selection.frag
**选择高亮着色器**
- 高亮显示选中的物体
- 支持高亮颜色和强度
- 用于物体选择反馈

#### clipped_mesh.vert / clipped_mesh.frag
**裁剪网格着色器**
- 支持平面裁剪效果
- 显示裁剪截面
- 用于模型剖面查看

#### clipping_plane.vert / clipping_plane.frag
**裁剪平面可视化**
- 渲染裁剪平面的半透明表面
- 显示裁剪边界
- 便于调整裁剪参数

### 光照和阴影着色器

#### light_cube.vert / light_cube.frag
**光源可视化着色器**
- 渲染光源位置的立方体
- 显示光源颜色和强度
- 用于调试光照设置

#### shadow_mapping_depth.vert / shadow_mapping_depth.frag
**阴影映射深度着色器**
- 生成深度贴图用于阴影
- 从光源视角渲染深度
- 支持实时阴影计算

### 环境渲染着色器

#### skybox.vert / skybox.frag
**天空盒着色器**
- 渲染立方体贴图天空盒
- 支持环境光反射
- 提供 360 度背景

#### irradiance_convolution.frag
**辐照度卷积着色器**
- 计算环境光的辐照度
- 用于基于图像的光照
- 支持 PBR 渲染

### 后处理着色器

#### brdf.vert / brdf.frag
**BRDF 积分着色器**
- 计算双向反射分布函数
- 生成 BRDF 查找表
- 用于 PBR 材质渲染

#### splitScreen.vert / splitScreen.frag
**分屏显示着色器**
- 实现分屏对比效果
- 支持多种分割方式
- 用于渲染效果对比

### 调试着色器

#### debug_quad.vert / debug_quad_depth.frag
**调试四边形着色器**
- 渲染全屏四边形用于调试
- 可视化深度缓冲区
- 显示各种渲染缓冲区内容

#### axis.vert / axis.geom / axis.frag
**坐标轴着色器**
- 渲染 3D 坐标轴
- X、Y、Z 轴不同颜色
- 用于方向参考

### 文本渲染着色器

#### text.vert / text.frag
**文本渲染着色器**
- 在 3D 场景中渲染文本
- 支持字体纹理和颜色
- 用于显示信息标签

### 高级渲染着色器

#### twoside_per_fragment.vert / twoside_per_fragment.geom / twoside_per_fragment.frag
**双面片元着色器**
- 实现双面渲染效果
- 支持不同面的不同材质
- 用于透明和半透明物体

#### subdiv.geom
**细分几何着色器**
- 实现几何细分
- 增加网格细节
- 用于 LOD 和平滑效果

## 着色器特性

### 跨平台支持
- **GLSL** - 手写源文件，编译为 460（桌面）与 310 es（Android GLES）变体
- **HLSL 50** - DirectX 11/12 变体（qsb 从 GLSL 转译）
- **MSL 12** - Metal 变体
- **SPIR-V** - qsb 从 GLSL 生成、供 Vulkan 后端加载的二进制变体（Android API > 31）

### 版本兼容
- **OpenGL ES 3.1（310 es）** - Android 低版本回退后端；清单声明 `glEsVersion 0x00030001`
- **OpenGL 4.6 / GLSL 460** - 桌面端变体
- **DirectX 11（HLSL 50）** / **Metal 12（MSL 12）** - 其它桌面后端

## 编译配置

着色器由 `qt_add_shaders` 在构建时通过 qsb 编译，桌面与 Android 使用同一段配置：

```cmake
qt_add_shaders(ModelViewer "shaders"
    GLSL "460,310 es"     # 桌面 460 + Android GLES 310 es
    HLSL 50
    MSL 12
    PRECOMPILE
    OPTIMIZE
    FILES ${MODELVIEWER_SHADER_FILES}
)
```

`310 es` 变体给 OpenGL ES 3.1 后端；SPIR-V 变体给 Vulkan 后端（Android `SDK_INT > 31`）。
桌面 Shader Editor 另外在运行时用 `qsb` 编译源码并热重建管线，移动端没有该编辑器。

## 使用方式

着色器由 `RhiRenderer` 类加载和管理。每种渲染模式选择适当的着色器组合来实现所需的视觉效果。

## 着色器统一变量

### 常用 Uniform
```glsl
uniform mat4 u_modelMatrix;        // 模型矩阵
uniform mat4 u_viewMatrix;         // 视图矩阵
uniform mat4 u_projectionMatrix;   // 投影矩阵
uniform mat4 u_mvpMatrix;          // 模型视图投影矩阵
uniform mat3 u_normalMatrix;       // 法线矩阵

uniform vec3 u_lightPosition;      // 光源位置
uniform vec3 u_lightColor;         // 光源颜色
uniform float u_lightIntensity;    // 光照强度

uniform vec3 u_cameraPosition;     // 摄像机位置
uniform float u_time;              // 时间参数
```

### 材质参数
```glsl
uniform vec3 u_albedo;             // 反射率
uniform float u_metallic;          // 金属度
uniform float u_roughness;         // 粗糙度
uniform float u_ao;                // 环境光遮蔽

uniform sampler2D u_albedoMap;     // 反射率贴图
uniform sampler2D u_normalMap;     // 法线贴图
uniform sampler2D u_metallicMap;   // 金属度贴图
uniform sampler2D u_roughnessMap;  // 粗糙度贴图
uniform sampler2D u_aoMap;         // 环境光遮蔽贴图
```

## 性能优化

### GPU 优化
- **分支最小化** - 减少 if-else 分支
- **纹理采样优化** - 使用 mipmap 和各向异性过滤
- **精度选择** - 合理使用 float/mediump/lowp
- **循环展开** - 避免动态循环

### 内存带宽
- **数据局部性** - 优化缓冲区访问模式
- **压缩纹理** - 使用 BC/ETC 压缩格式
- **实例化渲染** - 减少顶点数据传输
- **统一缓冲区** - 批量更新 uniform 数据

## 扩展指南

### 添加新着色器
1. 创建 `.vert` / `.frag` 文件（不要依赖几何着色器）
2. 加入 `src/CMakeLists.txt` 的 `MODELVIEWER_SHADER_FILES`（桌面/Android 共用同一列表）
3. 保留 `GLSL "460,310 es"` 变体字符串，否则 GLES 后端无法加载
4. 在 `RhiRenderer` 中创建管线并绑定 uniform
5. 实现渲染调用逻辑并在两种后端上验证

### 最佳实践
- 使用描述性的变量名
- 添加适当的注释
- 考虑性能影响
- 测试不同平台兼容性
- 保持代码模块化
