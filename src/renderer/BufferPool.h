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

#ifndef MODELVIEWER_SRC_RENDERER_BUFFERPOOL_H_
#define MODELVIEWER_SRC_RENDERER_BUFFERPOOL_H_

#include <rhi/qrhi.h>
#include <QHash>
#include <QVector>

/// @brief GPU 缓冲对象池
///
/// 网格在对象变换/动画时会产生大量“旧缓冲释放 + 新缓冲创建”操作。
/// 该对象池按缓冲区类型与容量分桶缓存动态缓冲，复用前要求缓冲已释放
/// 至少 2 帧，避免与仍在 GPU 上执行的上一帧命令发生写入冲突。
class BufferPool {
 public:
  explicit BufferPool(QRhi* rhi = nullptr) : m_rhi(rhi) {}

  ~BufferPool() { clear(); }

  BufferPool(const BufferPool&) = delete;
  BufferPool& operator=(const BufferPool&) = delete;

  void setRhi(QRhi* rhi) {
    if (m_rhi != rhi) {
      clear();
      m_rhi = rhi;
    }
  }

  /// @brief 每帧开始时推进帧计数（用于延迟复用）
  void beginFrame() { ++m_frame; }

  /// @brief 获取一个容量不小于 size 的动态缓冲
  QRhiBuffer* acquire(QRhiBuffer::UsageFlag usage, quint32 size);

  /// @brief 归还缓冲（按当前帧打上时间戳，稍后复用）
  void release(QRhiBuffer* buffer, QRhiBuffer::UsageFlag usage,
               quint32 capacity);

  /// @brief 释放全部缓存缓冲
  void clear();

  int cachedCount() const;

 private:
  static quint32 bucketSize(quint32 size) {
    const quint32 granularity = 4096;
    return ((size + granularity - 1) / granularity) * granularity;
  }

  static quint64 makeKey(QRhiBuffer::UsageFlag usage, quint32 bucket) {
    return (quint64(usage) << 56) | quint64(bucket);
  }

  struct Entry {
    QRhiBuffer* buffer = nullptr;
    quint32 capacity = 0;
    quint64 releasedFrame = 0;
  };

  QRhi* m_rhi = nullptr;
  quint64 m_frame = 0;
  QHash<quint64, QVector<Entry>> m_free;
};

#endif  // MODELVIEWER_SRC_RENDERER_BUFFERPOOL_H_
