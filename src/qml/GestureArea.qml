// Copyright 2026 ModelViewer Project
// SPDX-License-Identifier: Apache-2.0
//
// 触控手势捕获覆盖层：
// - 单指拖动 → 旋转（发出绝对坐标 + 视口尺寸，Camera::rotate 需要）
// - 双指拖动 → 平移；双指捏合 → 缩放（PinchArea）
// - 双击 → 适配视图；长按 → 拾取
// - 桌面调试：鼠标拖动/滚轮同样生效（合成鼠标事件）
//
// 说明：采用 PinchArea（外层）+ MouseArea（内层）组合：MouseArea 没有
// pinch 属性组，捏合必须由 PinchArea 提供；PinchArea 通过 childMouseEventFilter
// 在两指时拦截触摸，单指仍由内层 MouseArea 处理，语义与桌面 RhiWidget
// 鼠标处理一一对应。

import QtQuick

Item {
    id: root

    // x/y 为覆盖层内坐标；vw/vh 为视口（RhiView）尺寸
    signal gestureBegin(real x, real y)
    signal rotated(real x, real y, real vw, real vh)
    signal panned(real dx, real dy)
    signal zoomed(real delta)
    signal doubleTapped()
    signal longPressed(real x, real y, real vw, real vh)

    // ── 双指捏合缩放 + 双指拖动平移 ─────────────────────────────
    PinchArea {
        id: pinchArea
        anchors.fill: parent
        pinch.minimumScale: 0.05
        pinch.maximumScale: 20.0

        onPinchUpdated: (pinch) => {
            // Camera::zoom 正值会拉远（缩小），捏合放大需传负增量；
            // 用比例增量，避免绝对差值随缩放级别放大导致手感忽快忽慢
            if (pinch.previousScale > 0.0001) {
                root.zoomed(1.0 - pinch.scale / pinch.previousScale)
            }
            root.panned(pinch.center.x - pinch.previousCenter.x,
                        pinch.center.y - pinch.previousCenter.y)
        }

        MouseArea {
            id: touchArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: false

            // ── 单指旋转（桌面: 左键拖动）─────────────────────────
            onPressed: (mouse) => {
                           root.gestureBegin(mouse.x, mouse.y)
                       }
            onPositionChanged: (mouse) => {
                                   if (pinchArea.pinch.active) {
                                       return
                                   }
                                   root.rotated(mouse.x, mouse.y,
                                                root.width, root.height)
                               }

            // ── 双击适配 / 长按拾取 ─────────────────────────────
            onDoubleClicked: root.doubleTapped()
            onPressAndHold: (mouse) => {
                                if (pinchArea.pinch.active) {
                                    return
                                }
                                root.longPressed(mouse.x, mouse.y,
                                                 root.width, root.height)
                            }

            // ── 桌面调试：滚轮缩放 ───────────────────────────────
            onWheel: (wheel) => root.zoomed(wheel.angleDelta.y / 120.0)
        }
    }
}
