/**
 * @file    GearDisplay.qml
 * @brief   Hiển thị trạng thái hộp số PRND
 *
 * Màu sắc theo tiêu chuẩn automotive:
 *   P → Trắng  (xe đứng yên, an toàn)
 *   R → Đỏ    (cảnh báo, lùi xe)
 *   N → Vàng  (trung tính, cẩn thận)
 *   D → Xanh  (sẵn sàng tiến)
 */

import QtQuick 2.15
import QtQuick.Effects

Item {
    id: root
    property string currentGear: "P"

    // ── Model dữ liệu PRND ──
    readonly property var gearData: ({
        "P": { color: "#E8E8F0", glow: "#AAAACC", label: "PARK"    },
        "R": { color: "#FF3030", glow: "#FF0000", label: "REVERSE"  },
        "N": { color: "#FFD700", glow: "#FFB000", label: "NEUTRAL"  },
        "D": { color: "#00E676", glow: "#00C853", label: "DRIVE"    }
    })

    Row {
        anchors.centerIn: parent
        spacing: root.width * 0.04

        Repeater {
            model: ["P", "R", "N", "D"]

            delegate: Item {
                id: gearItem
                property string gearLetter: modelData
                property bool   isActive:   (gearLetter === root.currentGear)
                property var    data:       root.gearData[gearLetter]

                width:  root.width  * 0.18
                height: root.height

                // ── Background glow effect khi active ──
                Rectangle {
                    id: glowRect
                    anchors.centerIn: parent
                    width:  parent.isActive ? parent.width  + 8 : parent.width
                    height: parent.isActive ? parent.height + 8 : parent.height
                    radius: 10
                    color:  "transparent"
                    border {
                        color: parent.isActive ? parent.data.glow : "transparent"
                        width: parent.isActive ? 2 : 0
                    }

                    Behavior on border.width { NumberAnimation { duration: 200 } }
                    Behavior on width        { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                }

                // ── Main tile ──
                Rectangle {
                    id: tile
                    anchors.fill: parent
                    radius: 8

                    // Màu nền: active = màu gear, inactive = tối
                    color: parent.isActive ? parent.data.color : "#0D1520"

                    // Viền
                    border {
                        color: parent.isActive ? parent.data.glow : "#1E3040"
                        width: 1.5
                    }

                    // Gradient overlay
                    gradient: parent.isActive ? activeGrad : inactiveGrad

                    Gradient {
                        id: activeGrad
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: Qt.lighter(parent.parent.data.color, 1.4) }
                        GradientStop { position: 1.0; color: parent.parent.data.color }
                    }
                    Gradient {
                        id: inactiveGrad
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: "#111B28" }
                        GradientStop { position: 1.0; color: "#0A1018" }
                    }

                    // Hiệu ứng transition mượt
                    Behavior on color {
                        ColorAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }

                    // ── Ký tự Gear to ──
                    Text {
                        id: gearText
                        anchors {
                            horizontalCenter: parent.horizontalCenter
                            top:             parent.top
                            topMargin:       parent.height * 0.12
                        }
                        text:  gearItem.gearLetter
                        color: gearItem.isActive ? "#0A0A0F" : "#2A4060"
                        font {
                            family:    "Courier New"
                            bold:      true
                            pixelSize: parent.height * 0.45
                        }

                        Behavior on color { ColorAnimation { duration: 250 } }
                    }

                    // ── Label nhỏ bên dưới ──
                    Text {
                        anchors {
                            horizontalCenter: parent.horizontalCenter
                            bottom:           parent.bottom
                            bottomMargin:     parent.height * 0.08
                        }
                        text:     gearItem.data.label
                        color:    gearItem.isActive ? "#1A1A2A" : "#1A2A3A"
                        font {
                            family:    "Courier New"
                            pixelSize: parent.height * 0.10
                            letterSpacing: 1
                        }

                        Behavior on color { ColorAnimation { duration: 250 } }
                    }

                    // ── Scale animation khi chuyển số ──
                    scale: gearItem.isActive ? 1.05 : 1.0
                    Behavior on scale {
                        SpringAnimation { spring: 5; damping: 0.5 }
                    }
                }
            }
        }
    }
}
