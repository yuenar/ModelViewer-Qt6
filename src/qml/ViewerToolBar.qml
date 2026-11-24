// Copyright 2026 ModelViewer Project
// SPDX-License-Identifier: Apache-2.0
//
// 顶部工具栏（移动端）：左侧汉堡按钮打开功能抽屉，中间显示当前模型标题。
// 具体操作项（打开/渲染模式/适配/投影/截图）统一放在
// MobileViewer.qml 的左侧 Drawer 中，工具栏本身保持精简。
// 标题允许被压缩（minimumWidth: 0 + 省略号），长文件名不会挤压按钮。

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ToolBar {
    id: bar

    property string title: qsTr("ModelViewer")

    signal menuRequested

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 0

        ToolButton {
            id: menuButton
            implicitWidth: 40
            implicitHeight: 40
            padding: 0
            onClicked: bar.menuRequested()

            // 三条横线自绘，避免依赖字体里的 ☰ 字形
            contentItem: Item {
                implicitWidth: 24
                implicitHeight: 24

                Column {
                    anchors.centerIn: parent
                    spacing: 5

                    Repeater {
                        model: 3

                        Rectangle {
                            width: 18
                            height: 2
                            radius: 1
                            color: Material.foreground
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: bar.title
            font.pixelSize: 15
            font.bold: true
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        // 右侧占位，与左侧按钮等宽，保证标题在视觉上居中
        Item {
            implicitWidth: menuButton.implicitWidth
            implicitHeight: 1
        }
    }
}
