# Math 模块

本模块提供 3D 图形学应用所需的基础数学工具和算法实现。

## 文件说明

### BoundingBox.h/cpp
包围盒计算和管理类。

**主要功能：**
- 轴对齐包围盒（AABB）的表示和操作
- 包围盒的合并、变换、相交检测
- 用于视锥剔除和碰撞检测

**核心方法：**
```cpp
class BoundingBox {
public:
    void reset();                           // 重置为空包围盒
    void extend(const QVector3D& point);    // 扩展包围盒以包含点
    void extend(const BoundingBox& other);   // 合并另一个包围盒
    bool contains(const QVector3D& point) const;  // 点是否在包围盒内
    bool intersects(const BoundingBox& other) const;  // 包围盒相交检测
    BoundingBox transformed(const QMatrix4x4& matrix) const;  // 变换包围盒
};
```

### Camera.h/cpp
3D 摄像机控制和视图矩阵计算。

**主要功能：**
- 透视投影和正交投影矩阵生成
- 摄像机位置、朝向、上方向控制
- 视图矩阵和投影矩阵计算
- 摄像机移动和旋转操作

**核心参数：**
```cpp
class Camera {
public:
    // 视图参数
    QVector3D position;      // 摄像机位置
    QVector3D target;        // 观察目标点
    QVector3D up;            // 上方向向量
    
    // 投影参数
    float fov;               // 视场角（度）
    float aspectRatio;       // 宽高比
    float nearPlane;         // 近裁剪面
    float farPlane;          // 远裁剪面
    
    // 矩阵计算
    QMatrix4x4 viewMatrix() const;      // 视图矩阵
    QMatrix4x4 projectionMatrix() const; // 投影矩阵
    QMatrix4x4 viewProjectionMatrix() const; // 视图投影矩阵
};
```

### TrackBall.h/cpp
轨迹球控制器，用于 3D 模型的旋转操作。

**主要功能：**
- 鼠标拖拽转换为 3D 旋转
- 平滑的旋转插值
- 弧球映射算法实现
- 支持旋转轴和角度的提取

**核心算法：**
```cpp
class TrackBall {
public:
    void push(const QPointF& point);     // 记录鼠标位置
    void move(const QPointF& point);     // 更新旋转
    void release(const QPointF& point);  // 释放鼠标
    QQuaternion rotation() const;         // 获取当前旋转四元数
    
private:
    QVector3D mapToSphere(const QPointF& point) const;  // 屏幕坐标映射到球面
};
```

## 数学算法

### 坐标系统
- **右手坐标系** - 符合 OpenGL 和数学惯例
- **行主序矩阵** - 与 Qt 和 DirectX 兼容
- **齐次坐标** - 支持 3D 变换和投影

### 变换矩阵
- **模型矩阵** - 物体的局部变换
- **视图矩阵** - 摄像机的世界变换
- **投影矩阵** - 3D 到 2D 的投影
- **法线矩阵** - 法线向量的正确变换

### 几何计算
- **向量运算** - 点积、叉积、长度、归一化
- **四元数** - 旋转表示和插值
- **平面方程** - 裁剪平面和碰撞检测
- **射线相交** - 拾取和射线追踪

## 性能优化

- **SIMD 指令** - 利用 Qt 的向量化运算
- **缓存友好** - 数据结构布局优化
- **延迟计算** - 矩阵按需计算和缓存
- **内联函数** - 小函数内联减少调用开销

## 使用示例

```cpp
// 摄像机设置
Camera camera;
camera.position = QVector3D(0, 0, 5);
camera.target = QVector3D(0, 0, 0);
camera.fov = 45.0f;
camera.aspectRatio = 16.0f / 9.0f;

// 获取视图投影矩阵
QMatrix4x4 vp = camera.viewProjectionMatrix();

// 包围盒计算
BoundingBox bbox;
for (const auto& vertex : vertices) {
    bbox.extend(vertex.position);
}

// 轨迹球旋转
TrackBall trackball;
trackball.push(mousePos);
trackball.move(newMousePos);
QQuaternion rotation = trackball.rotation();
```

## 依赖项

- **Qt Gui** - QVector3D、QMatrix4x4、QQuaternion 等数学类型
- **Qt Core** - 基础容器和算法

## 扩展性

模块设计支持：
- 添加新的几何体类型
- 实现高级数学算法
- 支持不同的坐标系统
- 添加性能分析工具
