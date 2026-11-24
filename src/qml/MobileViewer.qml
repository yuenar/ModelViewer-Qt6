// Copyright 2026 ModelViewer Project
// SPDX-License-Identifier: Apache-2.0
//
// 移动端主界面（Android）：RhiView 3D 视图 + 手势层 + 工具栏 +
// 渲染模式侧滑面板 + 底部统计 HUD + 模型列表页（StackView 第二页）。

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import ModelViewer

ApplicationWindow {
    id: root
    visible: true
    title: qsTr("ModelViewer")
    Material.theme: Material.Dark
    color: "#0e0e14"

    StackView {
        id: pageStack
        anchors.fill: parent
        initialItem: mainViewComponent
    }

    // ── 主视图（初始页，StackView push 时保留不销毁）──────────────
    Component {
        id: mainViewComponent

        Item {
            id: mainView

            ViewerToolBar {
                id: toolBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                title: ViewerBridge.modelLabel
                onOpenFileRequested: pageStack.push(modelListComponent)
                onRenderPanelRequested: renderPanel.open()
                onFitRequested: ViewerBridge.fitView()
                onProjectionToggled: (ortho) => ViewerBridge.setOrthographic(ortho)
                onScreenshotRequested: ViewerBridge.saveScreenshot()
            }

            // 3D 渲染区域（MobileRhiItem，渲染线程）
            RhiView {
                id: rhiView
                anchors.top: toolBar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: hudBar.top
                bridge: ViewerBridge
            }

            // 手势覆盖层（单指旋转 / 双指平移缩放 / 双击适配 / 长按拾取）
            GestureArea {
                id: gestureArea
                anchors.fill: rhiView
                onGestureBegin: (x, y) => ViewerBridge.beginRotateGesture(x, y)
                onRotated: (x, y, vw, vh) => ViewerBridge.rotateAt(x, y, vw, vh)
                onPanned: (dx, dy) => ViewerBridge.panGesture(dx, dy)
                onZoomed: (delta) => ViewerBridge.zoomGesture(delta)
                onDoubleTapped: ViewerBridge.fitView()
                onLongPressed: (x, y, vw, vh) =>
                                   ViewerBridge.pickObject(x, y, vw, vh)
            }

            // ── 底部统计 HUD ──────────────────────────────────────
            Rectangle {
                id: hudBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 30
                color: "#1a1a24"

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 11
                    color: "#9aa0b0"
                    elide: Text.ElideMiddle
                    text: ViewerBridge.statsText
                }
            }

            // ── 加载中（异步加载大模型）────────────────────────────
            BusyIndicator {
                anchors.centerIn: parent
                running: visible
                visible: ViewerBridge.loading
                width: 72
                height: 72
            }

            // ── 动画播放控制（有动画剪辑时显示）────────────────────
            RowLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: hudBar.top
                anchors.bottomMargin: 12
                spacing: 8
                visible: ViewerBridge.animationCount > 0

                RoundButton {
                    text: ViewerBridge.animationPlaying ? "❚❚" : "▶"
                    font.pixelSize: 16
                    onClicked: {
                        if (ViewerBridge.animationPlaying)
                            ViewerBridge.pauseAnimation()
                        else
                            ViewerBridge.playAnimation(0)
                    }
                }
                RoundButton {
                    text: "■"
                    font.pixelSize: 14
                    onClicked: ViewerBridge.stopAnimation()
                }
            }
        }
    }

    // ── 模型列表页（最近打开 + 演示曲面 + 系统文件选择器）──────────
    Component {
        id: modelListComponent

        ModelListPage {
            onDone: pageStack.pop()
        }
    }

    // ── 渲染模式 / 显示风格 / 显示选项侧滑面板 ─────────────────────
    RenderModePanel {
        id: renderPanel
        onRenderModeSelected: (mode) => ViewerBridge.setRenderMode(mode)
        onDisplayStyleSelected: (style) => ViewerBridge.setDisplayStyle(style)
    }

    // ── 提示横幅（错误 / 截图结果）────────────────────────────────
    Rectangle {
        id: banner
        property string message: ""
        visible: message !== ""
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 44
        color: "#c6222222"

        Label {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            verticalAlignment: Text.AlignVCenter
            color: "white"
            font.pixelSize: 13
            elide: Text.ElideMiddle
            text: banner.message
        }

        Timer {
            id: bannerTimer
            interval: 3200
            onTriggered: banner.message = ""
        }

        function show(msg) {
            message = msg
            bannerTimer.restart()
        }
    }

    // ── 文件选择结果（SAF 拷贝完成 → 加载）────────────────────────
    Connections {
        target: FileHelper
        function onFileSelected(path) {
            if (pageStack.depth > 1) {
                pageStack.pop()
            }
            ViewerBridge.loadModel(path)
        }
        function onFilePickCancelled() {
            // 用户取消，无需处理
        }
    }

    // ── 桥接信号反馈 ──────────────────────────────────────────────
    Connections {
        target: ViewerBridge
        function onLoadError(message) {
            banner.show(message)
        }
        function onScreenshotSaved(path) {
            banner.show(qsTr("截图已保存: ") + path)
        }
        function onScreenshotFailed(message) {
            banner.show(message)
        }
        function onSelectionChanged(index) {
            if (index >= 0) {
                banner.show(qsTr("已选中: ") + ViewerBridge.meshNames()[index])
            }
        }
    }

    // 首屏空场景时生成一个演示球，避免冷启动黑屏感
    Component.onCompleted: {
        if (ViewerBridge.meshCount === 0) {
            ViewerBridge.createSurface(1)
        }
    }
}
