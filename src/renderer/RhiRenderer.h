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

#ifndef MODELVIEWER_SRC_RENDERER_RHIRENDERER_H_
#define MODELVIEWER_SRC_RENDERER_RHIRENDERER_H_

#include <rhi/qrhi.h>
#include "../math/Camera.h"
#include "../core/Material.h"
#include "../core/Light.h"
#include "../core/CpuMesh.h"

class RhiMesh;

/// @brief 帧统一缓冲区数据结构
/// 
/// 包含每帧渲染所需的基本变换和光照信息。
/// 16 字节对齐确保 GPU 高效访问。
struct alignas(16) FrameUBOData {
  float model[16];        ///< 模型矩阵（4x4）
  float view[16];         ///< 视图矩阵（4x4）
  float projection[16];   ///< 投影矩阵（4x4）
  float normalMatrix[16];  ///< 法线变换矩阵（4x4）
  float lightPos[3];      ///< 光源位置（x, y, z）
  float _pad0;           ///< 填充字节，16字节对齐
  float lightColor[3];    ///< 光源颜色（r, g, b）
  float _pad1;           ///< 填充字节，16字节对齐
  float viewPos[3];       ///< 摄像机位置（x, y, z）
  float _pad2;           ///< 填充字节，16字节对齐
};

/// @brief 材质统一缓冲区数据结构
/// 
/// 包含 Phong 光照模型的材质参数。
/// 16 字节对齐确保 GPU 高效访问。
struct alignas(16) MaterialUBOData {
  float ambient[3];     ///< 环境光反射系数（r, g, b）
  float shininess;        ///< 光泽度
  float diffuse[3];     ///< 漫反射系数（r, g, b）
  float _pad0;           ///< 填充字节，16字节对齐
  float specular[3];    ///< 镜面反射系数（r, g, b）
  float _pad1;           ///< 填充字节，16字节对齐
};

/// @brief 背景统一缓冲区数据结构
/// 
/// 包含渐变背景的颜色信息。
/// 16 字节对齐确保 GPU 高效访问。
struct alignas(16) BackgroundUBOData {
  float topColor[4];     ///< 顶部颜色（r, g, b, a）
  float botColor[4];     ///< 底部颜色（r, g, b, a）
};

/// @brief 渲染模式枚举
/// 
/// 定义了渲染器支持的各种渲染效果和可视化模式。
/// 每种模式对应不同的着色器和渲染管线。
enum class RenderMode {
  Phong = 0,           ///< Phong 光照着色
  Wireframe = 1,        ///< 线框模式
  Normals = 2,          ///< 顶点法线可视化
  FaceNormals = 3,       ///< 面法线可视化
  VertexNormals = 4,     ///< 顶点法线可视化（增强版）
  FlatShading = 5,       ///< 平面着色
  PBR = 6,              ///< 基于物理的渲染
  ShadowMapping = 7,     ///< 阴影映射
  Skybox = 8,           ///< 天空盒渲染
  Selection = 9,         ///< 选择高亮
  ClippingPlane = 10,    ///< 裁剪平面
  SplitScreen = 11       ///< 分屏显示
};

/// @brief 基于 Qt RHI 的跨平台渲染器
/// 
/// 该类是 ModelViewer 的核心渲染引擎，基于 Qt RHI（Rendering Hardware Interface）
/// 实现跨平台 GPU 渲染。支持 OpenGL、DirectX、Vulkan、Metal 等多种图形 API。
/// 
/// 主要功能：
/// - 多种渲染模式和效果
/// - 渲染管线管理和切换
/// - GPU 资源管理
/// - 着色器加载和编译
/// - 统一缓冲区管理
/// 
/// 架构特点：
/// - 每种渲染模式独立的渲染管线
/// - 延迟初始化和按需创建
/// - 资源复用和内存管理
/// - 跨平台兼容性
/// 
/// 使用示例：
/// @code
/// RhiRenderer renderer(rhi, renderTarget);
/// renderer.initialize(commandBuffer);
/// renderer.setRenderMode(RenderMode::Phong);
/// 
/// renderer.render(commandBuffer, meshes, camera, light, material);
/// @endcode
class RhiRenderer {
 public:
  /// @brief 构造函数
  /// 
  /// @param[in] rhi RHI 设备指针，用于创建 GPU 资源
  /// @param[in] rt 渲染目标，指定渲染输出表面
  /// 
  /// @note 构造后需要调用 initialize() 完成初始化
  RhiRenderer(QRhi* rhi, QRhiRenderTarget* rt);
  
  /// @brief 析构函数，清理所有 GPU 资源
  ~RhiRenderer();
  
  /// @brief 初始化渲染器
  /// 
  /// 创建默认的渲染管线和资源。
  /// 必须在构造后调用，才能进行渲染操作。
  /// 
  /// @param[in] cb 命令缓冲区，用于资源创建操作
  void initialize(QRhiCommandBuffer* cb);
  
  /// @brief 设置渲染目标
  /// 
  /// 更改渲染输出目标，通常在窗口大小改变时调用。
  /// 
  /// @param[in] rt 新的渲染目标
  /// 
  /// @note 会触发渲染管线的重新创建
  void setRenderTarget(QRhiRenderTarget* rt);
  
  /// @brief 释放并重建所有渲染管线
  /// 
  /// 在 RHI 设备丢失或重置时调用。
  /// 会清理当前的所有管线并重新创建。
  /// 
  /// @note 这是一个昂贵的操作，应谨慎使用
  void releaseAndRebuildPipelines();
  
  /// @brief 设置渲染模式（整数版本）
  /// 
  /// @param[in] mode 渲染模式的整数值
  void setRenderMode(int mode);
  
  /// @brief 设置渲染模式（枚举版本）
  /// 
  /// @param[in] mode 渲染模式枚举值
  void setRenderMode(RenderMode mode) { setRenderMode(static_cast<int>(mode)); }
  
  /// @brief 上传网格数据到 GPU
  /// 
  /// 批量上传多个网格的顶点和索引数据到 GPU。
  /// 
  /// @param[in] cb 命令缓冲区，用于数据上传操作
  /// @param[in] meshes 要上传的网格数组
  void uploadMeshes(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes);
  
  /// @brief 重置网格上传标志
  /// 
  /// 清除网格已上传的标志，强制下次重新上传。
  /// 在网格数据发生变化时调用。
  void resetMeshUploadFlag() { m_meshesUploaded = false; }
  
  /// @brief 设置背景颜色
  /// 
  /// 设置渐变背景的顶部和底部颜色。
  /// 
  /// @param[in] topColor 顶部颜色
  /// @param[in] botColor 底部颜色
  void setBackgroundColors(const QVector3D& topColor, const QVector3D& botColor);
  
  /// @brief 执行渲染操作
  /// 
  /// 根据当前渲染模式渲染所有网格。
  /// 这是渲染器的主要入口点。
  /// 
  /// @param[in] cb 命令缓冲区，用于录制渲染命令
  /// @param[in] meshes 要渲染的网格数组
  /// @param[in] cam 摄像机，提供视图和投影信息
  /// @param[in] light 光源，提供照明参数
  /// @param[in] mat 材质，提供表面属性
  void render(QRhiCommandBuffer* cb,
              const QVector<RhiMesh*>& meshes,
              const Camera& cam,
              const Light& light,
              const Material& mat);

 private:
  /// @brief 构建默认 Phong 渲染管线
  void buildPipeline();
  
  /// @brief 构建线框渲染管线
  void buildWireframePipeline();
  
  /// @brief 构建法线可视化渲染管线
  void buildNormalsPipeline();
  
  /// @brief 构建面法线可视化渲染管线
  void buildFaceNormalsPipeline();
  
  /// @brief 构建顶点法线可视化渲染管线
  void buildVertexNormalsPipeline();
  
  /// @brief 构建背景渲染管线
  void buildBackgroundPipeline();
  
  /// @brief 构建平面着色渲染管线
  void buildFlatShadingPipeline();
  
  /// @brief 构建物理渲染管线
  void buildPBRPipeline();
  
  /// @brief 构建阴影映射渲染管线
  void buildShadowMappingPipeline();
  
  /// @brief 构建天空盒渲染管线
  void buildSkyboxPipeline();
  
  /// @brief 构建选择高亮渲染管线
  void buildSelectionPipeline();
  
  /// @brief 构建裁剪平面渲染管线
  void buildClippingPlanePipeline();
  
  /// @brief 构建分屏渲染管线
  void buildSplitScreenPipeline();
  
  /// @brief 加载着色器
  /// 
  /// 从 QSB 文件加载预编译的着色器。
  /// 
  /// @param[in] qsbPath QSB 文件路径
  /// @return QShader 加载的着色器对象
  QShader loadShader(const QString& qsbPath);
  
  // === 成员变量 ===
  
  /// @brief RHI 设备指针
  QRhi* m_rhi;
  
  /// @brief 当前渲染目标
  QRhiRenderTarget* m_rt;
  
  // === Phong 渲染管线资源 ===
  QRhiGraphicsPipeline* m_pipeline = nullptr;        ///< 主渲染管线
  QRhiShaderResourceBindings* m_srb = nullptr;       ///< 着色器资源绑定
  QRhiBuffer* m_frameUBO = nullptr;                ///< 帧统一缓冲区
  QRhiBuffer* m_materialUBO = nullptr;             ///< 材质统一缓冲区
  
  // === 线框渲染管线资源 ===
  QRhiGraphicsPipeline* m_wireframePipeline = nullptr;  ///< 线框渲染管线
  QRhiShaderResourceBindings* m_wireframeSrb = nullptr;  ///< 线框着色器绑定
  
  // === 法线可视化渲染管线资源 ===
  QRhiGraphicsPipeline* m_normalsPipeline = nullptr;     ///< 法线渲染管线
  QRhiShaderResourceBindings* m_normalsSrb = nullptr;    ///< 法线着色器绑定
  QRhiBuffer* m_normalsUBO = nullptr;                  ///< 法线统一缓冲区
  
  // === 面法线可视化渲染管线资源 ===
  QRhiGraphicsPipeline* m_faceNormalsPipeline = nullptr;   ///< 面法线渲染管线
  QRhiShaderResourceBindings* m_faceNormalsSrb = nullptr;  ///< 面法线着色器绑定
  QRhiBuffer* m_faceNormalsUBO = nullptr;               ///< 面法线统一缓冲区
  
  // === 顶点法线可视化渲染管线资源 ===
  QRhiGraphicsPipeline* m_vertexNormalsPipeline = nullptr; ///< 顶点法线渲染管线
  QRhiShaderResourceBindings* m_vertexNormalsSrb = nullptr; ///< 顶点法线着色器绑定
  QRhiBuffer* m_vertexNormalsUBO = nullptr;             ///< 顶点法线统一缓冲区
  
  // === 平面着色渲染管线资源 ===
  QRhiGraphicsPipeline* m_flatShadingPipeline = nullptr;  ///< 平面着色渲染管线
  QRhiShaderResourceBindings* m_flatShadingSrb = nullptr;  ///< 平面着色着色器绑定
  
  // === 背景渲染管线资源 ===
  QRhiGraphicsPipeline* m_bgPipeline = nullptr;          ///< 背景渲染管线
  QRhiShaderResourceBindings* m_bgSrb = nullptr;         ///< 背景着色器绑定
  QRhiBuffer* m_bgUBO = nullptr;                       ///< 背景统一缓冲区
  QRhiBuffer* m_bgVBuf = nullptr;                      ///< 背景顶点缓冲区
  
  // === PBR 渲染管线资源 ===
  QRhiGraphicsPipeline* m_pbrPipeline = nullptr;          ///< PBR 渲染管线
  QRhiShaderResourceBindings* m_pbrSrb = nullptr;         ///< PBR 着色器绑定
  
  // === 阴影映射渲染管线资源 ===
  QRhiGraphicsPipeline* m_shadowMappingPipeline = nullptr;  ///< 阴影映射渲染管线
  QRhiShaderResourceBindings* m_shadowMappingSrb = nullptr; ///< 阴影映射着色器绑定
  
  // === 天空盒渲染管线资源 ===
  QRhiGraphicsPipeline* m_skyboxPipeline = nullptr;       ///< 天空盒渲染管线
  QRhiShaderResourceBindings* m_skyboxSrb = nullptr;      ///< 天空盒着色器绑定
  
  // === 选择高亮渲染管线资源 ===
  QRhiGraphicsPipeline* m_selectionPipeline = nullptr;     ///< 选择高亮渲染管线
  QRhiShaderResourceBindings* m_selectionSrb = nullptr;    ///< 选择高亮着色器绑定
  
  // === 裁剪平面渲染管线资源 ===
  QRhiGraphicsPipeline* m_clippingPlanePipeline = nullptr;  ///< 裁剪平面渲染管线
  QRhiShaderResourceBindings* m_clippingPlaneSrb = nullptr; ///< 裁剪平面着色器绑定
  
  // === 分屏渲染管线资源 ===
  QRhiGraphicsPipeline* m_splitScreenPipeline = nullptr;    ///< 分屏渲染管线
  QRhiShaderResourceBindings* m_splitScreenSrb = nullptr;   ///< 分屏着色器绑定
  
  /// @brief 当前渲染模式
  int m_renderMode = 0;
  
  /// @brief 网格数据已上传标志
  bool m_meshesUploaded = false;
  
  /// @brief 背景顶部颜色
  QVector3D m_bgTopColor{0.2f, 0.2f, 0.3f};
  
  /// @brief 背景底部颜色
  QVector3D m_bgBotColor{0.1f, 0.1f, 0.15f};
};

#endif  // MODELVIEWER_SRC_RENDERER_RHIRENDERER_H_
