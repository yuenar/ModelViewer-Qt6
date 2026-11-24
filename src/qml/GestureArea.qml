// Copyright 2026 ModelViewer Project
// SPDX-License-Identifier: Apache-2.0
//
// 触控手势捕获覆盖层：
// - 单指拖动 → 旋转（发出绝对坐标 + 视口尺寸，Camera::rotate 需要）
// - 双指拖动 → 平移；双指捏合 → 缩放（MouseArea pinch 组）
// - 双击 → 适配视图；长按 → 拾取
// - 桌面调试：鼠标拖动/滚轮同样生效（合成鼠标事件）
//
// 说明：采用 MouseArea + pinch 组合（全平台稳定、语义与桌面
// RhiWidget 鼠标处理一一对应），而非 DragHandler/PinchHandler。

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

    MouseArea {
        id: touchArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: false

        // ── 单指旋转（桌面: 左键拖动）─────────────────────────────
        onPressed: (mouse) => {
                       root.gestureBegin(mouse.x, mouse.y)
                   }
        onPositionChanged: (mouse) => {
                               root.rotated(mouse.x, mouse.y,
                                            root.width, root.height)
                           }

        // ── 双击适配 / 长按拾取 ─────────────────────────────────
        onDoubleClicked: root.doubleTapped()
        onPressAndHold: (mouse) => {
                            root.longPressed(mouse.x, mouse.y,
                                             root.width, root.height)
                        }

        // ── 桌面调试：滚轮缩放 ───────────────────────────────────
        onWheel: (wheel) => root.zoomed(wheel.angleDelta.y / 120.0)

        // ── 双指捏合缩放 + 双指拖动平移 ──────────────────────────
        pinch.accepted: true
        pinch.minimumScale: 0.05
        pinch.maximumScale: 20.0
        onPinchUpdated: (pinch) => {
                            root.zoomed(pinch.scale - pinch.previousScale)
                            root.panned(pinch.center.x - pinch.previousCenter.x,
                                        pinch.center.y - pinch.previousCenter.y)
                        }
    }
}
