// Copyright 2026 ModelViewer Project
// SPDX-License-Identifier: Apache-2.0
//
// 顶部工具栏：打开模型 / 渲染面板 / 适配视图 / 投影切换 / 截图。
// 图标复用 res/ 下的桌面 PNG（qrc:/icons/...，见 src/CMakeLists.txt）。

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ToolBar {
    id: bar

    property string title: qsTr("ModelViewer")

    signal openFileRequested
    signal renderPanelRequested
    signal fitRequested
    signal projectionToggled(bool ortho)
    signal screenshotRequested

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ToolButton {
            icon.source: "qrc:/icons/fileopen.png"
            icon.width: 24
            icon.height: 24
            onClicked: bar.openFileRequested()

            ToolTip.visible: hovered
            ToolTip.text: qsTr("打开模型")
            ToolTip.delay: 600
        }

        Label {
            Layout.fillWidth: true
            text: bar.title
            font.pixelSize: 15
            font.bold: true
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        ToolButton {
            icon.source: "qrc:/icons/wireframe.png"
            icon.width: 24
            icon.height: 24
            onClicked: bar.renderPanelRequested()

            ToolTip.visible: hovered
            ToolTip.text: qsTr("渲染模式")
            ToolTip.delay: 600
        }

        ToolButton {
            icon.source: "qrc:/icons/fit-all.png"
            icon.width: 24
            icon.height: 24
            onClicked: bar.fitRequested()

            ToolTip.visible: hovered
            ToolTip.text: qsTr("适配视图")
            ToolTip.delay: 600
        }

        ToolButton {
            id: projectionButton
            checkable: true
            icon.source: checked ? "qrc:/icons/Ortho.png"
                                 : "qrc:/icons/Perspective.png"
            icon.width: 24
            icon.height: 24
            onCheckedChanged: bar.projectionToggled(checked)

            ToolTip.visible: hovered
            ToolTip.text: checked ? qsTr("当前: 正交投影") : qsTr("当前: 透视投影")
            ToolTip.delay: 600
        }

        ToolButton {
            icon.source: "qrc:/icons/export.png"
            icon.width: 24
            icon.height: 24
            onClicked: bar.screenshotRequested()

            ToolTip.visible: hovered
            ToolTip.text: qsTr("截图")
            ToolTip.delay: 600
        }
    }
}
