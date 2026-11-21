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
#include <QImage>
#include <QMatrix4x4>
#include <QSize>
#include <QString>
#include <QVector>
#include <QVector4D>

#include "../math/Camera.h"
#include "../math/Frustum.h"
#include "../core/Material.h"
#include "../core/Light.h"
#include "../core/CpuMesh.h"
#include "BufferPool.h"

class RhiMesh;

/// @brief 帧统一缓冲区（std140，与着色器 FrameUBO 一致）
struct alignas(16) FrameUBOData {
  float model[16];
  float view[16];
  float projection[16];
  float normalMatrix[16];
  float lightPos[3];   float _pad0;
  float lightColor[3]; float _pad1;
  float viewPos[3];    float _pad2;
  float lightSpace[16];
  float shadowBias[16];
};

/// @brief 材质统一缓冲区（std140，与着色器 MaterialUBO 一致）
struct alignas(16) MaterialUBOData {
  float ambient[3];  float shininess;
  float diffuse[3];  float metallic;
  float specular[3]; float roughness;
};

/// @brief 渐变背景统一缓冲区
struct alignas(16) BackgroundUBOData {
  float topColor[4];
  float botColor[4];
};

/// @brief 法线可视化参数
struct alignas(16) NormalUBOData {
  float color[3];
  float length;
};

/// @brief 裁剪平面参数（世界空间，dot(plane, vec4(pos,1)) >= 0 保留）
struct alignas(16) ClipUBOData {
  float plane[4];
};

/// @brief 阴影渲染参数
struct alignas(16) ShadowUBOData {
  float lightSpace[16];
  float shadowBias[16];
};

/// @brief 后处理参数（FXAA / 深度 AO）
struct alignas(16) PostUBOData {
  float texelX;
  float texelY;
  float flipY;
  float aoStrength;
  float nearPlane;
  float farPlane;
  float aoRadius;
  float fxaaEnabled;
  float aoEnabled;
  float exposure;
  float padding0;
  float padding1;
};

/// @brief 选择高亮颜色
struct alignas(16) SelectionUBOData {
  float color[4];
};

/// @brief 天空盒参数
struct alignas(16) SkyboxUBOData {
  float projection[16];
  float view[16];
};

/// @brief 渲染模式枚举
enum class RenderMode {
  Phong = 0,
  Wireframe = 1,
  Normals = 2,
  FaceNormals = 3,
  VertexNormals = 4,
  FlatShading = 5,
  PBR = 6,
  ShadowMapping = 7,
  Skybox = 8,
  Selection = 9,
  ClippingPlane = 10,
  SplitScreen = 11
};

/// @brief 显示风格（叠加在渲染模式之上）
enum class DisplayStyle {
  Shaded = 0,
  Wireframe = 1,
  WireShaded = 2,
  Realistic = 3
};

/// @brief 单帧渲染统计（HUD 展示 / 性能验证）
struct RenderStats {
  int objects = 0;        ///< 场景对象总数
  int drawn = 0;          ///< 实际绘制对象数
  int culled = 0;         ///< 视锥剔除对象数
  int drawCalls = 0;      ///< 绘制调用次数
  int triangles = 0;      ///< 提交的三角形数量
  int lod0 = 0;           ///< 使用 LOD0 的对象数
  int lod1 = 0;           ///< 使用 LOD1 的对象数
  int lod2 = 0;           ///< 使用 LOD2 的对象数
  int pooledBuffers = 0;  ///< 缓冲池缓存数量
  int materialBatches = 0;///< 材质状态切换次数（批处理效果）
};

/// @brief 基于 Qt RHI 的跨平台渲染器
class RhiRenderer {
 public:
  RhiRenderer(QRhi* rhi, QRhiRenderTarget* rt);
  ~RhiRenderer();

  void initialize(QRhiCommandBuffer* cb);
  void releaseResources();

  void setRenderTarget(QRhiRenderTarget* rt);
  void rebuildPipelines();

  QRhi* rhi() const { return m_rhi; }
  bool isInitialized() const { return m_initialized; }

  void setRenderMode(int mode);
  RenderMode renderMode() const { return m_renderMode; }

  void setDisplayStyle(int style);
  DisplayStyle displayStyle() const { return m_displayStyle; }

  void setMultiView(bool enabled);
  bool isMultiView() const { return m_multiView; }

  void setAxisVisible(bool visible);
  void setGridVisible(bool visible);

  void uploadMeshes(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes);
  void resetMeshUploadFlag();

  void setBackgroundColors(const QVector3D& topColor, const QVector3D& botColor);
  void setNormalLength(float length);
  void setClipPlane(const QVector4D& plane);
  void setSelectedMesh(int index) { m_selectedMesh = index; }
  int selectedMesh() const { return m_selectedMesh; }

  /// @brief 设置场景漫反射贴图（空图片则使用 1x1 白色回退）
  void setDiffuseTexture(const QImage& image);

  /// @brief 视锥剔除开关（默认开启）
  void setCullingEnabled(bool enabled) { m_cullEnabled = enabled; }
  bool isCullingEnabled() const { return m_cullEnabled; }

  /// @brief 距离 LOD 开关（默认开启）
  void setLodEnabled(bool enabled) { m_lodEnabled = enabled; }
  bool isLodEnabled() const { return m_lodEnabled; }

  /// @brief 最近一帧的渲染统计
  const RenderStats& stats() const { return m_stats; }

  /// @brief 后处理开关（FXAA / 深度 AO）
  ///
  /// 开启后场景先渲染到离屏纹理，再通过全屏后处理通道输出。
  /// 与 MSAA 互斥：MSAA 开启时应关闭后处理。
  void setPostProcessing(bool fxaa, bool ao, float aoStrength = 0.6f);
  bool isFxaaEnabled() const { return m_fxaaEnabled; }
  bool isAoEnabled() const { return m_aoEnabled; }
  bool isPostEnabled() const { return m_fxaaEnabled || m_aoEnabled; }

  void render(QRhiCommandBuffer* cb,
              const QVector<RhiMesh*>& meshes,
              const Camera& cam,
              const Light& light,
              const Material& mat);

 private:
  struct PipelineDesc {
    QString vertShader;
    QString fragShader;
    QRhiVertexInputLayout layout;
    QRhiShaderResourceBindings* srb = nullptr;
    QRhiGraphicsPipeline::Topology topology = QRhiGraphicsPipeline::Triangles;
    bool depthTest = true;
    bool depthWrite = true;
    QRhiGraphicsPipeline::CompareOp depthOp = QRhiGraphicsPipeline::Less;
    QRhiGraphicsPipeline::CullMode cull = QRhiGraphicsPipeline::Back;
    QRhiGraphicsPipeline::FrontFace frontFace = QRhiGraphicsPipeline::CCW;
    QRhiRenderPassDescriptor* renderPass = nullptr;
    int sampleCount = -1;   ///< -1 自动（离屏/交换链），否则使用指定采样数
  };

  QRhiGraphicsPipeline* createPipeline(const PipelineDesc& desc);
  QShader loadShader(const QString& qsbPath);

  void createBuffers(QRhiCommandBuffer* cb);
  void destroyBuffers();
  void createShadowResources();
  void destroyShadowResources();
  void createSkyboxGeometry(QRhiCommandBuffer* cb);
  void destroySkyboxGeometry();
  void createDiffuseTexture();
  void destroyDiffuseTexture();

  /// @brief 创建/重建离屏场景渲染目标（尺寸变化时）
  void ensureSceneTarget(const QSize& size);
  void destroySceneTarget();
  void createPostResources();
  void destroyPostResources();
  void drawPost(QRhiCommandBuffer* cb);

  void rebuildCapGeometry(const QVector<RhiMesh*>& meshes);
  void uploadCapGeometry(QRhiResourceUpdateBatch* batch);
  void drawCap(QRhiCommandBuffer* cb, int slot);

  void createLabelResources(QRhiCommandBuffer* cb);
  void destroyLabelResources();
  void drawAxisLabels(QRhiCommandBuffer* cb, int slot);

  void buildAllPipelines();
  void destroyPipelines();

  FrameUBOData makeFrameData(const Camera& cam, const Light& light,
                             float aspect, const QMatrix4x4& lightSpace,
                             const QMatrix4x4& shadowBias) const;

  void uploadAxisGrid(QRhiResourceUpdateBatch* batch);
  void drawAxisGrid(QRhiCommandBuffer* cb, int slot);

  void renderShadowPass(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                        const QMatrix4x4& lightSpace, const QMatrix4x4& shadowBias);
  void drawBackground(QRhiCommandBuffer* cb);
  void drawSkybox(QRhiCommandBuffer* cb);
  void drawMeshesIndexed(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                         QRhiShaderResourceBindings* srb, int slot,
                         bool useMaterial = false);
  void drawWireframe(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                     QRhiShaderResourceBindings* srb, int slot);
  void drawNormalLines(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                       bool faceNormals, QRhiShaderResourceBindings* srb, int slot);
  void drawSurface(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes, int slot);
  void drawWireOverlay(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes, int slot);
  void drawMultiView(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                     float width, float height);
  void updateSceneBounds(const QVector<RhiMesh*>& meshes);

  /// @brief 切换当前视口的相机状态（视锥、LOD、包围球测试使用）
  void beginSlot(int slot);
  bool isMeshCulled(const RhiMesh* mesh) const;
  int lodLevelFor(const RhiMesh* mesh) const;

  QRhiCommandBuffer::DynamicOffset frameOffset(int slot) const {
    return QRhiCommandBuffer::DynamicOffset(
        0, quint32(slot) * quint32(m_frameStride));
  }

  QRhiCommandBuffer::DynamicOffset materialOffset(int slot) const {
    return QRhiCommandBuffer::DynamicOffset(
        1, quint32(slot) * quint32(m_materialStride));
  }

  // ── 顶点布局 ──────────────────────────────────────────────
  static QRhiVertexInputLayout meshInputLayout();
  static QRhiVertexInputLayout lineInputLayout();
  static QRhiVertexInputLayout backgroundInputLayout();
  static QRhiVertexInputLayout skyboxInputLayout();
  static QRhiVertexInputLayout colorVertexInputLayout();

  QRhi* m_rhi = nullptr;
  QRhiRenderTarget* m_rt = nullptr;
  bool m_initialized = false;

  // ── 公共 UBO ──────────────────────────────────────────────
  QRhiBuffer* m_frameUBO = nullptr;      ///< 动态偏移，多槽位
  int m_frameStride = 0;
  int m_frameSlots = 8;
  QRhiBuffer* m_materialUBO = nullptr;
  int m_materialStride = 0;
  int m_materialSlots = 256;
  QRhiTexture* m_diffuseTexture = nullptr;
  QRhiSampler* m_diffuseSampler = nullptr;
  QImage m_diffuseImage;
  bool m_diffuseDirty = false;
  bool m_diffuseUploadPending = false;
  QRhiBuffer* m_bgUBO = nullptr;
  QRhiBuffer* m_bgVBuf = nullptr;
  QRhiBuffer* m_normalUBO = nullptr;
  QRhiBuffer* m_clipUBO = nullptr;
  QRhiBuffer* m_selectionUBO = nullptr;
  QRhiBuffer* m_shadowUBO = nullptr;
  QRhiBuffer* m_skyboxUBO = nullptr;
  QRhiBuffer* m_skyboxVBuf = nullptr;

  // ── 坐标轴 / 网格 ─────────────────────────────────────────
  QRhiBuffer* m_axisVBuf = nullptr;
  QRhiBuffer* m_gridVBuf = nullptr;
  int m_axisVertexCount = 0;
  int m_gridVertexCount = 0;
  bool m_axisDirty = true;
  bool m_axisVisible = false;
  bool m_gridVisible = false;

  // ── 背景 ──────────────────────────────────────────────────
  QRhiGraphicsPipeline* m_bgPipeline = nullptr;
  QRhiShaderResourceBindings* m_bgSrb = nullptr;

  // ── 基础着色 ──────────────────────────────────────────────
  QRhiGraphicsPipeline* m_phongPipeline = nullptr;
  QRhiShaderResourceBindings* m_phongSrb = nullptr;
  QRhiGraphicsPipeline* m_wireframePipeline = nullptr;
  QRhiGraphicsPipeline* m_wireOverlayPipeline = nullptr;
  QRhiShaderResourceBindings* m_wireframeSrb = nullptr;
  QRhiGraphicsPipeline* m_normalsPipeline = nullptr;
  QRhiShaderResourceBindings* m_normalsSrb = nullptr;
  QRhiGraphicsPipeline* m_flatPipeline = nullptr;
  QRhiShaderResourceBindings* m_flatSrb = nullptr;
  QRhiGraphicsPipeline* m_pbrPipeline = nullptr;
  QRhiShaderResourceBindings* m_pbrSrb = nullptr;

  // ── 法线线段 ──────────────────────────────────────────────
  QRhiGraphicsPipeline* m_normalLinesPipeline = nullptr;
  QRhiShaderResourceBindings* m_normalLinesSrb = nullptr;

  // ── 坐标轴 / 网格管线 ─────────────────────────────────────
  QRhiGraphicsPipeline* m_axisPipeline = nullptr;
  QRhiShaderResourceBindings* m_axisSrb = nullptr;

  // ── 阴影 ──────────────────────────────────────────────────
  QRhiGraphicsPipeline* m_shadowPipeline = nullptr;
  QRhiShaderResourceBindings* m_shadowSrb = nullptr;
  QRhiGraphicsPipeline* m_shadowedPhongPipeline = nullptr;
  QRhiShaderResourceBindings* m_shadowedPhongSrb = nullptr;
  QRhiTexture* m_shadowTexture = nullptr;
  QRhiSampler* m_shadowSampler = nullptr;
  QRhiTextureRenderTarget* m_shadowRt = nullptr;
  QRhiRenderPassDescriptor* m_shadowRp = nullptr;

  // ── 天空盒 ────────────────────────────────────────────────
  QRhiGraphicsPipeline* m_skyboxPipeline = nullptr;
  QRhiShaderResourceBindings* m_skyboxSrb = nullptr;

  // ── 选择高亮 ──────────────────────────────────────────────
  QRhiGraphicsPipeline* m_selectionPipeline = nullptr;
  QRhiShaderResourceBindings* m_selectionSrb = nullptr;

  // ── 后处理（FXAA / AO）────────────────────────────────────
  bool m_fxaaEnabled = false;
  bool m_aoEnabled = false;
  float m_aoStrength = 0.6f;
  QRhiTexture* m_sceneColor = nullptr;
  QRhiTexture* m_sceneDepth = nullptr;
  QRhiTextureRenderTarget* m_sceneRt = nullptr;
  QRhiRenderPassDescriptor* m_sceneRp = nullptr;
  QRhiSampler* m_sceneColorSampler = nullptr;
  QRhiSampler* m_sceneDepthSampler = nullptr;
  QSize m_sceneSize;
  QRhiBuffer* m_postUBO = nullptr;
  QRhiBuffer* m_postVBuf = nullptr;
  QRhiGraphicsPipeline* m_postPipeline = nullptr;
  QRhiShaderResourceBindings* m_postSrb = nullptr;
  QRhiRenderPassDescriptor* m_pipelineRp = nullptr;  ///< 场景管线使用的渲染通道

  // ── 裁剪平面 ──────────────────────────────────────────────
  QRhiGraphicsPipeline* m_clippedPipeline = nullptr;
  QRhiShaderResourceBindings* m_clippedSrb = nullptr;

  // ── 剖面封盖 ──────────────────────────────────────────────
  QRhiGraphicsPipeline* m_capPipeline = nullptr;
  QRhiShaderResourceBindings* m_capSrb = nullptr;
  QRhiBuffer* m_capVBuf = nullptr;
  int m_capVertexCount = 0;
  bool m_capDirty = true;
  QVector<QVector3D> m_capVerts;

  // ── 3D 文字标注（轴标签）──────────────────────────────────
  QRhiTexture* m_labelTexture = nullptr;
  QRhiSampler* m_labelSampler = nullptr;
  QRhiGraphicsPipeline* m_labelPipeline = nullptr;
  QRhiShaderResourceBindings* m_labelSrb = nullptr;
  QRhiBuffer* m_labelVBuf = nullptr;
  int m_labelVertexCount = 0;

  // ── 状态 ──────────────────────────────────────────────────
  RenderMode m_renderMode = RenderMode::Phong;
  DisplayStyle m_displayStyle = DisplayStyle::Shaded;
  bool m_multiView = false;
  bool m_meshesUploaded = false;
  int m_selectedMesh = -1;

  // ── 剔除 / LOD / 批处理 / 缓冲池 ──────────────────────────
  BufferPool m_bufferPool;             ///< 网格 GPU 缓冲对象池
  bool m_cullEnabled = true;
  bool m_lodEnabled = true;
  Frustum m_frustum;                   ///< 当前视口视锥
  bool m_frustumValid = false;
  QVector3D m_slotEye[4];              ///< 各视口相机位置
  QMatrix4x4 m_slotViewProj[4];        ///< 各视口 view-projection
  float m_slotProjScale[4] = {0.4f, 0.4f, 0.4f, 0.4f};
  int m_currentSlot = 0;
  QVector<int> m_meshMaterialSlot;     ///< 每个对象复用的材质槽位
  RenderStats m_stats;

  QVector3D m_bgTopColor{0.2f, 0.2f, 0.3f};
  QVector3D m_bgBotColor{0.1f, 0.1f, 0.15f};
  float m_normalLength = 0.1f;
  QVector4D m_clipPlane{-1.0f, 0.0f, 0.0f, 0.0f};
  QVector3D m_sceneCenter{0.0f, 0.0f, 0.0f};
  float m_sceneRadius = 1.0f;
  QVector3D m_sceneMin{-1.0f, -1.0f, -1.0f};
};

#endif  // MODELVIEWER_SRC_RENDERER_RHIRENDERER_H_
