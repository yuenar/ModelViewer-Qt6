#include "Animation.h"

#include <QtMath>

QVector3D AnimationChannel::sampleVec3(const QVector<float>& times,
                                       const QVector<QVector3D>& values,
                                       float time,
                                       const QVector3D& fallback) const {
    if (times.isEmpty() || values.isEmpty() || times.size() != values.size()) {
        return fallback;
    }
    if (time <= times.first()) {
        return values.first();
    }
    if (time >= times.last()) {
        return values.last();
    }

    int index = 0;
    while (index + 1 < times.size() && times[index + 1] < time) {
        ++index;
    }
    if (index + 1 >= times.size()) {
        return values.last();
    }

    const float span = times[index + 1] - times[index];
    const float t = span > 1e-6f ? (time - times[index]) / span : 0.0f;
    return values[index] * (1.0f - t) + values[index + 1] * t;
}

QQuaternion AnimationChannel::sampleQuat(const QVector<float>& times,
                                         const QVector<QQuaternion>& values,
                                         float time) const {
    if (times.isEmpty() || values.isEmpty() || times.size() != values.size()) {
        return restRotation;
    }
    if (time <= times.first()) {
        return values.first();
    }
    if (time >= times.last()) {
        return values.last();
    }

    int index = 0;
    while (index + 1 < times.size() && times[index + 1] < time) {
        ++index;
    }
    if (index + 1 >= times.size()) {
        return values.last();
    }

    const float span = times[index + 1] - times[index];
    const float t = span > 1e-6f ? (time - times[index]) / span : 0.0f;
    return QQuaternion::slerp(values[index], values[index + 1], t);
}

QMatrix4x4 AnimationChannel::sample(float time) const {
    const QVector3D position =
        sampleVec3(positionTimes, positionValues, time, restPosition);
    const QVector3D scale =
        sampleVec3(scaleTimes, scaleValues, time, restScale);
    const QQuaternion rotation = sampleQuat(rotationTimes, rotationValues, time);

    QMatrix4x4 matrix;
    matrix.translate(position);
    matrix.rotate(rotation);
    matrix.scale(scale);
    return matrix;
}
