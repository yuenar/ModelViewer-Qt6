#include "BufferPool.h"

QRhiBuffer* BufferPool::acquire(QRhiBuffer::UsageFlag usage, quint32 size) {
    if (!m_rhi || size == 0) {
        return nullptr;
    }

    const quint32 bucket = bucketSize(size);
    auto& entries = m_free[makeKey(usage, bucket)];
    for (int i = 0; i < entries.size(); ++i) {
        const Entry& entry = entries[i];
        if (entry.capacity >= size && entry.releasedFrame + 2 <= m_frame) {
            QRhiBuffer* buffer = entry.buffer;
            entries.removeAt(i);
            return buffer;
        }
    }

    QRhiBuffer* buffer =
        m_rhi->newBuffer(QRhiBuffer::Dynamic, usage, bucket);
    if (!buffer->create()) {
        delete buffer;
        return nullptr;
    }
    return buffer;
}

void BufferPool::release(QRhiBuffer* buffer, QRhiBuffer::UsageFlag usage,
                         quint32 capacity) {
    if (!buffer) {
        return;
    }
    if (!m_rhi || capacity == 0) {
        delete buffer;
        return;
    }

    Entry entry;
    entry.buffer = buffer;
    entry.capacity = capacity;
    entry.releasedFrame = m_frame;
    m_free[makeKey(usage, bucketSize(capacity))].append(entry);
}

void BufferPool::clear() {
    for (auto it = m_free.begin(); it != m_free.end(); ++it) {
        for (const Entry& entry : it.value()) {
            delete entry.buffer;
        }
    }
    m_free.clear();
}

int BufferPool::cachedCount() const {
    int count = 0;
    for (auto it = m_free.begin(); it != m_free.end(); ++it) {
        count += it.value().size();
    }
    return count;
}
