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
        // Android 15 起强制 edge-to-edge，窗口会画到状态栏/手势条下面；
        // 用 SafeArea 让内容整体避开系统栏（Qt 已内缩时该值为 0，无副作用）。
        anchors.topMargin: SafeArea.margins.top
        anchors.bottomMargin: SafeArea.margins.bottom
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
                onMenuRequested: menuDrawer.open()
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

    // ── 左侧功能抽屉（工具栏汉堡按钮打开）───────────────────────────
    Drawer {
        id: menuDrawer

        edge: Qt.LeftEdge
        width: Math.min(root.width * 0.78, 320)

        // 打开任意一项后收起抽屉。
        // 注意：若动作是打开另一个 Drawer（如渲染面板），必须等本抽屉
        // 的关闭动画结束（onClosed）再执行，否则两个 Popup 同时切换会失败。
        property var pendingAction: null

        function closeAnd(action) {
            close()
            action()
        }

        function runAfterClose(action) {
            pendingAction = action
            close()
        }

        onClosed: {
            if (pendingAction) {
                const action = pendingAction
                pendingAction = null
                action()
            }
        }

        Column {
            anchors.fill: parent
            // 避让状态栏 / 底部手势条（Qt 已内缩时该值为 0）
            anchors.topMargin: SafeArea.margins.top
            anchors.bottomMargin: SafeArea.margins.bottom
            spacing: 0

            Label {
                width: parent.width
                height: 52
                leftPadding: 16
                text: qsTr("ModelViewer")
                font.pixelSize: 16
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#33ffffff"
            }

            ItemDelegate {
                width: parent.width
                text: qsTr("打开模型")
                icon.source: "qrc:/icons/fileopen.png"
                icon.width: 24
                icon.height: 24
                // 直接弹系统文件选择器（Android SAF 授权弹窗）
                onClicked: {
                    menuDrawer.close()
                    FileHelper.pickModel()
                }
            }

            ItemDelegate {
                width: parent.width
                text: qsTr("模型库")
                icon.source: "qrc:/icons/section.png"
                icon.width: 24
                icon.height: 24
                onClicked: menuDrawer.closeAnd(
                               () => pageStack.push(modelListComponent))
            }

            ItemDelegate {
                width: parent.width
                text: qsTr("渲染模式")
                // wireframe.png 是深灰线框，深色抽屉上几乎不可见，改用浅色图标
                icon.source: "qrc:/icons/shaded.png"
                icon.width: 24
                icon.height: 24
                // 渲染面板也是 Drawer：必须等本抽屉关闭动画结束再打开
                onClicked: menuDrawer.runAfterClose(() => renderPanel.open())
            }

            ItemDelegate {
                width: parent.width
                text: qsTr("适配视图")
                icon.source: "qrc:/icons/fit-all.png"
                icon.width: 24
                icon.height: 24
                onClicked: menuDrawer.closeAnd(() => ViewerBridge.fitView())
            }

            ItemDelegate {
                width: parent.width
                text: qsTr("正交投影")
                icon.source: orthoSwitch.checked ? "qrc:/icons/Ortho.png"
                                                 : "qrc:/icons/Perspective.png"
                icon.width: 24
                icon.height: 24
                onClicked: orthoSwitch.toggle()

                Switch {
                    id: orthoSwitch
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    onToggled: ViewerBridge.setOrthographic(checked)
                }
            }

            ItemDelegate {
                width: parent.width
                text: qsTr("截图")
                icon.source: "qrc:/icons/export.png"
                icon.width: 24
                icon.height: 24
                onClicked: menuDrawer.closeAnd(
                               () => ViewerBridge.saveScreenshot())
            }
        }
    }

    // ── 提示横幅（错误 / 截图结果）────────────────────────────────
    Rectangle {
        id: banner
        property string message: ""
        visible: message !== ""
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // 避让底部手势条
        anchors.bottomMargin: SafeArea.margins.bottom
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
