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
#include <QMatrix4x4>
#include <QSize>
#include <QString>
#include <QVector>
#include <QVector4D>

#include "../math/Camera.h"
#include "../core/Material.h"
#include "../core/Light.h"
#include "../core/CpuMesh.h"

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
  };

  QRhiGraphicsPipeline* createPipeline(const PipelineDesc& desc);
  QShader loadShader(const QString& qsbPath);

  void createBuffers(QRhiCommandBuffer* cb);
  void destroyBuffers();
  void createShadowResources();
  void destroyShadowResources();
  void createSkyboxGeometry(QRhiCommandBuffer* cb);
  void destroySkyboxGeometry();

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
                         QRhiShaderResourceBindings* srb, int slot);
  void drawWireframe(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                     QRhiShaderResourceBindings* srb, int slot);
  void drawNormalLines(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                       bool faceNormals, QRhiShaderResourceBindings* srb, int slot);
  void drawSurface(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes, int slot);
  void drawWireOverlay(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes, int slot);
  void drawMultiView(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes,
                     float width, float height);
  void updateSceneBounds(const QVector<RhiMesh*>& meshes);

  QRhiCommandBuffer::DynamicOffset frameOffset(int slot) const {
    return QRhiCommandBuffer::DynamicOffset(
        0, quint32(slot) * quint32(m_frameStride));
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

  // ── 裁剪平面 ──────────────────────────────────────────────
  QRhiGraphicsPipeline* m_clippedPipeline = nullptr;
  QRhiShaderResourceBindings* m_clippedSrb = nullptr;

  // ── 状态 ──────────────────────────────────────────────────
  RenderMode m_renderMode = RenderMode::Phong;
  DisplayStyle m_displayStyle = DisplayStyle::Shaded;
  bool m_multiView = false;
  bool m_meshesUploaded = false;
  int m_selectedMesh = -1;

  QVector3D m_bgTopColor{0.2f, 0.2f, 0.3f};
  QVector3D m_bgBotColor{0.1f, 0.1f, 0.15f};
  float m_normalLength = 0.1f;
  QVector4D m_clipPlane{-1.0f, 0.0f, 0.0f, 0.0f};
  QVector3D m_sceneCenter{0.0f, 0.0f, 0.0f};
  float m_sceneRadius = 1.0f;
  QVector3D m_sceneMin{-1.0f, -1.0f, -1.0f};
};

#endif  // MODELVIEWER_SRC_RENDERER_RHIRENDERER_H_
