// Copyright 2026 ModelViewer Project
// SPDX-License-Identifier: Apache-2.0
//
// 模型选择页：最近打开（QSettings 持久化）+ 参数曲面演示 + 系统文件
// 选择器（Android SAF / 其他平台 QML FileDialog）。

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import ModelViewer

Page {
    id: page

    signal done()

    // 列表数据：静态演示曲面 + 动态最近打开
    property var entries: []

    function rebuildEntries() {
        const items = []
        const surfaces = [
                    { kind: "surface", name: qsTr("平面"), index: 0 },
                    { kind: "surface", name: qsTr("球体"), index: 1 },
                    { kind: "surface", name: qsTr("圆环"), index: 2 },
                    { kind: "surface", name: qsTr("圆柱"), index: 3 },
                    { kind: "surface", name: qsTr("圆锥"), index: 4 }
                ]
        for (const s of surfaces) {
            items.push(s)
        }
        const recents = FileHelper.recentModels()
        for (const path of recents) {
            const name = String(path).split("/").pop()
            items.push({ kind: "recent", name: name, path: path })
        }
        page.entries = items
    }

    Component.onCompleted: rebuildEntries()

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            ToolButton {
                text: qsTr("←")
                font.pixelSize: 18
                onClicked: page.done()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("模型")
                font.pixelSize: 17
                font.bold: true
            }

            // 最近打开清空
            ToolButton {
                text: qsTr("清空")
                visible: FileHelper.recentModels().length > 0
                onClicked: {
                    FileHelper.clearRecents()
                    page.rebuildEntries()
                }
            }

            // 系统文件选择器
            ToolButton {
                icon.source: "qrc:/icons/fileopen.png"
                icon.width: 24
                icon.height: 24
                onClicked: {
                    if (Qt.platform.os === "android") {
                        FileHelper.pickModel()
                    } else {
                        fileDialog.open()
                    }
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("从文件系统选择")
                ToolTip.delay: 600
            }
        }
    }

    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: page.entries

        ScrollBar.vertical: ScrollBar {}

        section.property: "kind"
        section.delegate: Rectangle {
            required property string section
            width: ListView.view.width
            height: 32
            color: "#1a1a24"

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                color: "#9aa0b0"
                font.pixelSize: 12
                font.bold: true
                text: section === "surface" ? qsTr("演示曲面（参数化生成）")
                                            : qsTr("最近打开")
            }
        }

        delegate: ItemDelegate {
            required property var modelData
            width: ListView.view.width
            height: 64

            contentItem: RowLayout {
                spacing: 12

                Image {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    fillMode: Image.PreserveAspectFit
                    source: modelData.kind === "surface"
                                ? "qrc:/icons/shaded.png"
                                : "qrc:/icons/section.png"
                }

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        font.pixelSize: 15
                        elide: Text.ElideMiddle
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: modelData.kind === "recent"
                        text: modelData.path ? modelData.path : ""
                        font.pixelSize: 10
                        color: "#808080"
                        elide: Text.ElideMiddle
                    }
                }
            }

            onClicked: {
                if (modelData.kind === "surface") {
                    ViewerBridge.createSurface(modelData.index)
                } else {
                    ViewerBridge.loadModel(modelData.path)
                }
                page.done()
            }
        }

        Label {
            anchors.centerIn: parent
            visible: page.entries.length === 0
            color: "#808080"
            text: qsTr("点击右上角图标打开一个模型文件")
        }
    }

    // ── 桌面调试兜底：QtQuick.Dialogs 文件选择 ─────────────────────
    FileDialog {
        id: fileDialog
        nameFilters: [
            qsTr("3D 模型 (*.obj *.stl *.fbx *.glb *.gltf *.ply *.3ds *.dae)"),
            qsTr("所有文件 (*)")
        ]
        onAccepted: ViewerBridge.loadModel(selectedFile.toString().replace("file:///", ""))
    }
}
