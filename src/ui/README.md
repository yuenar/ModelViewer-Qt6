# UI 模块

本模块提供 ModelViewer 的用户界面，基于 Qt Widgets 框架实现，包含主窗口、渲染控件和各种设置对话框。

## 文件说明

### MainWindow.h/cpp
应用程序主窗口类。

**主要功能：**
- 菜单栏和工具栏管理
- 中央渲染窗口的布局
- 文件操作（打开、保存、导出）
- 视图模式切换
- 状态栏信息显示

**核心组件：**
```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
    
private:
    RhiWidget* m_renderWidget;      // 中央渲染控件
    QMenuBar* m_menuBar;            // 菜单栏
    QToolBar* m_toolBar;            // 工具栏
    QStatusBar* m_statusBar;        // 状态栏
    
    // 对话框
    LightDialog* m_lightDialog;     // 光照设置对话框
    MaterialDialog* m_materialDialog; // 材质编辑对话框
};
```

### RhiWidget.h/cpp
基于 Qt RHI 的渲染窗口控件。

**主要功能：**
- RHI 渲染上下文管理
- 鼠标和键盘事件处理
- 摄像机控制（轨道球）
- 渲染循环和帧率控制
- 窗口大小变化的响应

**事件处理：**
```cpp
class RhiWidget : public QRhiWidget {
    Q_OBJECT
    
protected:
    void mousePressEvent(QMouseEvent* event) override;      // 鼠标按下
    void mouseMoveEvent(QMouseEvent* event) override;       // 鼠标移动
    void mouseReleaseEvent(QMouseEvent* event) override;    // 鼠标释放
    void wheelEvent(QWheelEvent* event) override;           // 鼠标滚轮
    void keyPressEvent(QKeyEvent* event) override;          // 键盘输入
    
private:
    Camera m_camera;              // 摄像机
    TrackBall m_trackBall;        // 轨迹球控制器
    RhiRenderer* m_renderer;     // 渲染器实例
};
```

### LightDialog.h/cpp
光照设置对话框。

**主要功能：**
- 光源类型选择（点光源、方向光、聚光灯）
- 光源位置、颜色、强度设置
- 环境光、漫反射、镜面反射参数
- 实时预览光照效果

**控制参数：**
- 光源位置 (x, y, z)
- 光源颜色 (RGB)
- 光照强度
- 衰减系数
- 聚光灯角度和方向

### MaterialDialog.h/cpp
材质属性编辑对话框。

**主要功能：**
- 基础材质参数设置
- PBR 材质参数调整
- 纹理文件加载和预览
- 材质保存和加载

**材质参数：**
- 漫反射颜色
- 镜面反射颜色
- 自发光颜色
- 金属度 (Metallic)
- 粗糙度 (Roughness)
- 法线贴图强度

## 交互设计

### 鼠标控制
- **左键拖拽** - 旋转模型
- **右键拖拽** - 平移视图
- **滚轮** - 缩放视图
- **中键拖拽** - 快速平移

### 键盘快捷键
- **Space** - 重置视图
- **F** - 聚焦选中物体
- **W** - 切换线框模式
- **N** - 显示法线
- **L** - 切换光照
- **S** - 切换阴影

### 菜单结构
```
文件(F)
├── 打开模型(O)...
├── 保存场景(S)...
├── 导出(E)...
└── 退出(X)

视图(V)
├── 重置视图(R)
├── 聚焦(F)
├── 线框模式(W)
├── 显示法线(N)
└── 全屏(F11)

工具(T)
├── 光照设置(L)...
├── 材质编辑(M)...
└── 渲染设置(R)...

帮助(H)
├── 关于(A)...
└── 文档(D)...
```

## 界面布局

### 主窗口布局
```
┌─────────────────────────────────────┐
│ 菜单栏                               │
├─────────────────────────────────────┤
│ 工具栏                               │
├─────────────────────────────────────┤
│                                     │
│            渲染窗口                  │
│                                     │
│                                     │
├─────────────────────────────────────┤
│ 状态栏                               │
└─────────────────────────────────────┘
```

### 对话框设计
- **模态对话框** - 光照和材质设置
- **属性面板** - 参数分组和滑块控制
- **实时预览** - 参数变化即时反馈
- **预设管理** - 常用配置保存和加载

## 主题和样式

### 外观设置
- **深色主题** - 默认深色界面
- **浅色主题** - 可选浅色界面
- **自定义样式** - 支持 QSS 样式表
- **高 DPI 支持** - 4K 显示器适配

### 字体和图标
- **系统字体** - 跟随系统设置
- **矢量图标** - 高 DPI 缩放
- **工具提示** - 功能说明提示
- **状态指示** - 加载和错误状态

## 国际化支持

### 多语言
- **中文（简体）** - 默认语言
- **英文** - 可选语言
- **动态切换** - 运行时语言切换
- **翻译文件** - Qt .ts 格式

### 本地化
- **数字格式** - 地区相关的数字显示
- **日期时间** - 本地化日期格式
- **文件编码** - UTF-8 文件名支持

## 性能优化

### 渲染优化
- **垂直同步** - 防止画面撕裂
- **帧率限制** - 可配置帧率上限
- **后台渲染** - 空闲时降低渲染频率
- **LOD 切换** - 视角相关细节层次

### 界面响应
- **异步加载** - 大文件后台加载
- **进度显示** - 加载进度条和取消
- **延迟更新** - 参数调整防抖动
- **内存管理** - 及时释放资源

## 使用示例

```cpp
// 创建主窗口
MainWindow window;
window.show();

// 设置光照
LightDialog lightDialog;
lightDialog.setLightType(LightType::Point);
lightDialog.setPosition(QVector3D(1, 1, 1));
lightDialog.exec();

// 编辑材质
MaterialDialog materialDialog;
materialDialog.setMetallic(0.8);
materialDialog.setRoughness(0.2);
materialDialog.exec();
```

## 依赖项

- **Qt Widgets** - 用户界面框架
- **Qt Gui** - 图形和事件处理
- **Renderer 模块** - 渲染功能调用
- **Core 模块** - 数据结构访问

## 扩展性

模块设计支持：
- 自定义界面主题
- 插件式工具栏
- 自定义快捷键
- 脚本化界面操作
