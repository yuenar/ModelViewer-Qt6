// Copyright 2026 ModelViewer Project
// SPDX-License-Identifier: Apache-2.0
//
// 渲染模式 / 显示风格 / 显示选项侧滑面板（右侧 Drawer）。
// 枚举值与 src/renderer/RhiRenderer.h 的 RenderMode(0-11) 和
// DisplayStyle(0-3) 一一对应。

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Drawer {
    id: panel

    edge: Qt.RightEdge
    width: Math.min(parent.width * 0.78, 340)

    signal renderModeSelected(int mode)
    signal displayStyleSelected(int style)

    // 与 RhiRenderer::RenderMode 对齐（12 项）
    readonly property var renderModes: [
        { name: qsTr("Phong 着色"),    value: 0 },
        { name: qsTr("线框模式"),      value: 1 },
        { name: qsTr("法线着色"),      value: 2 },
        { name: qsTr("面法线"),        value: 3 },
        { name: qsTr("顶点法线"),      value: 4 },
        { name: qsTr("平面着色"),      value: 5 },
        { name: qsTr("PBR 渲染"),      value: 6 },
        { name: qsTr("阴影映射"),      value: 7 },
        { name: qsTr("天空盒"),        value: 8 },
        { name: qsTr("选择高亮"),      value: 9 },
        { name: qsTr("剖面裁剪"),      value: 10 },
        { name: qsTr("分屏对比"),      value: 11 }
    ]

    // 与 RhiRenderer::DisplayStyle 对齐（4 项）
    readonly property var displayStyles: [
        { name: qsTr("着色"),          value: 0 },
        { name: qsTr("线框"),          value: 1 },
        { name: qsTr("线框 + 着色"),   value: 2 },
        { name: qsTr("拟真"),          value: 3 }
    ]

    property int currentRenderMode: 0
    property int currentDisplayStyle: 0

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height
        clip: true

        ColumnLayout {
            id: content
            width: panel.width
            spacing: 4

            Label {
                Layout.topMargin: 12
                Layout.leftMargin: 16
                text: qsTr("渲染模式")
                font.pixelSize: 12
                font.bold: true
                color: "#9aa0b0"
            }

            Repeater {
                model: panel.renderModes

                ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    highlighted: modelData.value === panel.currentRenderMode

                    onClicked: {
                        panel.currentRenderMode = modelData.value
                        panel.renderModeSelected(modelData.value)
                    }
                }
            }

            Label {
                Layout.topMargin: 16
                Layout.leftMargin: 16
                text: qsTr("显示风格")
                font.pixelSize: 12
                font.bold: true
                color: "#9aa0b0"
            }

            Repeater {
                model: panel.displayStyles

                ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    highlighted: modelData.value === panel.currentDisplayStyle

                    onClicked: {
                        panel.currentDisplayStyle = modelData.value
                        panel.displayStyleSelected(modelData.value)
                    }
                }
            }

            Label {
                Layout.topMargin: 16
                Layout.leftMargin: 16
                text: qsTr("显示选项")
                font.pixelSize: 12
                font.bold: true
                color: "#9aa0b0"
            }

            SwitchDelegate {
                Layout.fillWidth: true
                text: qsTr("坐标轴")
                onToggled: ViewerBridge.setAxisVisible(checked)
            }

            SwitchDelegate {
                Layout.fillWidth: true
                text: qsTr("地面网格")
                onToggled: ViewerBridge.setGridVisible(checked)
            }

            SwitchDelegate {
                Layout.fillWidth: true
                text: qsTr("视锥剔除")
                checked: true
                onToggled: ViewerBridge.setCullingEnabled(checked)
            }

            SwitchDelegate {
                Layout.fillWidth: true
                text: qsTr("距离 LOD")
                checked: true
                onToggled: ViewerBridge.setLodEnabled(checked)
            }

            Label {
                Layout.topMargin: 16
                Layout.leftMargin: 16
                text: qsTr("法线长度（法线模式）")
                font.pixelSize: 12
                font.bold: true
                color: "#9aa0b0"
            }

            Slider {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                from: 0.02
                to: 0.6
                value: 0.15
                onMoved: ViewerBridge.setNormalLength(value)
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
            }
        }
    }
}
